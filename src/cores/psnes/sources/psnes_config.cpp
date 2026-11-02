//
// Created by cpasjuste on 29/05/18.
//

#include <unistd.h>
#include "skeleton/pemu.h"
#include "psnes_config.h"

using namespace c2d;
using namespace pemu;

PSNESConfig::PSNESConfig(Renderer *renderer, int version) : PEMUConfig(renderer, "PSNES", version) {
    printf("PSNESConfig(%s, v%i)\n", getPath().c_str(), version);

    c2d::Io *io = renderer->getIo();

    // pppEmu: snes files used to be in the data folder itself, move them to "psnes/" once.
    // File by file, a file already in "psnes/" is never overwritten (the old one stays).
    const std::string emuDir = io->getDataPath() + "psnes/";
    io->create(emuDir);
    for (const char *dir: {"sram", "saves", "configs", "cheat", "screenshots", "spc", "patch", "bios", "log"}) {
        const std::string from = io->getDataPath() + dir + "/";
        const std::string to = emuDir + dir + "/";
        if (io->getType(from) != c2d::Io::Type::Directory) continue;
        io->create(to);
        int moved = 0, kept = 0;
        for (const auto &f: io->getDirList(from)) {
            if (f.name.empty() || f.name == "." || f.name == "..") continue;
            if (!io->exist(to + f.name) && rename((from + f.name).c_str(), (to + f.name).c_str()) == 0) moved++;
            else kept++;
        }
        // empty now: removed, so it's not done again
        if (kept == 0) rmdir(from.c_str());
        printf("PSNESConfig: %s -> %s: %i moved, %i kept\n", from.c_str(), to.c_str(), moved, kept);
    }

    const auto group = getGroup(EMULATION);
    if (!group) {
        printf("PSNESConfig: error, group not found (MENU_ROM_OPTIONS)\n");
        return;
    }

    group->addOption({"AUDIO_SYNC", {"OFF", "ON"}, 0, EMU_AUDIO_SYNC,
                      "ON: PERFECT AUDIO - OFF: MINOR AUDIO STUTTERING (FAVOR FPS)"});
    group->addOption({"CHEATS", {"OFF", "ON"}, 1, EMU_PSNES_CHEATS});
    group->addOption({"BLOCk_INVALID_VRAM", {"OFF", "ON"}, 1, EMU_PSNES_BLOCK_VRAM});
    group->addOption({"TRANSPARENCY", {"OFF", "ON"}, 1, EMU_PSNES_TRANSPARENCY});
    group->addOption({"DISPLAY_MESSAGES", {"OFF", "ON"}, 1, EMU_PSNES_DISPLAY_MESSAGES});
    group->addOption({"FRAMESKIP",
                      {"OFF", "AUTO", "1", "2", "3", "4", "5", "6", "7", "8", "9"},
                      0, EMU_PSNES_FRAMESKIP});
#ifdef __VITA__
    get(EMU_PSNES_FRAMESKIP)->setArrayIndex(3);
#endif
    group->addOption({"TURBO_MODE", {"OFF", "ON"}, 0, EMU_PSNES_TURBO_MODE});
    group->addOption({"TURBO_FRAMESKIP",
                      {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
                       "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25"},
                      15, EMU_PSNES_TURBO_FRAMESKIP});

    // no need for auto-scaling mode
    getOption(EMU_SCALING_MODE)->setArray({"ASPECT", "INTEGER"}, 0);

    // "romlist.cpp" (RomList::build) will also reload config, but we need new roms paths
    PEMUConfig::load();

    // add custom rom path
    addRomPath("SNES", io->getDataPath() + "roms/", {4, 0, "Super Nintendo"});
    PEMUConfig::save();

    // create roms paths if needed
    const auto paths = getRomPaths();
    for (const auto &path: paths) {
        // network paths (smb://) are not local directories
        if (Utility::startWith(Utility::toLower(path.path), "smb://")) continue;
        io->create(path.path);
    }
}
