//
// Created by cpasjuste on 19/10/16.
//

#ifndef C2DUI_ROMLIST_H
#define C2DUI_ROMLIST_H

#include <vector>

// local roms: sub folder levels scanned below a console folder
#define LOCAL_MAX_FOLDER_DEPTH 6
// local roms: how deep a console folder is looked for ("usb0/pppsnes/roms/SNES" = 3)
#define LOCAL_MAX_CONTAINER_DEPTH 3

namespace pemu {
    class UiMain;

    class RomList {
    public:
        RomList(UiMain *ui, const std::string &emuVersion, const std::vector<std::string> &filters);

        virtual ~RomList();

        virtual void build(const ss_api::GameList::GameAddedCb &cb = nullptr);

        virtual void initFav();

        void addFav(const ss_api::Game &game);

        void removeFav(const ss_api::Game &game);

        void setLoadingText(const char *format, ...);

        UiMain *ui;
        c2d::RectangleShape *rect;
        c2d::Text *text;
        ss_api::GameList *gameList = nullptr;
        ss_api::GameList *gameListFav = nullptr;
        ss_api::GameList::GameAddedCb p_cb = nullptr;

        // local directory where roms from smb:// paths are cached
        std::string getSmbCachePath();

        // usb drives roots scanned for this console's folder
        virtual std::vector<std::string> getUsbRoots() { return {"/mnt/usb0/", "/mnt/usb1/"}; }

    protected:
        // add games found in "path": "roms" are file names relative to "path" (sub folders allowed),
        // "gameListXml" an optional local gamelist.xml with names, genres...
        void addGames(const std::string &path, const ss_api::System &system,
                      std::vector<std::string> roms, const std::string &gameListXml);

        // add games of a local folder, "maxDepth" levels of sub folders included
        void appendLocal(const std::string &dir, const ss_api::System &system, int maxDepth);

        // add games of a local roms root (internal drive, usb): only this console's folder(s),
        // found by name. "allowLooseFiles": also use the files directly in "root".
        void appendLocalRoot(const std::string &root, const ss_api::System &system, bool allowLooseFiles);

        std::vector<std::string> m_scanned;

#ifdef __SMB__
        // add games from an smb:// rom path (remote listing + optional remote gamelist.xml)
        // "maxDepth": how many levels of sub folders are scanned
        void appendSmb(const std::string &path, const ss_api::System &system, int maxDepth);
#endif

        std::vector<std::string> filters;
        size_t m_games_count = 0;
        size_t m_games_available_count = 0;
    };
}

#endif //C2DUI_ROMLIST_H
