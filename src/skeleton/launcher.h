//
// pppEmu: games of other consoles run in another program of the package.
//
// The launcher (eboot.bin) lists every console's games. A game whose console has a
// "program" (PEMUConfig::CoreSystem) is started by replacing the launcher with that
// program (sceSystemServiceLoadExec on the ps4, exec elsewhere), with:
//   --pppemu-rom=<local rom file>       the game (network games are downloaded first)
//   --pppemu-return=<launcher program>  where to go back when the game is closed
//   --pppemu-select=<game>              the game to select again in the launcher's list
// When the game is closed, that program starts the launcher again with --pppemu-select.
//

#ifndef PEMU_LAUNCHER_H
#define PEMU_LAUNCHER_H

#include <string>
#include <vector>

namespace ss_api {
    class Game;
}

namespace c2d::config {
    class Config;
}

namespace pemu {
    class UiMain;

    struct LaunchArgs {
        // game to run (--pppemu-rom, or a plain file argument)
        std::string rom;
        // program to go back to when the game is closed
        std::string returnTo;
        // game to select in the list (romsPath + path)
        std::string select;
        // settings shared by all the programs (display, controls), see writeSharedSettings
        std::string settings;
        // console page (system filter, L2 / R2) to show again in the launcher's list
        std::string system;
        // true if started by the launcher or returning to it
        bool pppemu = false;
    };

    LaunchArgs parseLaunchArgs(int argc, char **argv);

    // display and controller settings shared by pppEmu's programs: written to "path" when
    // a program hands over to another one, applied (and saved) by the program starting.
    // Only the options both programs have are used (same group and option name).
    bool writeSharedSettings(c2d::config::Config *cfg, const std::string &path);
    bool applySharedSettings(c2d::config::Config *cfg, const std::string &path);

    // "smb://user:password@host/..." -> "smb://user:***@host/..." (for the log)
    std::string hidePassword(const std::string &text);

    // this program's own path, given to the other program to come back here
    std::string selfProgramPath();

    // if "game" belongs to a console run by another program: get it ready (network games are
    // downloaded) and close the ui, main() then starts that program (runPendingLaunch).
    // Returns false if the game is run by this program, true otherwise (errors are shown).
    bool launchInOtherProgram(UiMain *ui, const ss_api::Game &game);

    // replace this program with "program" (args without the program name).
    // Only returns on errors.
    bool execProgram(const std::string &program, const std::vector<std::string> &args, std::string *error);

    // called by main() once the ui is closed: starts the program chosen by launchInOtherProgram.
    // Only returns if there was nothing to start, or on errors.
    bool runPendingLaunch();

    // game closed in a program started by the launcher: go back to it.
    // Only returns on errors (then the program should exit as usual).
    void returnToLauncher(const LaunchArgs &args);
}

#endif //PEMU_LAUNCHER_H
