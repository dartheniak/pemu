//
// Created by cpasjuste on 19/10/16.
//

#include <vector>
#include <string>
#include <algorithm>
#include <functional>
#include "pemu.h"
#include "folder_names.h"
#include "pemu_log.h"

RomList::RomList(UiMain *_ui, const std::string &emuVersion, const std::vector<std::string> &_filters) {
    printf("RomList()\n");
    ui = _ui;
    filters = _filters;

    // UI
    rect = new C2DRectangle(
            Vector2f(ui->getSize().x - 8, ui->getSize().y - 8));
    ui->getSkin()->loadRectangleShape(rect, {"MAIN"});

    auto *title = new RectangleShape({16, 16});
    if (ui->getSkin()->loadRectangleShape(title, {"MAIN", "TITLE"})) {
        title->setOrigin(Origin::Center);
        title->setPosition(Vector2f(rect->getSize().x / 2, rect->getSize().y / 2));
        float scaling = std::min(
                (rect->getSize().x * 0.8f) / title->getSize().x,
                (rect->getSize().y * 0.8f) / title->getSize().y);
        title->setScale(scaling, scaling);
        rect->add(title);
    } else {
        delete (title);
    }

    text = new Text();
    ui->getSkin()->loadText(text, {"ROM_LIST", "TEXT"});
    text->setOrigin(Origin::BottomLeft);
    text->setPosition(8, rect->getSize().y - 8);
    rect->add(text);

    auto *version = new Text();
    ui->getSkin()->loadText(version, {"ROM_LIST", "TEXT"});
    version->setOrigin(Origin::BottomRight);
    version->setPosition(rect->getSize().x - 8, rect->getSize().y - 8);
    version->setString(emuVersion);
    rect->add(version);

    ui->add(rect);
    //ui->flip();
    // UI

    gameList = new GameList();
    gameListFav = new GameList();

    // debug log (<data>/pppemu.log), see pemu_log.h
    logInit(ui->getIo()->getDataPath());
    logLine("RomList: start");

    // artwork and game information downloads (consoles with a libretro name).
    // Several consoles: each one has its own media folder ("media/Sega - Mega Drive - Genesis/")
    const auto systems = ui->getConfig()->getSystems();
    for (const auto &sys: systems) {
        if (sys.libretro.empty()) continue;
        const std::string media = ui->getIo()->getDataPath() + "media/" +
                                  (systems.size() > 1 ? sys.libretro + "/" : "");
        artworks.push_back(new Artwork(ui->getIo(), sys.libretro, sys.system.id, media));
    }
    artwork = artworks.empty() ? nullptr : artworks.front();
}

Artwork *RomList::getArtwork(const Game &game) {
    for (auto a: artworks) {
        if (a->getSystemId() == game.system.id) return a;
    }
    return nullptr;
}

void RomList::build(const ss_api::GameList::GameAddedCb &cb) {
    float time_start = ui->getElapsedTime().asSeconds();
    auto cfg = ui->getConfig();
    p_cb = cb;
    m_games_count = m_games_available_count = 0;

    ui->flip();

    auto romPaths = cfg->getRomPaths();
    logLine("RomList::build: %zu rom paths", romPaths.size());
    const bool useFolderRules = !cfg->getCoreFolderNames().empty();
    m_scanned.clear();
    warnings.clear();

#ifdef __SMB__
    // network games downloaded in a previous session
    auto clearCache = cfg->get(PEMUConfig::OptId::NET_SMB_CLEAR_CACHE);
    if (clearCache && clearCache->getInteger()) {
        ui->getIo()->removeDir(getSmbCachePath());
    }

    // network rom path from the NETWORK settings: only the consoles' folders inside it
    const auto systems = cfg->getSystems();
    const std::string netPath = cfg->getNetworkRomPath();
    if (!netPath.empty()) {
        const std::string display = SmbIo::toDisplayUrl(netPath);
        setLoadingText("Connecting to %s...", display.c_str());
        bool anyFound = false;
        const size_t gamesBefore = gameList->games.size();
        const size_t warningsBefore = warnings.size();
        for (size_t i = 0; i < systems.size(); i++) {
            const auto &sys = systems[i];
            SmbIo::SystemFolders folders;
            std::string error;
            if (!SmbIo::findSystemFolders(netPath, sys.folderNames, &folders, &error)) {
                // unreachable: appendSmb shows the error (and cached games if any)
                appendSmb(netPath, systems[0].system, SMB_MAX_FOLDER_DEPTH, systems[0].extensions);
                anyFound = true;
                break;
            }
            if (folders.found || sys.folderNames.empty()) {
                anyFound = true;
            } else if (systems.size() > 1 || i > 0) {
                continue;   // several consoles: loose files can't be told apart, only folders are used
            } else {
                setLoadingText("Network: no %s folder found in %s, using its files only",
                               sys.system.name.c_str(), display.c_str());
                warnings.push_back("Network: no " + sys.system.name + " folder found in " + display +
                                   ", only the games directly in it are listed.");
                ui->delay(3000);
            }
            for (const auto &url: folders.urls) {
                appendSmb(url, sys.system, folders.scanSubFolders ? SMB_MAX_FOLDER_DEPTH : 0, sys.extensions);
            }
        }
        if (anyFound && warnings.size() == warningsBefore && gameList->games.size() == gamesBefore) {
            warnings.push_back("Network: connected, but no games found in " + display + ".");
        }
        if (!anyFound) {
            std::string names;
            for (const auto &sys: systems) {
                const std::string n = sys.folderNames.empty() ? sys.system.name : sys.folderNames.back();
                names += (names.empty() ? "" : ", ") + n;
            }
            warnings.push_back("Network: no console folder (" + names + ") found in " + display + ".");
        }
    }
#endif

    for (const auto &p: romPaths) {
        if (Utility::startWith(Utility::toLower(p.path), "smb://")) {
#ifdef __SMB__
            appendSmb(p.path, p.system, SMB_MAX_FOLDER_DEPTH, filters);
#else
            printf("RomList::build: skipping %s (built without smb support)\n", p.path.c_str());
#endif
            continue;
        }

        // cores with known folder names: same rules as the network (console folder found by name,
        // sub folders scanned). Other cores (pfbneo...) keep pemu's original behaviour.
        if (useFolderRules) {
            for (size_t i = 0; i < systems.size(); i++) {
                // games directly in the roms folder: this program's own console
                appendLocalRoot(p.path, systems[i], i == 0);
            }
            continue;
        }

        std::string gameListPath = p.path + "gamelist.xml";
        gameList->append(gameListPath, p.path, false, filters, p.system, false, [this](Game *game) {
            if (p_cb) p_cb(game);
            m_games_count++;
            if (game->available) m_games_available_count++;
            if (!(m_games_count % 200)) {
                setLoadingText("Games: %li / %li", m_games_available_count, m_games_count);
            }
        });
        setLoadingText("Games: %li / %li", m_games_available_count, m_games_count);
        printf("RomList::build: %s, games found: %zu / %zu (system: %s (0x%08x))\n",
               gameListPath.c_str(), gameList->getAvailableCount(),
               gameList->games.size(), p.system.name.c_str(), p.system.id);
    }

    // usb drives: only this console's folder(s), wherever they are in the first levels
    // ("/mnt/usb0/Roms/SNES", "/mnt/usb0/pppemu/roms/Nintendo - Super Nintendo...")
    if (useFolderRules) {
        for (const auto &usb: getUsbRoots()) {
            for (const auto &sys: systems) appendLocalRoot(usb, sys, false);
        }
    }

    logLine("RomList::build: %zu games found", gameList->games.size());
    if (gameList->games.empty() && useFolderRules) {
        const auto names = cfg->getCoreFolderNames();
        const std::string folder = names.empty() ? "" : names.back();
        warnings.push_back("No games found. Add games to " + ui->getIo()->getDataPath() + "roms/" +
                           (folder.empty() ? "" : ", a \"" + folder + "\" folder on a USB drive") +
                           " or a network share (main menu, NETWORK).");
    }

    // downloaded game information (developer, genre, year...) for games that don't have it
    for (auto a: artworks) {
        if (a->loadDatabase()) {
            setLoadingText("Applying game information...");
            a->applyDatabase(gameList);
        }
    }

    updateFilterOptions();

    // custom core hide/show flags
    auto ids = cfg->getCoreHiddenOptionToEnable();
    for (const auto &id: ids) {
        cfg->get(id)->setFlags(0);
    }

    // we need to reload config to update new options we just added
    printf("RomList::build: reloading config...\n");
    cfg->load();

    logLine("RomList::build: list built in %f", ui->getElapsedTime().asSeconds() - time_start);

    // remove ui widgets
    delete (rect);
}

void RomList::updateFilterOptions() {
    auto cfg = ui->getConfig();

    // sort games
    printf("RomList::updateFilterOptions: sorting games...\n");
    std::sort(gameList->systemList.systems.begin(), gameList->systemList.systems.end(), Api::sortSystemByName);
    std::sort(gameList->editors.begin(), gameList->editors.end(), Api::sortEditorByName);
    std::sort(gameList->developers.begin(), gameList->developers.end(), Api::sortDeveloperByName);
    std::sort(gameList->genres.begin(), gameList->genres.end(), Api::sortGenreByName);
    std::sort(gameList->players.begin(), gameList->players.end(), Api::sortInteger);
    std::sort(gameList->ratings.begin(), gameList->ratings.end(), Api::sortInteger);
    std::sort(gameList->rotations.begin(), gameList->rotations.end(), Api::sortInteger);
    std::sort(gameList->resolutions.begin(), gameList->resolutions.end(), Api::sortByName);
    std::sort(gameList->dates.begin(), gameList->dates.end(), Api::sortByName);

    // add filtering options (the current choice is kept when it's still in the new list)
    printf("RomList::updateFilterOptions: add filtering options...\n");
    auto grp = cfg->getGroup(PEMUConfig::GrpId::UI_FILTERING);
    auto setFilter = [grp](int id, const std::vector<std::string> &values) {
        auto opt = grp->getOption(id);
        const std::string current = opt->getString();
        int index = 0;
        for (size_t i = 0; i < values.size(); i++) {
            if (values[i] == current) index = (int) i;
        }
        opt->setArray(values, index);
    };
    setFilter(PEMUConfig::OptId::UI_FILTER_SYSTEM, gameList->systemList.getNames());
    setFilter(PEMUConfig::OptId::UI_FILTER_GENRE, gameList->getGenreNames());
    setFilter(PEMUConfig::OptId::UI_FILTER_DATE, gameList->getDates());
    setFilter(PEMUConfig::OptId::UI_FILTER_EDITOR, gameList->getEditorNames());
    setFilter(PEMUConfig::OptId::UI_FILTER_DEVELOPER, gameList->getDeveloperNames());
    setFilter(PEMUConfig::OptId::UI_FILTER_PLAYERS, gameList->getPlayersNames());
    setFilter(PEMUConfig::OptId::UI_FILTER_RATING, gameList->getRatingNames());
}

void RomList::initFav() {
    // load favorites
    delete (gameListFav);
    gameListFav = new GameList();
    gameListFav->append(ui->getIo()->getDataPath() + "favorites.xml");
    for (auto &g: gameListFav->games) {
        auto game = gameList->findGameByPathAndSystem(g.path, g.system.id);
        if (!game.path.empty()) {
            g.available = true;
            g.romsPath = game.romsPath;
        }
    }

    printf("RomList::initFav: %zu favorites\n", gameListFav->games.size());
}

void RomList::addFav(const Game &game) {
    if (!gameListFav->exist(game.id)) {
        gameListFav->games.emplace_back(game);
        gameListFav->save(ui->getIo()->getDataPath() + "favorites.xml", "mixrbv2", "", "video");
    }
}

void RomList::removeFav(const Game &game) {
    if (gameListFav->remove(game.id)) {
        gameListFav->save(ui->getIo()->getDataPath() + "favorites.xml", "mixrbv2", "", "video");
    }
}

void RomList::setLoadingText(const char *format, ...) {
    char buffer[512];
    va_list arg;
    va_start(arg, format);
    vsnprintf(buffer, 512, format, arg);
    va_end(arg);

    text->setString(buffer);
    ui->flip();
}

std::string RomList::getSmbCachePath() {
    return ui->getIo()->getDataPath() + "cache/smb/";
}

static bool hasExtension(const std::string &name, const std::vector<std::string> &exts) {
    const std::string lower = Utility::toLower(name);
    for (const auto &e: exts) {
        const std::string le = Utility::toLower(e);
        if (lower.size() >= le.size() && lower.compare(lower.size() - le.size(), le.size(), le) == 0) return true;
    }
    return false;
}

void RomList::addGames(const std::string &path, const System &system, std::vector<std::string> roms,
                       const std::string &gameListXml, const std::vector<std::string> &exts) {
    auto onGameAdded = [this](Game *game) {
        if (p_cb) p_cb(game);
        m_games_count++;
        if (game->available) m_games_available_count++;
        if (!(m_games_count % 200)) {
            setLoadingText("Games: %li / %li", m_games_available_count, m_games_count);
        }
    };

    gameList->romPaths.emplace_back(path);

    // gamelist.xml gives names, genres, dates...
    if (!gameListXml.empty()) {
        const size_t start = gameList->games.size();
        // empty roms path: sscrap must not list the folder itself, we already did (with sub folders)
        gameList->append(gameListXml, "", false, exts, system, false, nullptr);
        for (size_t i = start; i < gameList->games.size(); i++) {
            Game &g = gameList->games[i];
            g.romsPath = path;
            // gamelist.xml entries without a screenscraper id: the ui needs an id
            // for rom info, favorites and per-rom options
            if (g.id == 0) g.id = std::hash<std::string>()(path + "/" + g.path);
            auto it = std::find(roms.begin(), roms.end(), g.path);
            g.available = it != roms.end();
            if (g.available) roms.erase(it);
            onGameAdded(&g);
        }
    }

    // roms not in gamelist.xml (same as sscrap "unknown" files)
    for (const auto &name: roms) {
        Game game;
        game.id = std::hash<std::string>()(path + "/" + name);
        game.path = name;
        game.romsPath = path;
        // "A-M/Some Game.sfc" -> "Some Game"
        game.name = Utility::removeExt(name.substr(name.find_last_of('/') + 1));
        game.available = true;
        game.system = system;
        onGameAdded(&game);
        gameList->games.emplace_back(game);
    }

    auto &systems = gameList->systemList.systems;
    if (std::find_if(systems.begin(), systems.end(), [system](const System &s) {
        return s.id == system.id || s.name == system.name;
    }) == systems.end()) {
        systems.emplace_back(system);
    }

    setLoadingText("Games: %li / %li", m_games_available_count, m_games_count);
}

void RomList::appendLocal(const std::string &dir, const System &system, int maxDepth,
                          const std::vector<std::string> &exts) {
    std::string root = dir;
    if (root.empty() || root.back() != '/') root += "/";

    // same folder found twice (internal drive + usb listing...): add its games once
    if (std::find(m_scanned.begin(), m_scanned.end(), root) != m_scanned.end()) return;
    m_scanned.push_back(root);

    setLoadingText("Scanning %s...", root.c_str());
    std::vector<std::string> roms;
    std::function<void(const std::string &, const std::string &, int)> scan;
    scan = [&](const std::string &d, const std::string &rel, int depthLeft) {
        for (const auto &f: ui->getIo()->getDirList(d)) {
            if (f.name.empty() || f.name[0] == '.' || f.name[0] == '$') continue;
            const std::string name = rel.empty() ? f.name : rel + "/" + f.name;
            if (f.type == c2d::Io::Type::Directory) {
                if (depthLeft > 0) scan(d + f.name + "/", name, depthLeft - 1);
            } else if (hasExtension(f.name, exts)) {
                roms.push_back(name);
            }
        }
    };
    scan(root, "", maxDepth);

    const std::string xml = ui->getIo()->exist(root + "gamelist.xml") ? root + "gamelist.xml" : "";
    addGames(root, system, roms, xml, exts);
    logLine("RomList::appendLocal: %s, %zu roms (gamelist.xml: %s)",
           root.c_str(), roms.size(), xml.empty() ? "no" : "yes");
}

void RomList::appendLocalRoot(const std::string &root, const PEMUConfig::CoreSystem &sys, bool allowLooseFiles) {
    auto io = ui->getIo();
    if (io->getType(root) != c2d::Io::Type::Directory) return;

    const auto &names = sys.folderNames;
    const auto &system = sys.system;
    std::string dir = root;
    while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
    const std::string rootName = dir.substr(dir.find_last_of('/') + 1);

    // the folder is this console's folder itself
    if (matchesFolderName(rootName, names)) {
        appendLocal(root, system, LOCAL_MAX_FOLDER_DEPTH, sys.extensions);
        return;
    }

    // this console's folder(s) inside it: "Roms/Nintendo - Super Nintendo...", "pppemu/roms/SNES"...
    std::vector<std::string> found;
    std::function<void(const std::string &, int)> find;
    find = [&](const std::string &d, int depthLeft) {
        for (const auto &f: io->getDirList(d)) {
            if (f.type != c2d::Io::Type::Directory) continue;
            if (f.name.empty() || f.name[0] == '.' || f.name[0] == '$') continue;
            const std::string sub = d + f.name + "/";
            if (matchesFolderName(f.name, names)) {
                found.push_back(sub);       // scanned with its own sub folders, no need to go deeper
            } else if (depthLeft > 0) {
                find(sub, depthLeft - 1);
            }
        }
    };
    std::string start = root;
    if (start.back() != '/') start += "/";
    find(start, LOCAL_MAX_CONTAINER_DEPTH);

    for (const auto &f: found) appendLocal(f, system, LOCAL_MAX_FOLDER_DEPTH, sys.extensions);

    // the app's own roms folder: games directly in it are always used (old behaviour)
    if (allowLooseFiles) appendLocal(start, system, 0, sys.extensions);
}

#ifdef __SMB__

void RomList::appendSmb(const std::string &path, const System &system, int maxDepth,
                        const std::vector<std::string> &exts) {
    const std::string display = SmbIo::toDisplayUrl(path);
    setLoadingText("Connecting to %s...", display.c_str());

    // list roms (and the top level gamelist.xml), sub folders included
    std::vector<std::string> wanted = exts;
    wanted.emplace_back("gamelist.xml");
    std::vector<SmbIo::File> remote;
    std::string error;
    bool online = SmbIo::listFiles(path, wanted, &remote, &error, maxDepth);
    if (!online) {
        logLine("RomList::appendSmb: %s: %s", display.c_str(), error.c_str());
        setLoadingText("Network: %s", SmbIo::friendlyError(error, path).c_str());
        ui->delay(3000);

        // server unreachable: show what was already downloaded (playable offline)
        std::function<void(const std::string &, const std::string &)> scan;
        scan = [&](const std::string &dir, const std::string &rel) {
            for (const auto &f: ui->getIo()->getDirList(dir)) {
                if (f.name.empty() || f.name[0] == '.') continue;
                const std::string name = rel.empty() ? f.name : rel + "/" + f.name;
                if (f.type == c2d::Io::Type::Directory) {
                    scan(dir + f.name + "/", name);
                } else if (!Utility::endsWith(f.name, ".part")) {
                    remote.push_back({name, f.size});
                }
            }
        };
        scan(SmbIo::getCacheDir(getSmbCachePath(), path), "");
        std::string warning = "Network: " + SmbIo::friendlyError(error, path);
        while (!warning.empty() && warning.back() == '.') warning.pop_back();
        if (remote.empty()) {
            warnings.push_back(warning + ".");
            return;
        }
        warnings.push_back(warning + ". Showing " + std::to_string(remote.size()) +
                           " network games played before (offline).");
        printf("RomList::appendSmb: offline, using %zu cached files\n", remote.size());
    }

    std::vector<std::string> roms;
    bool hasGameList = false;
    for (const auto &f: remote) {
        if (Utility::toLower(f.name) == "gamelist.xml") {
            hasGameList = true;
        } else if (SmbIo::matchExt(f.name, exts)) {
            roms.push_back(f.name);
        }
    }

    if (online && roms.empty()) {
        // connected fine but nothing to show: say so instead of failing silently
        // (one console folder being empty is normal: the warning is given if the whole share is empty)
        setLoadingText("Network: no games found in %s", display.c_str());
    }

    // remote gamelist.xml (cached locally, refreshed when it changes)
    std::string localXml;
    if (hasGameList) {
        localXml = SmbIo::fetchToCache(path, "gamelist.xml",
                                       SmbIo::getCacheDir(getSmbCachePath(), path), nullptr, &error);
    }

    addGames(path, system, roms, localXml, exts);
    logLine("RomList::appendSmb: %s, %zu files (gamelist.xml: %s, online: %s)",
           display.c_str(), remote.size(), hasGameList ? "yes" : "no", online ? "yes" : "no");
}

#endif

RomList::~RomList() {
    printf("~RomList()\n");
    for (auto a: artworks) delete (a);
    delete (gameList);
    delete (gameListFav);
}
