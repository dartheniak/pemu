//
// pppEmu: the animated "Neon Horizon" background (nh_bg.h) behind the whole ui: the loading
// screen and the game list. One colour per console page (L2 / R2), crossfading.
//
// Started as early as possible (main(), before the game list is loaded). Built scenes are
// saved in <data>/cache/background/ and loaded from there the next times, and the other
// console pages are built there while nothing else is going on, so after the first run the
// background is on screen almost at once, even when coming back from another emulator.
//

#ifndef PEMU_NEON_BACKGROUND_H
#define PEMU_NEON_BACKGROUND_H

#include <string>
#include "cross2d/c2d.h"

struct nh_bg;

namespace pemu {
    class UiMain;

    class NeonBackground {
    public:
        explicit NeonBackground(UiMain *ui);

        ~NeonBackground();

        // console page name ("Mega Drive", "ALL"...) -> the module's system id ("genesis"...)
        static const char *systemId(const std::string &page);

        // only a program showing the game list has it (not one started to run a game)
        void setEnabled(bool value) { mEnabled = value; }

        // shows this console page's colours (started now if not yet, and enabled)
        void setPage(const std::string &page);

        // every frame: draws the next frame if "visible" (the list or the loading screen is
        // on screen), else hidden and idle work stopped (a game is running)
        void update(bool visible);

    private:
        void start(const char *id);

        UiMain *pMain = nullptr;
        ::nh_bg *mNh = nullptr;
        c2d::C2DTexture *mTexture = nullptr;
        c2d::C2DClock mClock;
        std::string mId;
        bool mFailed = false;
        bool mEnabled = false;
    };
}

#endif //PEMU_NEON_BACKGROUND_H
