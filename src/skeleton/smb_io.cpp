//
// SMB (samba / windows shares) support for pEMU
//

#ifdef __SMB__

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <fcntl.h>
#include <sys/stat.h>
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include "smb_io.h"

using namespace pemu;

#define SMB_TIMEOUT_SECONDS 10
#define SMB_MAX_CHUNK (1024 * 1024)

static std::string lower(std::string s) {
    for (auto &c: s) c = (char) tolower((unsigned char) c);
    return s;
}

static std::string trimSlashes(const std::string &s) {
    size_t a = s.find_first_not_of('/');
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of('/');
    return s.substr(a, b - a + 1);
}

static std::string percentDecode(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()
            && isxdigit((unsigned char) s[i + 1]) && isxdigit((unsigned char) s[i + 2])) {
            out += (char) strtol(s.substr(i + 1, 2).c_str(), nullptr, 16);
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

static bool mkdirs(const std::string &path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); i++) {
        cur += path[i];
        if ((path[i] == '/' || i == path.size() - 1) && cur.size() > 1) {
            if (mkdir(cur.c_str(), 0777) != 0 && errno != EEXIST) {
                struct stat st{};
                if (stat(cur.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return false;
            }
        }
    }
    return true;
}

static void setError(std::string *error, const std::string &msg) {
    printf("SmbIo: %s\n", msg.c_str());
    if (error) *error = msg;
}

// connect to url.server/url.share, returns nullptr on error
static smb2_context *smbConnect(const SmbIo::Url &url, std::string *error) {
    smb2_context *smb2 = smb2_init_context();
    if (!smb2) {
        setError(error, "could not create smb2 context");
        return nullptr;
    }

    smb2_set_timeout(smb2, SMB_TIMEOUT_SECONDS);

    // libsmb2 falls back to the current unix user when no user is given,
    // which doesn't exist on consoles: default to "guest"
    const std::string user = url.user.empty() ? "guest" : url.user;
    smb2_set_user(smb2, user.c_str());
    // NOTE: only set a password when we have one. With an empty password set,
    // libsmb2 signs guest sessions with an invalid key and servers reject them
    // (STATUS_ACCESS_DENIED on tree connect).
    if (!url.password.empty()) {
        smb2_set_password(smb2, url.password.c_str());
    }
    if (!url.domain.empty()) {
        smb2_set_domain(smb2, url.domain.c_str());
    }

    if (smb2_connect_share(smb2, url.server.c_str(), url.share.c_str(), user.c_str()) < 0) {
        setError(error, "connection to \\\\" + url.server + "\\" + url.share
                        + " failed: " + smb2_get_error(smb2));
        smb2_destroy_context(smb2);
        return nullptr;
    }

    return smb2;
}

static void smbDisconnect(smb2_context *smb2) {
    if (!smb2) return;
    smb2_disconnect_share(smb2);
    smb2_destroy_context(smb2);
}

bool SmbIo::isSmbPath(const std::string &path) {
    return path.size() > 6 && lower(path.substr(0, 6)) == "smb://";
}

bool SmbIo::parseUrl(const std::string &url, Url *out) {
    if (!isSmbPath(url) || !out) return false;

    *out = Url();
    std::string rest = url.substr(6);

    // "[credentials@]server" ends at the first '/'. Special characters in
    // credentials ('/', '@', ':', ...) must be percent-encoded (%2F, %40, %3A).
    // '@' in the share/path part (rom file names) is allowed as-is.
    const size_t slash = rest.find('/');
    if (slash == std::string::npos) return false;
    const std::string head = rest.substr(0, slash);
    const size_t at = head.rfind('@');

    std::string authority = at != std::string::npos ? head.substr(0, at) : "";
    std::string hostAndPath = (at != std::string::npos ? head.substr(at + 1) : head) + rest.substr(slash);

    if (!authority.empty()) {
        // [domain;]user[:password]
        size_t semi = authority.find(';');
        if (semi != std::string::npos) {
            out->domain = percentDecode(authority.substr(0, semi));
            authority = authority.substr(semi + 1);
        }
        size_t colon = authority.find(':');
        if (colon != std::string::npos) {
            out->user = percentDecode(authority.substr(0, colon));
            out->password = percentDecode(authority.substr(colon + 1));
        } else {
            out->user = percentDecode(authority);
        }
    }

    size_t s = hostAndPath.find('/');
    if (s == std::string::npos || s == 0) return false;
    out->server = hostAndPath.substr(0, s);

    std::string sharePath = trimSlashes(hostAndPath.substr(s + 1));
    if (sharePath.empty()) return false;
    size_t s2 = sharePath.find('/');
    if (s2 == std::string::npos) {
        out->share = percentDecode(sharePath);
    } else {
        out->share = percentDecode(sharePath.substr(0, s2));
        out->path = percentDecode(trimSlashes(sharePath.substr(s2 + 1)));
    }

    return !out->server.empty() && !out->share.empty();
}

bool SmbIo::matchExt(const std::string &name, const std::vector<std::string> &filters) {
    if (filters.empty()) return true;
    const std::string ln = lower(name);
    for (const auto &f: filters) {
        const std::string e = lower(f);
        if (ln.size() >= e.size() && ln.compare(ln.size() - e.size(), e.size(), e) == 0) {
            return true;
        }
    }
    return false;
}

std::string SmbIo::toDisplayUrl(const std::string &url) {
    Url u;
    if (!parseUrl(url, &u)) return url;
    std::string res = "smb://";
    if (!u.domain.empty()) res += u.domain + ";";
    if (!u.user.empty()) res += u.user + "@";
    res += u.server + "/" + u.share;
    if (!u.path.empty()) res += "/" + u.path;
    return res;
}

bool SmbIo::listFiles(const std::string &dirUrl, const std::vector<std::string> &filters,
                      std::vector<File> *out, std::string *error) {
    Url url;
    if (!parseUrl(dirUrl, &url)) {
        setError(error, "invalid smb url: " + toDisplayUrl(dirUrl));
        return false;
    }

    smb2_context *smb2 = smbConnect(url, error);
    if (!smb2) return false;

    smb2dir *dir = smb2_opendir(smb2, url.path.c_str());
    if (!dir) {
        setError(error, "could not open directory \"" + url.path + "\": " + smb2_get_error(smb2));
        smbDisconnect(smb2);
        return false;
    }

    smb2dirent *ent;
    while ((ent = smb2_readdir(smb2, dir))) {
        if (!ent->name || ent->name[0] == '.') continue;
        if (ent->st.smb2_type != SMB2_TYPE_FILE) continue;
        if (!matchExt(ent->name, filters)) continue;
        if (out) out->push_back({ent->name, ent->st.smb2_size});
    }

    smb2_closedir(smb2, dir);
    smbDisconnect(smb2);
    return true;
}

static bool downloadWith(smb2_context *smb2, const std::string &remotePath, const std::string &localPath,
                         uint64_t *remoteSize, const SmbIo::ProgressCb &progress, std::string *error) {
    smb2fh *fh = smb2_open(smb2, remotePath.c_str(), O_RDONLY);
    if (!fh) {
        setError(error, "could not open \"" + remotePath + "\": " + smb2_get_error(smb2));
        return false;
    }

    smb2_stat_64 st{};
    if (smb2_fstat(smb2, fh, &st) < 0) {
        setError(error, "could not stat \"" + remotePath + "\": " + smb2_get_error(smb2));
        smb2_close(smb2, fh);
        return false;
    }
    if (remoteSize) *remoteSize = st.smb2_size;

    const std::string partPath = localPath + ".part";
    FILE *fp = fopen(partPath.c_str(), "wb");
    if (!fp) {
        setError(error, "could not create \"" + partPath + "\": " + strerror(errno));
        smb2_close(smb2, fh);
        return false;
    }

    uint32_t chunk = smb2_get_max_read_size(smb2);
    if (chunk == 0 || chunk > SMB_MAX_CHUNK) chunk = SMB_MAX_CHUNK;
    auto *buf = new uint8_t[chunk];

    bool ok = true;
    uint64_t done = 0;
    if (progress && !progress(0, st.smb2_size)) ok = false;
    while (ok && done < st.smb2_size) {
        int count = smb2_read(smb2, fh, buf, chunk);
        if (count < 0) {
            setError(error, "read error on \"" + remotePath + "\": " + smb2_get_error(smb2));
            ok = false;
            break;
        }
        if (count == 0) break;
        if (fwrite(buf, 1, (size_t) count, fp) != (size_t) count) {
            setError(error, "write error on \"" + partPath + "\": " + strerror(errno));
            ok = false;
            break;
        }
        done += (uint64_t) count;
        if (progress && !progress(done, st.smb2_size)) {
            setError(error, "download cancelled");
            ok = false;
        }
    }

    delete[] buf;
    fclose(fp);
    smb2_close(smb2, fh);

    if (ok && done != st.smb2_size) {
        setError(error, "short read on \"" + remotePath + "\"");
        ok = false;
    }

    if (!ok) {
        remove(partPath.c_str());
        return false;
    }

    remove(localPath.c_str());
    if (rename(partPath.c_str(), localPath.c_str()) != 0) {
        setError(error, "could not rename \"" + partPath + "\": " + strerror(errno));
        remove(partPath.c_str());
        return false;
    }

    return true;
}

std::string SmbIo::getCacheDir(const std::string &baseCacheDir, const std::string &dirUrl) {
    Url url;
    std::string key;
    if (parseUrl(dirUrl, &url)) {
        key = url.server + "_" + url.share + (url.path.empty() ? "" : "_" + url.path);
    } else {
        key = dirUrl;
    }

    for (auto &c: key) {
        if (!isalnum((unsigned char) c) && c != '-' && c != '.') c = '_';
    }

    std::string base = baseCacheDir;
    if (!base.empty() && base.back() != '/') base += "/";
    return base + key + "/";
}

std::string SmbIo::fetchToCache(const std::string &dirUrl, const std::string &fileName,
                                const std::string &cacheDir,
                                const ProgressCb &progress, std::string *error) {
    Url url;
    if (!parseUrl(dirUrl, &url) || fileName.empty()) {
        setError(error, "invalid smb url: " + toDisplayUrl(dirUrl));
        return "";
    }
    const std::string remotePath = url.path.empty() ? fileName : url.path + "/" + fileName;

    std::string dir = cacheDir;
    if (!dir.empty() && dir.back() != '/') dir += "/";
    if (!mkdirs(dir)) {
        setError(error, "could not create cache directory \"" + dir + "\"");
        return "";
    }
    const std::string localPath = dir + fileName;

    smb2_context *smb2 = smbConnect(url, error);
    if (!smb2) {
        // offline: fall back to a previously cached copy if we have one
        struct stat lst{};
        if (stat(localPath.c_str(), &lst) == 0 && lst.st_size > 0) {
            printf("SmbIo::fetchToCache: server unreachable, using cached %s\n", localPath.c_str());
            return localPath;
        }
        return "";
    }

    // reuse cached copy when sizes match
    smb2_stat_64 rst{};
    struct stat lst{};
    if (smb2_stat(smb2, remotePath.c_str(), &rst) == 0
        && stat(localPath.c_str(), &lst) == 0
        && (uint64_t) lst.st_size == rst.smb2_size) {
        printf("SmbIo::fetchToCache: cache hit %s\n", localPath.c_str());
        smbDisconnect(smb2);
        return localPath;
    }

    printf("SmbIo::fetchToCache: downloading %s/%s -> %s\n",
           toDisplayUrl(dirUrl).c_str(), fileName.c_str(), localPath.c_str());
    bool ok = downloadWith(smb2, remotePath, localPath, nullptr, progress, error);
    smbDisconnect(smb2);
    return ok ? localPath : "";
}

#endif // __SMB__
