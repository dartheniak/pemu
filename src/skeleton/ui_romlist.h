//
// Created by cpasjuste on 14/01/18.
//

#ifndef C2DUI_UI_ROMLIST_H
#define C2DUI_UI_ROMLIST_H

class UIRomInfo;

namespace pemu {

    class UIRomList : public SkinnedRectangle {

    public:
        enum PreviewType {
            Tex,
            Vid
        };

        UIRomList(UiMain *ui, RomList *romList, const c2d::Vector2f &size);

        ~UIRomList() override;

        virtual void filterRomList();

        virtual void sortRomList();

        virtual void updateRomList();

        virtual ss_api::Game getSelection();

        virtual RomList *getRomList();

        // select the game "romsPath + path" in the list, false if it's not there
        bool selectGame(const std::string &fullPath);

        // demo mode (see demo.h): start a random game now
        void startDemo();

        // demo mode: start a random game as soon as the list is shown (back from another program)
        void startDemoLater() { mDemoPending = true; }

        // demo mode: a game run by this program is over (button pressed, or next game wanted)
        void onDemoGameDone(const ss_api::Game &game);

        // image type chosen in the ARTWORK menu
        Artwork::Type getArtworkType();

        // image type shown in the bottom box (ROM_SYNOPSIS) when the game has no synopsis:
        // the screenshot, or the title screen if the screenshot is already the main image
        // images of the bottom box: the two types not shown in the main box (title screen and
        // screenshot when the main box shows the box art), left one first
        std::vector<Artwork::Type> getSecondArtworkTypes();

        // true if images and game information are downloaded automatically (ARTWORK menu)
        bool isArtworkAutoDownload();

        virtual RectangleShape *getBlur() { return pBlur; };

        virtual std::string getPreview(const ss_api::Game &game, PreviewType type);

        virtual void setVideoSnapDelay(int delay);

        void setGames(const std::vector<ss_api::Game> &games);

        void setVisibility(c2d::Visibility visibility, bool tweenPlay = false) override;

    protected:

        bool onInput(c2d::Input::Player *players) override;

        void onUpdate() override;

        UiMain *pMain = nullptr;
        RomList *pRomList = nullptr;
        UIRomInfo *pRomInfo = nullptr;
        UIListBox *pListBox = nullptr;
        RectangleShape *pBlur = nullptr;
        SkinnedText *pTitleText = nullptr;
        float mTitleMaxWidth = 0;
        ss_api::GameList mGameList;
        bool mShowSystemText = false;
        bool mArtworkDbChecked = false;

        // demo mode: idle time in the list, game shown last, start requested
        c2d::C2DClock mIdleClock;
        std::string mDemoLastGame;
        bool mDemoPending = false;

        // per console background ("background_mega_drive.jpg" in the skin folder), shown
        // instead of the skin's own background when that console is selected (L2 / R2)
        void updateBackground();
        c2d::C2DTexture *mBackground = nullptr;
        std::string mBackgroundPath;

        c2d::C2DClock mTimerLoadInfo;
#ifdef __3DS__
        int mTimerLoadInfoDelay = 1000;
#else
        int mTimerLoadInfoDelay = 300;
#endif
        int mTimerLoadInfoDone = 0;
        c2d::C2DClock mTimerLoadVideo;
        int mTimerLoadVideoDelay = 5000;
        int mTimerLoadVideoDone = 0;
    };
}

#endif //C2DUI_UI_ROMLIST_H
