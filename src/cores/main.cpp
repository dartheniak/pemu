//
// Created by cpasjuste on 19/09/23.
//

#include "main.h"
#include "skeleton/pemu_log.h"
#include "skeleton/launcher.h"

using namespace c2d;
using namespace pemu;

PEMUUiMain *pemu_ui;

int main(int argc, char **argv) {
    // command line game info
    Game game;

    // custom io
    const auto io = new PEMUIo();

    // create main ui/renderer
    // NOTE: size must stay {0, 0} on non-switch platforms (PS4, PS5, Vita, Linux, Windows...).
    // libcross2d's SDL2Renderer only sets SDL_WINDOW_FULLSCREEN_DESKTOP when the requested
    // size is <= 0 (see SDL2Renderer::SDL2Renderer in libcross2d/source/platforms/sdl2/sdl2_renderer.cpp).
    // Passing a fixed size like {1280, 720} here creates a small, non-fullscreen SDL window,
    // which on PS4 (no windowing system / no desktop compositor) never actually gets
    // presented to the TV output -> black screen, even though audio keeps working fine
    // since it's an independent subsystem. Switch ignores this parameter entirely and
    // forces its own {1280, 720} internally (see UiMain's __SWITCH__ constructor), so it's
    // safe to leave this at {0, 0} for every platform.
#if defined(__LINUX__) || defined(__WINDOWS__) || defined(__SWITCH__)
    pemu_ui = new PEMUUiMain(Vector2f{1280, 720});
#else
    pemu_ui = new PEMUUiMain(Vector2f{0, 0});
#endif
    pemu_ui->setIo(io);

    // load configuration
    constexpr int version = (__PEMU_VERSION_MAJOR__ * 100) + __PEMU_VERSION_MINOR__;
    const auto cfg = new PEMUConfig(pemu_ui, version);
    pemu_ui->setConfig(cfg);

    // load skin configuration
    const auto skin = new PEMUSkin(pemu_ui);
    pemu_ui->setSkin(skin);

    // parse command line ("program <rom>", or pppEmu's options, see skeleton/launcher.h)
    const LaunchArgs launchArgs = parseLaunchArgs(argc, argv);
    if (launchArgs.pppemu) {
        // started by the launcher, or coming back to it: keep the log of the previous program
        logKeepPrevious();
        logInit(io->getDataPath());
        logLine("main: started with%s%s%s%s%s%s",
                launchArgs.rom.empty() ? "" : " rom ", launchArgs.rom.c_str(),
                launchArgs.select.empty() ? "" : " select ", hidePassword(launchArgs.select).c_str(),
                launchArgs.returnTo.empty() ? "" : " return ", launchArgs.returnTo.c_str());
        logKeepPrevious();
    }
    // pppEmu: same display and controller settings as the program we come from
    if (launchArgs.pppemu) {
        const std::string settings = !launchArgs.settings.empty() ? launchArgs.settings
                                     : io->getDataPath() + "shared_settings.cfg";
        applySharedSettings(cfg, settings);
    }

    if (!launchArgs.rom.empty()) {
        const std::string rom = launchArgs.rom;
        if (io->exist(rom)) {
            game.path = Utility::baseName(rom);
            game.name = Utility::removeExt(game.path);
            game.romsPath = Utility::remove(rom, game.path);
        } else {
            printf("main: file provided as console argument does not exist (%s)\n", rom.c_str());
            logLine("main: rom not found: %s", rom.c_str());
            delete (skin);
            delete (cfg);
            delete (pemu_ui);
            // started by the launcher: go back to it
            returnToLauncher(launchArgs);
            return 1;
        }
    }

    // ui
    const auto romList = new PEMURomList(pemu_ui, cfg->getCoreVersion(), cfg->getCoreSupportedExt());
    if (game.path.empty()) {
        // pppEmu: back from a game run by another program, same list as before
        romList->useListCache = launchArgs.pppemu && launchArgs.rom.empty() && !launchArgs.select.empty();
        romList->build();
        romList->initFav();
    } else {
        delete (romList->rect);
    }
    const auto uiRomList = new PEMUUiRomList(pemu_ui, romList, pemu_ui->getSize());
    const auto uiMenu = new PEMUUiMenu(pemu_ui);
    const auto uiEmu = new PEMUUiEmu(pemu_ui);
    const auto uiState = new PEMUUiMenuState(pemu_ui);
    pemu_ui->init(uiRomList, uiMenu, uiEmu, uiState);

    // load specified game from command line if requested
    if (!game.path.empty()) {
        uiRomList->setVisibility(Visibility::Hidden);
        uiRomList->setGames({game});
        cfg->loadGame(game);
        uiEmu->setExitOnStop(true);
        // pppEmu demo mode: shown for a while, no saves written (see skeleton/demo.h)
        if (launchArgs.demoSeconds > 0) demo::begin(launchArgs.demoSeconds, launchArgs.demoSound);
        uiEmu->load(game);
    }

    // problems found while listing the games (network unreachable, no games...)
    if (game.path.empty() && !romList->warnings.empty()) {
        std::string message;
        for (const auto &w: romList->warnings) {
            pemu::logLine("main: warning: %s", w.c_str());
            message += (message.empty() ? "" : "\n\n") + w;
        }
        pemu_ui->getUiMessageBox()->show("WARNING", message, "OK");
    }

    // pppEmu: back from a game run by another program, select it again
    if (game.path.empty() && !launchArgs.select.empty()) {
        // same console page as before (L2 / R2)
        if (!launchArgs.system.empty()) {
            auto sysOpt = cfg->get(PEMUConfig::OptId::UI_FILTER_SYSTEM);
            const auto pages = sysOpt->getArray();
            if (std::find(pages.begin(), pages.end(), launchArgs.system) != pages.end()) {
                sysOpt->setArrayIndex(launchArgs.system);
                uiRomList->updateRomList();
            }
        }
        uiRomList->selectGame(launchArgs.select);
        // demo mode: the previous game's time is up, next one
        if (launchArgs.demoNext) uiRomList->startDemoLater();
    }

    pemu::logLine("main: ui ready, screen %ix%i", (int) pemu_ui->getSize().x, (int) pemu_ui->getSize().y);
    while (!pemu_ui->done) {
        pemu_ui->flip();
    }

    // pppEmu: going back to the launcher, with this program's settings (changed in its menu maybe)
    if (!launchArgs.returnTo.empty() && !launchArgs.settings.empty()) {
        writeSharedSettings(cfg, launchArgs.settings);
    }

    delete (skin);
    delete (cfg);
    delete (pemu_ui);

    // pppEmu: a game of another console was chosen, start its program (never returns on success)
    runPendingLaunch();
    // pppEmu: game closed in a program started by the launcher, go back to it
    returnToLauncher(launchArgs);

#ifdef  __PS4__
    sceSystemServiceLoadExec((char *) "exit", nullptr);
    while (true) {
    }
#endif

    return 0;
}
