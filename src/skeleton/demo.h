//
// pppEmu attract mode ("demo mode"): random games shown one after the other.
//
// The launcher's list starts it (idle time, or L3 + R3, see UIRomList) and picks the games.
// A game runs for DEMO_TIME seconds in whatever program runs its console (pppEmu itself, or
// the Genesis / NES / Game Boy program, started with --pppemu-demo, see launcher.h):
//   - any button: the demo ends, back to the launcher's list on that game
//   - time is up: the next random game is started
// While a demo game runs, the emulators don't write the game's saves (sram, battery...),
// and the sound can be off (DEMO_SOUND).
//

#ifndef PEMU_DEMO_H
#define PEMU_DEMO_H

#include <string>

namespace pemu {
    namespace demo {
        // a demo game is starting/running
        void begin(int seconds, bool sound);

        // demo over (button pressed, or not started)
        void end();

        bool isActive();

        int getSeconds();

        bool hasSound();

        // the time of the current game is up: the next game should be started
        void setNextWanted(bool value);

        bool isNextWanted();

        // "1 MIN" -> 60, "30 SEC" -> 30, "OFF" -> 0
        int parseDuration(const std::string &value);
    }
}

#endif //PEMU_DEMO_H
