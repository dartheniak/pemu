//
// Created by cpasjuste on 29/05/18.
//

#include "skeleton/pemu.h"
#include "pgba_config.h"

using namespace c2d;
using namespace pemu;

PGBAConfig::PGBAConfig(Renderer *renderer, const int version) : PEMUConfig(renderer, "PGBA", version) {
    c2d::Io *io = renderer->getIo();

    // no need for auto-scaling mode
    getOption(PEMUConfig::OptId::EMU_SCALING_MODE)->setArray({"ASPECT", "INTEGER"}, 0);

    // pppEmu: screen colours of original game boy games (see pgba_ui_emu.cpp), before the
    // config is loaded (or the saved value is not read)
    if (const auto group = getGroup(EMULATION)) {
        group->addOption({"GB_COLORS", {"WHITE", "CLASSIC GREEN", "LIGHT BLUE", "PINK"}, 0,
                          EMU_PGBA_GB_COLORS, "SCREEN COLOURS OF ORIGINAL GAME BOY GAMES"});
    }

    // "romlist.cpp" (RomList::build) will also reload config, but we need new roms paths
    PEMUConfig::load();

    // add custom rom path
    PEMUConfig::addRomPath("GB", io->getDataPath() + "gb/", {10, 0, "Game Boy"});
    PEMUConfig::addRomPath("GBC", io->getDataPath() + "gbc/", {11, 0, "Game Boy Color"});
    PEMUConfig::addRomPath("GBA", io->getDataPath() + "gba/", {12, 0, "Game Boy Advance"});
    PEMUConfig::save();

    // create roms paths if needed
    auto paths = getRomPaths();
    for (const auto &path: paths) {
        io->create(path.path);
    }

}
