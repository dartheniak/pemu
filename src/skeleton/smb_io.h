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

// how many levels of sub folders are scanned below a network rom folder
#define SMB_MAX_FOLDER_DEPTH 6

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

        // build an smb:// url from plain (not url encoded) settings, as typed by the user.
        // "server" may also be a full windows path ("\\192.168.1.20\Roms\SNES"), in which
        // case its share/folder part is used when "folder" is empty.
        // Returns an empty string if server or share are missing.
        static std::string buildUrl(const std::string &server, const std::string &folder,
                                    const std::string &user, const std::string &password);

        // names of the direct sub folders of "dirUrl"
        static bool listDirs(const std::string &dirUrl, std::vector<std::string> *out,
                             std::string *error = nullptr);

        // url of the sub folder "name" (raw, not url encoded) of "dirUrl"
        static std::string childUrl(const std::string &dirUrl, const std::string &name);

        // last folder name of a url ("smb://host/Roms/SNES/" -> "SNES", "smb://host/Roms/" -> "Roms")
        static std::string folderName(const std::string &dirUrl);

        // folder name comparison ignoring case, spaces and punctuation:
        // "Nintendo - Super Nintendo Entertainment System" == "nintendo super nintendo entertainment system"
        static bool sameFolderName(const std::string &a, const std::string &b);

        // Find where a console's games are, from a folder chosen by the user:
        // - the folder itself if its name is one of "names" (it is that console's folder)
        // - else its direct sub folders whose name is one of "names" ("Roms" -> "Roms/SNES")
        // - else nothing: "found" is set to false and "urls" holds "dirUrl" only
        //   (its files are then used, without scanning sub folders)
        // "names" empty: "dirUrl" is used as is (old behaviour, sub folders scanned).
        struct SystemFolders {
            std::vector<std::string> urls;
            bool found = false;     // a folder matching "names" was found
            bool scanSubFolders = true;
        };

        static bool findSystemFolders(const std::string &dirUrl, const std::vector<std::string> &names,
                                      SystemFolders *out, std::string *error = nullptr);

        // turn a libsmb2 error into a short message for the user
        static std::string friendlyError(const std::string &error, const std::string &url);

        // true if "name" ends with one of "filters" (case insensitive, empty filters = true)
        static bool matchExt(const std::string &name, const std::vector<std::string> &filters);

        // url without the password (safe for logging / display)
        static std::string toDisplayUrl(const std::string &url);

        // list files (not directories) in "dirUrl" whose extension is in "filters" (empty = all).
        // Sub folders are scanned up to "maxDepth" levels deep; their files are returned
        // with a relative name ("A-M/game.sfc").
        static bool listFiles(const std::string &dirUrl, const std::vector<std::string> &filters,
                              std::vector<File> *out, std::string *error = nullptr, int maxDepth = 0);

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
