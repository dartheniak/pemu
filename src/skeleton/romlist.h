//
// Created by cpasjuste on 19/10/16.
//

#ifndef C2DUI_ROMLIST_H
#define C2DUI_ROMLIST_H

#include <vector>

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

    protected:
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
