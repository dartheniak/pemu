//
// Created by cpasjuste on 03/04/2020.
//

#ifndef PEMU_PSNES_IO_H
#define PEMU_PSNES_IO_H

namespace c2d {
    class PSNESIo : public c2d::C2DIo {
    public:
        PSNESIo() : C2DIo() {
            C2DIo::create(PSNESIo::getDataPath());
            C2DIo::create(PSNESIo::getDataPath() + "roms");
            // snes saves, save states and per-game settings: "psnes/" (see PSNESConfig)
            C2DIo::create(PSNESIo::getDataPath() + "psnes");
            C2DIo::create(PSNESIo::getDataPath() + "psnes/configs");
            C2DIo::create(PSNESIo::getDataPath() + "psnes/saves");
        }

#ifdef __PSP2__
        std::string getDataPath() override {
            return "ux0:/data/psnes/";
        }
#elif __PS4__
        // pppEmu: own data folder, never shared with the official pSNES
        std::string getDataPath() override {
            return "/data/pppemu/";
        }
#elif __3DS__
#ifndef NDEBUG
        std::string getDataPath() override {
            return "/3ds/psnes/";
        }
#endif
#elif __SWITCH__
#if __DEBUG_SVC__
        std::string getDataPath() override {
            return "/switch/psnes/";
        }
#endif
#endif
    };
}

#endif //PEMU_PSNES_IO_H
