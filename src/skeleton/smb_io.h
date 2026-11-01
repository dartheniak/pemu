//
// SMB (samba / windows shares) support for pEMU
//
// Rom paths in the config file can point to a network share:
//   smb://[domain;][user[:password]@]server[:port]/share/path/
//
// Remote roms are listed directly from the share, and downloaded to a local
// cache directory when a game is launched (cores keep using plain file paths).
//

#ifndef PEMU_SMB_IO_H
#define PEMU_SMB_IO_H

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace pemu {
    class SmbIo {
    public:
        struct File {
            std::string name;
            uint64_t size = 0;
        };

        struct Url {
            std::string domain;
            std::string user;
            std::string password;
            std::string server;   // host[:port]
            std::string share;
            std::string path;     // path inside the share, no leading/trailing '/'
        };

        // progress: bytes done, bytes total. Return false to cancel.
        typedef std::function<bool(uint64_t, uint64_t)> ProgressCb;

        // true if "path" is an smb:// url
        static bool isSmbPath(const std::string &path);

        // parse an smb:// url, returns false on malformed url
        static bool parseUrl(const std::string &url, Url *out);

        // true if "name" ends with one of "filters" (case insensitive, empty filters = true)
        static bool matchExt(const std::string &name, const std::vector<std::string> &filters);

        // url without the password (safe for logging / display)
        static std::string toDisplayUrl(const std::string &url);

        // list files (not directories) in "dirUrl" whose extension is in "filters" (empty = all)
        static bool listFiles(const std::string &dirUrl, const std::vector<std::string> &filters,
                              std::vector<File> *out, std::string *error = nullptr);

        // Return a local path for the remote file "fileName" located in "dirUrl", downloading it
        // into "cacheDir" if needed ("fileName" is a raw file name, not url encoded).
        // A cached file is reused if its size matches the remote file size, or if the
        // server is unreachable. The download is written to "<file>.part" then renamed.
        // Returns an empty string on failure.
        static std::string fetchToCache(const std::string &dirUrl, const std::string &fileName,
                                        const std::string &cacheDir,
                                        const ProgressCb &progress = nullptr, std::string *error = nullptr);

        // local cache directory for a given remote directory url (one sub directory per remote path)
        static std::string getCacheDir(const std::string &baseCacheDir, const std::string &dirUrl);
    };
}

#endif //PEMU_SMB_IO_H
