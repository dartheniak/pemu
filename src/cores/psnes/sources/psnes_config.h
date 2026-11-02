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
#ifdef PPPEMU_VERSION
        return "pppEmu " PPPEMU_VERSION " - snes9x " + std::string(VERSION);
#else
        return "snes9x: " + std::string(VERSION);
#endif
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

#if defined(PPPEMU_GENESIS_PROGRAM) || defined(PPPEMU_NES_PROGRAM) || defined(PPPEMU_GBA_PROGRAM) || defined(PPPEMU_ARCADE_PROGRAM)
    // pppEmu: the system filter (L2 / R2 in the list) switches between consoles
    std::vector<int> getCoreHiddenOptionToEnable() override {
        return {UI_FILTER_SYSTEM};
    }

    // pppEmu: consoles on L2 / R2: master system, game gear, mega drive, sega cd, nes,
    // game boy, game boy color, game boy advance, super nintendo, arcade
    std::vector<int> getSystemOrder() override {
        return {2, 21, 1, 20, 3, 9, 10, 12, 4, 75};
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
#ifdef PPPEMU_GBA_PROGRAM
        // game boy consoles: all run by the game boy program (mGBA). The gba bios is optional
        // (/data/pppemu/pgba/bios/gba_bios.bin), mGBA has its own replacement.
        systems.push_back({
                {9, 0, "Game Boy"},
                {
                        "Nintendo - Game Boy",                      // No-Intro
                        "Nintendo Game Boy", "Game Boy", "GameBoy", "GB",
                },
                {".zip", ".gb"},
                "Nintendo - Game Boy",
                PPPEMU_GBA_PROGRAM
        });
        systems.push_back({
                {10, 0, "Game Boy Color"},
                {
                        "Nintendo - Game Boy Color",                // No-Intro
                        "Nintendo Game Boy Color", "Game Boy Color", "GameBoy Color", "GBC",
                },
                {".zip", ".gbc", ".gb"},
                "Nintendo - Game Boy Color",
                PPPEMU_GBA_PROGRAM
        });
        systems.push_back({
                {12, 0, "Game Boy Advance"},
                {
                        "Nintendo - Game Boy Advance",              // No-Intro
                        "Nintendo Game Boy Advance", "Game Boy Advance", "GameBoy Advance", "GBA",
                },
                {".zip", ".gba"},
                "Nintendo - Game Boy Advance",
                PPPEMU_GBA_PROGRAM
        });
#endif
#ifdef PPPEMU_ARCADE_PROGRAM
        // arcade: all in one page (cps, neo geo...), run by the arcade program (FBNeo).
        // Romset names from the list made at build time (fbneo_arcade.tsv, see romset_list.h)
        systems.push_back({
                {75, 0, "Arcade"},
                {
                        "FBNeo - Arcade Games",                     // libretro
                        "FinalBurn Neo", "Final Burn Neo", "FBNeo", "FB Neo", "FBA",
                        "Arcade", "Arcades", "MAME",
                        "SNK - Neo Geo", "Neo Geo", "NeoGeo", "Neo-Geo",
                        "CPS1", "CPS2", "CPS3",
                },
                {".zip"},
                "FBNeo - Arcade Games",
                PPPEMU_ARCADE_PROGRAM,
                p_renderer->getIo()->getRomFsPath() + "fbneo_arcade.tsv"
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
