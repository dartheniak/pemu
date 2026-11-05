//
// pppEmu: the animated "Neon Horizon" background (nh_bg.h) behind the whole ui: the loading
// screen and the game list.
//
// One scene for every console page, built in grey and coloured on screen (the texture's
// colour, done by the GPU) with the kiwi logo colours cycling like the borders
// (UI_OPTIONS > BORDER_ANIMATION: OFF = one still colour). Started as early as possible
// (main(), before the game list is loaded), saved in <data>/cache/background/ the first time
// and loaded from there the next times.
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

        // only a program showing the game list has it (not one started to run a game)
        void setEnabled(bool value) { mEnabled = value; }

        // started now if not yet (and enabled)
        void start();

        // every frame: draws the next frame if "visible" (the list or the loading screen is
        // on screen), else hidden (a game is running)
        void update(bool visible);

    private:
        UiMain *pMain = nullptr;
        ::nh_bg *mNh = nullptr;
        c2d::C2DTexture *mTexture = nullptr;
        c2d::C2DClock mClock;
        bool mFailed = false;
        bool mEnabled = false;
    };
}

#endif //PEMU_NEON_BACKGROUND_H
