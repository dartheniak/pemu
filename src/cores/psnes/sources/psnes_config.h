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
};

#endif //PSNES_CONFIG_H
