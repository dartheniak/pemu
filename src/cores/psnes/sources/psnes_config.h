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

#ifdef PPPEMU_GENESIS_PROGRAM
    // pppEmu: the system filter (L2 / R2 in the list) switches between consoles
    std::vector<int> getCoreHiddenOptionToEnable() override {
        return {UI_FILTER_SYSTEM};
    }

    // pppEmu: super nintendo games run here, mega drive / genesis, master system and game gear
    // games in the genesis program shipped in the same package (see skeleton/launcher.h)
    std::vector<CoreSystem> getSystems() override {
        return {
                {getNetworkSystem(), getNetworkFolderNames(), getCoreSupportedExt(), getLibretroSystem(), ""},
                {
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
                },
                // also run by the genesis program (Genesis Plus GX)
                {
                        {2, 0, "Master System"},
                        {
                                "Sega - Master System - Mark III",          // No-Intro
                                "Sega Master System", "Master System", "MasterSystem",
                                "Sega Mark III", "Mark III", "SMS",
                        },
                        {".zip", ".sms"},
                        "Sega - Master System - Mark III",
                        PPPEMU_GENESIS_PROGRAM
                },
                {
                        {21, 0, "Game Gear"},
                        {
                                "Sega - Game Gear",                         // No-Intro
                                "Sega Game Gear", "Game Gear", "GameGear", "GG",
                        },
                        {".zip", ".gg"},
                        "Sega - Game Gear",
                        PPPEMU_GENESIS_PROGRAM
                },
        };
    }
#endif
};

#endif //PSNES_CONFIG_H
