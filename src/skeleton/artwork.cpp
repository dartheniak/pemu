//
// Artwork and game information downloader (see artwork.h)
//

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <unistd.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include "pemu.h"
#include "artwork.h"
#include "pemu_log.h"
#include "ps4_dns.h"

using namespace pemu;

#define ARTWORK_USER_AGENT "pppEmu (pEMU fork)"
#define ARTWORK_CONNECT_TIMEOUT 8
#define ARTWORK_TIMEOUT 60
#define ARTWORK_MAX_REDIRECTS 5
// download thread stack: libcurl + openssl need far more than the small default
// stack of secondary threads on the ps4 (stack overflow = instant CE-34878-0 crash)
#define ARTWORK_THREAD_STACK (4 * 1024 * 1024)
// game information database refreshed after this many days
#define ARTWORK_DB_MAX_AGE_DAYS 30
// background downloads paused after this many network errors in a row...
#define ARTWORK_MAX_FAILURES 3
// ...for this long (ms)
#define ARTWORK_PAUSE_MS (2 * 60 * 1000)

static const char *thumbnailFolders[] = {"Named_Boxarts", "Named_Titles", "Named_Snaps"};
static const char *localFolders[] = {"boxart", "title", "snap"};
static const char *databaseFields[] = {"developer", "publisher", "genre", "releaseyear", "maxusers"};

static std::string urlEncode(const std::string &s) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c: s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char) c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

static bool fileExists(const std::string &path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

static bool makeDirs(const std::string &path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); i++) {
        cur += path[i];
        if ((path[i] == '/' || i == path.size() - 1) && cur.size() > 1) {
            if (mkdir(cur.c_str(), 0777) != 0 && errno != EEXIST) return false;
        }
    }
    return true;
}

static std::string lowerCase(std::string s) {
    for (auto &c: s) c = (char) tolower((unsigned char) c);
    return s;
}

Artwork::Artwork(c2d::Io *io, const std::string &system, int systemId, const std::string &mediaPath) {
    m_io = io;
    m_system = system;
    m_systemId = systemId;
    m_mediaPath = mediaPath;

    // certificates for https downloads, shipped with the app (the ps4 has no usable ca store)
    const std::string ca = io->getRomFsPath() + "cacert.pem";
    if (fileExists(ca)) m_caFile = ca;
    m_mutex = new C2DMutex();
    logLine("Artwork: ready (system \"%s\", certificates: %s)", system.c_str(),
            m_caFile.empty() ? "none" : m_caFile.c_str());
    // nothing network related happens before the first download (see start())
}

void Artwork::start() {
    if (m_started) return;
    m_started = true;

    logLine("Artwork: network init");
    const CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    logLine("Artwork: network init done (%i)", (int) res);
}

void Artwork::startWorker() {
    start();
    if (m_workerStarted) return;
    m_workerStarted = true;

    logLine("Artwork: starting download thread");
    m_sdlThread = SDL_CreateThreadWithStackSize(workerMain, "artwork", ARTWORK_THREAD_STACK, this);
    logLine("Artwork: download thread %s", m_sdlThread ? "started" : SDL_GetError());
}

Artwork::~Artwork() {
    m_quit = true;
    if (m_sdlThread) {
        SDL_WaitThread((SDL_Thread *) m_sdlThread, nullptr);
    }
    delete (m_mutex);
}

std::string Artwork::mediaPath() {
    return m_mediaPath;
}

std::string Artwork::gameName(const ss_api::Game &game) {
    std::string name = game.path.substr(game.path.find_last_of('/') + 1);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) name = name.substr(0, dot);
    return name;
}

std::string Artwork::thumbnailName(const std::string &gameName) {
    std::string name = gameName;
    for (auto &c: name) {
        if (strchr("&*/:`<>?\\|\"", c)) c = '_';
    }
    return name;
}

std::string Artwork::imagePath(const ss_api::Game &game, Type type) {
    return mediaPath() + localFolders[(int) type] + "/" + thumbnailName(gameName(game)) + ".png";
}

std::string Artwork::findImage(const ss_api::Game &game, Type type) {
    const std::string path = imagePath(game, type);
    return fileExists(path) ? path : "";
}

bool Artwork::isMissing(const ss_api::Game &game, Type type) {
    return fileExists(imagePath(game, type) + ".missing");
}

// download "url" to "dstPath": returns the http code (0 = network error)
int Artwork::httpGet(const std::string &url, const std::string &dstPath, std::string *error) {
    start();
    const std::string part = dstPath + ".part";
    std::string current = url;
    long code = 0;

    // redirects are followed here rather than by curl, so that every host
    // goes through our own name lookup on the ps4 (see ps4_dns.h)
    for (int hop = 0; hop <= ARTWORK_MAX_REDIRECTS; hop++) {
        logLine("Artwork: get %s", current.c_str());

        std::string host, ip, dnsError;
        int port = 80;
        if (!urlHost(current, &host, &port)) {
            if (error) *error = "bad url " + current;
            return 0;
        }
#ifdef __PS4__
        if (!resolveHost(host, &ip, &dnsError)) {
            if (error) *error = dnsError;
            return 0;
        }
#endif

        CURL *curl = curl_easy_init();
        if (!curl) {
            if (error) *error = "curl init failed";
            return 0;
        }
        FILE *fp = fopen(part.c_str(), "wb");
        if (!fp) {
            if (error) *error = "could not write " + part;
            curl_easy_cleanup(curl);
            return 0;
        }

        struct curl_slist *resolve = nullptr;
        if (!ip.empty()) {
            const std::string entry = host + ":" + std::to_string(port) + ":" + ip;
            resolve = curl_slist_append(resolve, entry.c_str());
        }

        char errbuf[CURL_ERROR_SIZE] = {0};
        curl_easy_setopt(curl, CURLOPT_URL, current.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullptr); // default: fwrite to WRITEDATA
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, ARTWORK_USER_AGENT);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, (long) ARTWORK_CONNECT_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long) ARTWORK_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, (long) CURL_IPRESOLVE_V4);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
        if (resolve) curl_easy_setopt(curl, CURLOPT_RESOLVE, resolve);
        if (!m_caFile.empty()) curl_easy_setopt(curl, CURLOPT_CAINFO, m_caFile.c_str());

        logLine("Artwork: curl perform (%s)", ip.empty() ? host.c_str() : ip.c_str());
        const CURLcode res = curl_easy_perform(curl);
        code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        std::string location;
        if (res == CURLE_OK && code >= 300 && code < 400) {
            char *loc = nullptr;
            curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &loc);
            if (loc) location = loc;
        }
        logLine("Artwork: http %li, curl %i %s", code, (int) res, errbuf);
        curl_easy_cleanup(curl);
        curl_slist_free_all(resolve);
        fclose(fp);

        if (res != CURLE_OK) {
            if (error) *error = errbuf[0] ? errbuf : curl_easy_strerror(res);
            remove(part.c_str());
            return 0;
        }
        if (!location.empty()) {
            current = location;
            continue;
        }
        break;
    }

    if (code != 200) {
        if (error) *error = "http error " + std::to_string(code);
        remove(part.c_str());
        return (int) code;
    }

    remove(dstPath.c_str());
    if (rename(part.c_str(), dstPath.c_str()) != 0) {
        if (error) *error = "could not rename " + part;
        remove(part.c_str());
        return 0;
    }
    return 200;
}

static bool downloadImageByName(Artwork *self, const std::string &mediaPath, const std::string &system,
                                const std::string &name, Artwork::Type type,
                                const std::function<int(const std::string &, const std::string &, std::string *)> &get,
                                std::string *error) {
    const std::string folder = mediaPath + localFolders[(int) type] + "/";
    const std::string file = Artwork::thumbnailName(name) + ".png";
    if (!makeDirs(folder)) {
        if (error) *error = "could not create " + folder;
        return false;
    }

    // official thumbnails server first, then the github mirror
    std::string repo = system;
    for (auto &c: repo) if (c == ' ') c = '_';
    const std::string urls[] = {
            "http://thumbnails.libretro.com/" + urlEncode(system) + "/" + thumbnailFolders[(int) type] + "/" +
            urlEncode(file),
            "https://raw.githubusercontent.com/libretro-thumbnails/" + urlEncode(repo) + "/master/" +
            thumbnailFolders[(int) type] + "/" + urlEncode(file),
    };

    int notFound = 0;
    for (const auto &url: urls) {
        const int code = get(url, folder + file, error);
        if (code == 200) {
            printf("Artwork: downloaded %s\n", url.c_str());
            return true;
        }
        printf("Artwork: %s: %s\n", url.c_str(), error ? error->c_str() : "");
        if (code == 404) notFound++;
    }

    // a server answered "not found" (and none had it): remember it, so it's not downloaded again and again
    if (notFound > 0) {
        if (FILE *f = fopen((folder + file + ".missing").c_str(), "w")) fclose(f);
        if (error) *error = "no image available";
    }
    (void) self;
    return false;
}

bool Artwork::downloadImage(const ss_api::Game &game, Type type, std::string *error) {
    if (findImage(game, type).size()) return true;
    return downloadImageByName(this, mediaPath(), m_system, gameName(game), type,
                               [this](const std::string &u, const std::string &d, std::string *e) {
                                   return httpGet(u, d, e);
                               }, error);
}

bool Artwork::downloadDatabase(const ProgressCb &progress, std::string *error) {
    const std::string folder = mediaPath() + "db/";
    if (!makeDirs(folder)) {
        if (error) *error = "could not create " + folder;
        return false;
    }

    const int count = (int) (sizeof(databaseFields) / sizeof(databaseFields[0]));
    for (int i = 0; i < count; i++) {
        if (progress && !progress(i, count, databaseFields[i])) {
            if (error) *error = "cancelled";
            return false;
        }
        const std::string url = "https://raw.githubusercontent.com/libretro/libretro-database/master/metadat/" +
                                std::string(databaseFields[i]) + "/" + urlEncode(m_system) + ".dat";
        const std::string path = folder + databaseFields[i] + ".dat";
        const int code = httpGet(url, path, error);
        if (code == 404) {
            // libretro has no such file for this console (Sega CD...): nothing to show,
            // an empty file says it was checked (not downloaded again before ARTWORK_DB_MAX_AGE_DAYS)
            logLine("Artwork: no %s information for %s", databaseFields[i], m_system.c_str());
            if (FILE *f = fopen(path.c_str(), "w")) fclose(f);
            if (error) error->clear();
            continue;
        }
        if (code != 200) {
            printf("Artwork: database %s: %s\n", url.c_str(), error ? error->c_str() : "");
            return false;
        }
    }
    if (progress) progress(count, count, "");
    return true;
}

// read the value after a key: genre "Platform" -> Platform, users 2 -> 2
static std::string datValue(const std::string &line, size_t start) {
    size_t a = line.find('"', start);
    if (a != std::string::npos) {
        size_t b = line.find('"', a + 1);
        return line.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
    }
    size_t s = line.find_first_not_of(" \t", start);
    if (s == std::string::npos) return "";
    size_t e = line.find_first_of(" \t\r\n", s);
    return line.substr(s, e == std::string::npos ? std::string::npos : e - s);
}

bool Artwork::loadDatabase() {
    m_db.clear();
    bool any = false;
    for (const auto *field: databaseFields) {
        FILE *fp = fopen((mediaPath() + "db/" + field + ".dat").c_str(), "r");
        if (!fp) continue;
        any = true;

        char buf[1024];
        std::string current;
        while (fgets(buf, sizeof(buf), fp)) {
            std::string line = buf;
            const size_t k = line.find_first_not_of(" \t");
            if (k == std::string::npos) continue;
            line = line.substr(k);
            // the game name is in "comment" (or "name" in some files)
            if (line.rfind("comment ", 0) == 0 || line.rfind("name ", 0) == 0) {
                current = lowerCase(datValue(line, line.find(' ')));
                continue;
            }
            if (current.empty()) continue;
            const size_t sp = line.find(' ');
            if (sp == std::string::npos) continue;
            const std::string key = line.substr(0, sp);
            const std::string value = datValue(line, sp);
            Info &info = m_db[current];
            if (key == "developer") info.developer = value;
            else if (key == "publisher") info.publisher = value;
            else if (key == "genre") info.genre = value;
            else if (key == "releaseyear") info.year = value;
            else if (key == "users") info.players = atoi(value.c_str());
        }
        fclose(fp);
    }
    printf("Artwork: database loaded, %zu games\n", m_db.size());
    return any;
}

static int stableId(const std::string &name) {
    return (int) (std::hash<std::string>()(name) & 0x3FFFFFFF) + 1;
}

int Artwork::applyDatabase(ss_api::GameList *list) {
    if (!list || m_db.empty()) return 0;

    int updated = 0;
    for (auto &game: list->games) {
        if (game.system.id != m_systemId) continue;
        auto it = m_db.find(lowerCase(gameName(game)));
        if (it == m_db.end()) continue;
        const Info &info = it->second;
        bool changed = false;

        if (!info.developer.empty() && (game.developer.id == 0 || game.developer.name == "UNKNOWN")) {
            game.developer = {stableId(info.developer), info.developer};
            if (list->findDeveloperByName(info.developer).name != info.developer) {
                list->developers.emplace_back(game.developer);
            }
            changed = true;
        }
        if (!info.publisher.empty() && (game.editor.id == 0 || game.editor.name == "UNKNOWN")) {
            game.editor = {stableId(info.publisher), info.publisher};
            if (list->findEditorByName(info.publisher).name != info.publisher) {
                list->editors.emplace_back(game.editor);
            }
            changed = true;
        }
        if (!info.genre.empty() && (game.genre.id == 0 || game.genre.name == "UNKNOWN")) {
            game.genre = {stableId(info.genre), info.genre};
            if (list->findGenreByName(info.genre).name != info.genre) {
                list->genres.emplace_back(game.genre);
            }
            changed = true;
        }
        if (!info.year.empty() && game.date.empty()) {
            game.date = info.year;
            if (std::find(list->dates.begin(), list->dates.end(), info.year) == list->dates.end()) {
                list->dates.emplace_back(info.year);
            }
            changed = true;
        }
        if (info.players > 0 && game.playersInt == 0) {
            game.playersInt = info.players;
            game.players = info.players == 1 ? "1" : "1-" + std::to_string(info.players);
            if (std::find(list->players.begin(), list->players.end(), info.players) == list->players.end()) {
                list->players.emplace_back(info.players);
            }
            changed = true;
        }
        if (changed) updated++;
    }

    printf("Artwork: game information applied to %i games\n", updated);
    return updated;
}

void Artwork::request(const ss_api::Game &game, const std::vector<Type> &types) {
    if (types.empty()) return;
    startWorker();
    m_mutex->lock();
    m_requests.clear();
    for (const auto t: types) m_requests.emplace_back(gameName(game), t);
    m_mutex->unlock();
}

bool Artwork::needsDatabase() {
    const time_t now = time(nullptr);
    for (const auto *field: databaseFields) {
        struct stat st{};
        if (stat((mediaPath() + "db/" + field + ".dat").c_str(), &st) != 0) return true;
        if (now > 0 && st.st_mtime > 0 && now - st.st_mtime > ARTWORK_DB_MAX_AGE_DAYS * 24 * 3600) return true;
    }
    return false;
}

void Artwork::requestDatabase() {
    startWorker();
    m_mutex->lock();
    m_dbRequested = true;
    m_mutex->unlock();
}

bool Artwork::takeDatabaseReady() {
    m_mutex->lock();
    const bool ready = m_dbReady;
    m_dbReady = false;
    m_mutex->unlock();
    return ready;
}

void Artwork::setNotice(const std::string &msg) {
    logLine("Artwork: notice: %s", msg.c_str());
    m_mutex->lock();
    m_notice = msg;
    m_mutex->unlock();
}

std::string Artwork::takeNotice() {
    m_mutex->lock();
    std::string notice = m_notice;
    m_notice.clear();
    m_mutex->unlock();
    return notice;
}

std::string Artwork::takeDownloaded() {
    std::string name;
    m_mutex->lock();
    if (!m_downloaded.empty()) {
        name = m_downloaded.back();
        m_downloaded.clear();
    }
    m_mutex->unlock();
    return name;
}

int Artwork::workerMain(void *data) {
    auto self = (Artwork *) data;
    logLine("Artwork: download thread running");
    while (!self->m_quit) {
        std::string name;
        Type type = Type::Boxart;
        bool db = false;
        if (self->m_pauseUntil && SDL_TICKS_PASSED(SDL_GetTicks(), self->m_pauseUntil)) {
            self->m_pauseUntil = 0;
        }
        const bool paused = self->m_pauseUntil != 0;
        self->m_mutex->lock();
        if (paused) {
            // no internet: what's asked meanwhile is dropped (asked again when the game is selected)
            self->m_requests.clear();
        } else if (self->m_dbRequested) {
            db = true;
            self->m_dbRequested = false;
        } else if (!self->m_requests.empty()) {
            name = self->m_requests.front().first;
            type = self->m_requests.front().second;
            self->m_requests.erase(self->m_requests.begin());
        }
        self->m_mutex->unlock();

        if (db) {
            std::string error;
            logLine("Artwork: downloading game information in background");
            if (self->downloadDatabase(nullptr, &error)) {
                self->m_mutex->lock();
                self->m_dbReady = true;
                self->m_mutex->unlock();
                self->m_failuresInRow = 0;
            } else {
                logLine("Artwork: game information: %s", error.c_str());
                self->setNotice("GAME INFORMATION DOWNLOAD FAILED, CHECK THE INTERNET CONNECTION");
            }
            continue;
        }

        if (name.empty()) {
            usleep(50 * 1000);
            continue;
        }

        const std::string path = self->mediaPath() + localFolders[(int) type] + "/" + thumbnailName(name) + ".png";
        if (fileExists(path) || fileExists(path + ".missing")) continue;

        std::string error;
        if (downloadImageByName(self, self->mediaPath(), self->m_system, name, type,
                                [self](const std::string &u, const std::string &d, std::string *e) {
                                    return self->httpGet(u, d, e);
                                }, &error)) {
            self->m_failuresInRow = 0;
            self->m_mutex->lock();
            self->m_downloaded.push_back(name);
            self->m_mutex->unlock();
        } else if (fileExists(path + ".missing")) {
            self->m_failuresInRow = 0; // the server answered: no image for this game
        } else if (++self->m_failuresInRow >= ARTWORK_MAX_FAILURES) {
            self->m_failuresInRow = 0;
            self->m_pauseUntil = SDL_GetTicks() + ARTWORK_PAUSE_MS;
            if (!self->m_pauseUntil) self->m_pauseUntil = 1;
            logLine("Artwork: paused after errors: %s", error.c_str());
            self->setNotice("ARTWORK: NO INTERNET CONNECTION, RETRYING IN 2 MINUTES");
        }
    }
    return 0;
}
