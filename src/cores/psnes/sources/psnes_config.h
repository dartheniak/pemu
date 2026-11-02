//
// Created by cpasjuste on 29/05/18.
//

#ifndef PSNES_CONFIG_H
#define PSNES_CONFIG_H

#include "skeleton/pemu_config.h"
#include "snes9x.h"

class PSNESConfig final : public PEMUConfig {

public:
    PSNESConfig(Renderer *renderer, int version);

    // pppEmu: snes files in their own folder, like the other emulators ("/data/pppemu/psnes/")
    std::string getEmuDataPath() override {
        return p_renderer->getIo()->getDataPath() + "psnes/";
    }

    std::string getCoreVersion() override {
        return "snes9x: " + std::string(VERSION);
    }

    std::vector<std::string> getCoreSupportedExt() override {
        return {".zip", ".sfc", ".smc", ".swc", ".fig"};
    }

    // artwork and game information downloads (libretro thumbnails and database)
    std::string getLibretroSystem() override {
        return "Nintendo - Super Nintendo Entertainment System";
    }

    // network: snes folder names (No-Intro, EmulationStation/RetroArch and common names).
    // Compared ignoring case, spaces and punctuation.
    std::vector<std::string> getCoreFolderNames() override {
        return {
                "Nintendo - Super Nintendo Entertainment System",   // No-Intro
                "Super Nintendo Entertainment System",
                "Super Nintendo",
                "Nintendo SNES",
                "Super Famicom",
                "Nintendo - Super Famicom",
                "SFC",
                "SNES",     // last: shown with the first one in "folder not found" messages
        };
    }

#if defined(PPPEMU_GENESIS_PROGRAM) || defined(PPPEMU_NES_PROGRAM)
    // pppEmu: the system filter (L2 / R2 in the list) switches between consoles
    std::vector<int> getCoreHiddenOptionToEnable() override {
        return {UI_FILTER_SYSTEM};
    }

    // pppEmu: consoles on L2 / R2: master system, game gear, mega drive, sega cd, nes,
    // game boy, game boy color, game boy advance, super nintendo
    std::vector<int> getSystemOrder() override {
        return {2, 21, 1, 20, 3, 9, 10, 12, 4};
    }

    // pppEmu: super nintendo games run here, other consoles' games in other programs
    // shipped in the same package (see skeleton/launcher.h)
    std::vector<CoreSystem> getSystems() override {
        std::vector<CoreSystem> systems = {
                {getNetworkSystem(), getNetworkFolderNames(), getCoreSupportedExt(), getLibretroSystem(), ""},
        };
#ifdef PPPEMU_NES_PROGRAM
        systems.push_back({
                {3, 0, "NES"},
                {
                        "Nintendo - Nintendo Entertainment System",     // No-Intro
                        "Nintendo Entertainment System", "Nintendo NES",
                        "Nintendo - Family Computer", "Family Computer", "Famicom",
                        "FC", "NES",
                },
                {".zip", ".nes", ".unf", ".unif"},
                "Nintendo - Nintendo Entertainment System",
                PPPEMU_NES_PROGRAM
        });
#endif
#ifdef PPPEMU_GENESIS_PROGRAM
        // sega consoles: all run by the genesis program (Genesis Plus GX)
        systems.push_back({
                {1, 0, "Mega Drive"},
                {
                        "Sega - Mega Drive - Genesis",              // No-Intro
                        "Sega Mega Drive", "Sega Genesis",
                        "Mega Drive", "MegaDrive", "Genesis",
                        "Sega MD", "MD", "GEN", "Genesis - Mega Drive",
                },
                {".zip", ".md", ".smd", ".gen", ".bin", ".mdx"},
                "Sega - Mega Drive - Genesis",
                PPPEMU_GENESIS_PROGRAM
        });
        systems.push_back({
                {2, 0, "Master System"},
                {
                        "Sega - Master System - Mark III",          // No-Intro
                        "Sega Master System", "Master System", "MasterSystem",
                        "Sega Mark III", "Mark III", "SMS",
                },
                {".zip", ".sms"},
                "Sega - Master System - Mark III",
                PPPEMU_GENESIS_PROGRAM
        });
        systems.push_back({
                {21, 0, "Game Gear"},
                {
                        "Sega - Game Gear",                         // No-Intro
                        "Sega Game Gear", "Game Gear", "GameGear", "GG",
                },
                {".zip", ".gg"},
                "Sega - Game Gear",
                PPPEMU_GENESIS_PROGRAM
        });
        // cd images: no ".bin" here (the tracks of a ".cue" would be listed as games),
        // the bios files are needed in /data/pppemu/pgen/bios/ (bios_CD_U.bin, _E, _J)
        systems.push_back({
                {20, 0, "Sega CD"},
                {
                        "Sega - Mega-CD - Sega CD",                 // Redump / libretro
                        "Sega Mega-CD", "Sega Mega CD", "Mega-CD", "Mega CD", "MegaCD",
                        "Sega CD", "SegaCD", "SCD",
                },
                {".chd", ".cue", ".iso"},
                "Sega - Mega-CD - Sega CD",
                PPPEMU_GENESIS_PROGRAM
        });
#endif
        return systems;
    }
#endif
};

#endif //PSNES_CONFIG_H
