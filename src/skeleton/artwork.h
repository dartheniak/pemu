//
// Artwork and game information downloader (libretro thumbnails + libretro database)
//
// Images:  http://thumbnails.libretro.com/<System>/Named_Boxarts/<Game>.png
//          (fallback: raw.githubusercontent.com/libretro-thumbnails/<System_>/master/...)
// Infos:   raw.githubusercontent.com/libretro/libretro-database/master/metadat/<field>/<System>.dat
// Games are matched by their file name, which must follow the No-Intro naming
// ("Super Mario World (USA).sfc"). Everything is stored in "<data>/media/".
//

#ifndef PEMU_ARTWORK_H
#define PEMU_ARTWORK_H

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <cstdint>

namespace ss_api {
    class Game;

    class GameList;
}

namespace c2d {
    class Io;

    class Thread;

    class Mutex;
}

namespace pemu {
    class Artwork {
    public:
        enum class Type {
            Boxart = 0,
            Title = 1,
            Snap = 2
        };

        // progress: done, total, current item name. Return false to cancel.
        typedef std::function<bool(int, int, const std::string &)> ProgressCb;

        // "system": libretro system name ("Nintendo - Super Nintendo Entertainment System")
        Artwork(c2d::Io *io, const std::string &system);

        ~Artwork();

        // libretro game name of a game: its file name without extension
        static std::string gameName(const ss_api::Game &game);

        // libretro thumbnail file name: "&*/:`<>?\|\"" replaced by '_'
        static std::string thumbnailName(const std::string &gameName);

        // local image path (whether it exists or not)
        std::string imagePath(const ss_api::Game &game, Type type);

        // local image path if it was downloaded, else empty
        std::string findImage(const ss_api::Game &game, Type type);

        // true if the image is known to not exist online (a previous download got a "not found")
        bool isMissing(const ss_api::Game &game, Type type);

        // download one image now (blocking). Returns true if the image is now available locally.
        bool downloadImage(const ss_api::Game &game, Type type, std::string *error = nullptr);

        // download the game information database (blocking)
        bool downloadDatabase(const ProgressCb &progress, std::string *error = nullptr);

        // load the downloaded database, returns false if it was never downloaded
        bool loadDatabase();

        // fill unknown developer, publisher, genre, date and players of the list's games.
        // Returns the number of games updated.
        int applyDatabase(ss_api::GameList *list);

        // background download of a game's images (replaces the previous requests: only the
        // selected game matters)
        void request(const ss_api::Game &game, const std::vector<Type> &types);

        // name of an image downloaded in background since the last call (empty if none)
        std::string takeDownloaded();

        // true if the game information database was never downloaded, or is old
        bool needsDatabase();

        // download the game information database in background (done before the images)
        void requestDatabase();

        // true once, after a background database download succeeded
        bool takeDatabaseReady();

        // a message about a background download problem (no internet...), empty if none
        std::string takeNotice();

        // number of games in the loaded database
        size_t databaseSize() const { return m_db.size(); }

    private:
        struct Info {
            std::string developer, publisher, genre, year;
            int players = 0;
        };

        static int workerMain(void *data);

        // network init, on first download only
        void start();

        // background thread, on first background request only
        void startWorker();

        int httpGet(const std::string &url, const std::string &dstPath, std::string *error);

        std::string mediaPath();

        c2d::Io *m_io;
        std::string m_system;
        std::string m_caFile;
        std::unordered_map<std::string, Info> m_db;

        // background worker
        bool m_started = false;
        bool m_workerStarted = false;
        void *m_sdlThread = nullptr;
        c2d::Mutex *m_mutex = nullptr;
        std::atomic<bool> m_quit{false};
        std::vector<std::pair<std::string, Type>> m_requests;
        std::vector<std::string> m_downloaded;
        bool m_dbRequested = false;
        bool m_dbReady = false;
        std::string m_notice;
        // after network failures, background downloads stop until this time (SDL ticks)
        uint32_t m_pauseUntil = 0;
        int m_failuresInRow = 0;

        void setNotice(const std::string &msg);
    };
}

#endif //PEMU_ARTWORK_H
