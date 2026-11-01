//
// Created by cpasjuste on 13/09/23.
//

#ifndef PEMU_CONFIG_H
#define PEMU_CONFIG_H

#define PEMU_CONFIG_RESTART_NEEDED "YOU NEED TO RESTART THE APPLICATION AFTER CHANGING THIS OPTION"

namespace pemu {
    class PEMUConfig : public c2d::config::Config {
    public:
        enum Flags {
            INPUT = BIT(1),
            HIDDEN = BIT(2)
        };

        enum GrpId {
            UI_FILTERING = 0,
            UI_OPTIONS,
            EMULATION,
            GAMEPAD,
            KEYBOARD,
            NETWORK,
            ARTWORK
        };

        enum OptId {
            UI_SHOW_ZIP_NAMES = 0,
            UI_FULLSCREEN,
            UI_SKIN,
            UI_SKIN_ASPECT,
            UI_FONT_SCALING,
            UI_VIDEO_SNAP_DELAY,
            UI_FILTER_FAVORITES,
            UI_FILTER_AVAILABLE,
            UI_FILTER_CLONES,
            UI_FILTER_SYSTEM,
            UI_FILTER_EDITOR,
            UI_FILTER_DEVELOPER,
            UI_FILTER_PLAYERS,
            UI_FILTER_RATING,
            UI_FILTER_DATE,
            UI_FILTER_GENRE,
            EMU_SCALING,
            EMU_SCALING_MODE,
            EMU_FILTER,
            EMU_SHADER,
#ifdef __VITA__
            EMU_WAIT_RENDERING,
#endif
            EMU_FORCE_60HZ,
            EMU_AUDIO_SYNC,
            EMU_AUDIO_FREQ,
            EMU_AUDIO_INTERPOLATION,
            EMU_AUDIO_FMINTERPOLATION,
            EMU_ROTATION,
            EMU_SHOW_FPS,
            EMU_FRAMESKIP,
            EMU_NEOBIOS,
            EMU_PSNES_CHEATS,
            EMU_PSNES_BLOCK_VRAM,
            EMU_PSNES_TRANSPARENCY,
            EMU_PSNES_DISPLAY_MESSAGES,
            EMU_PSNES_FRAMESKIP,
            EMU_PSNES_TURBO_MODE,
            EMU_PSNES_TURBO_FRAMESKIP,
            NET_SMB_SERVER,
            NET_SMB_FOLDER,
            NET_SMB_USER,
            NET_SMB_PASSWORD,
            NET_SMB_CLEAR_CACHE,
            NET_SMB_SYSTEM_FOLDER,
            ART_IMAGE_TYPE,
            ART_AUTO_DOWNLOAD,
            JOY_UP,
            JOY_DOWN,
            JOY_LEFT,
            JOY_RIGHT,
            JOY_A,
            JOY_B,
            JOY_X,
            JOY_Y,
            JOY_LT,
            JOY_RT,
            JOY_LB,
            JOY_RB,
            JOY_SELECT,
            JOY_START,
            JOY_MENU1,
            JOY_MENU2,
            JOY_AXIS_LX,
            JOY_AXIS_LY,
            JOY_AXIS_RX,
            JOY_AXIS_RY,
            JOY_DEADZONE,
#ifndef NO_KEYBOARD
            KEY_UP,
            KEY_DOWN,
            KEY_LEFT,
            KEY_RIGHT,
            KEY_A,
            KEY_B,
            KEY_X,
            KEY_Y,
            KEY_LT,
            KEY_RT,
            KEY_LB,
            KEY_RB,
            KEY_SELECT,
            KEY_START,
            KEY_MENU1,
            KEY_MENU2,
#endif
            END
        };

        struct RomPath {
            std::string path;
            ss_api::System system;
        };

        PEMUConfig(c2d::Renderer *renderer, const std::string &name, int version = 1);

        ~PEMUConfig() override;

        bool loadGame(const ss_api::Game &game);

        bool saveGame();

        c2d::config::Option *get(int id, bool isGame = false);

        c2d::config::Option *getOption(int id, bool isGame = false);

        bool addRomPath(const std::string &name, const std::string &path, const ss_api::System &system);

        std::vector<RomPath> getRomPaths();

        // smb:// url built from the NETWORK settings (empty if no server is set)
        std::string getNetworkRomPath();

        // system used for games found on the network rom path (first configured rom path system)
        virtual ss_api::System getNetworkSystem();

        // folder names this core's games are stored in ("snes", "Nintendo - Super Nintendo...").
        // When the network FOLDER is a folder of several consoles ("Roms"), only its sub folders
        // with one of these names are scanned. Empty: the FOLDER is scanned as is.
        virtual std::vector<std::string> getCoreFolderNames() { return {}; }

        // libretro system name for artwork and game information downloads
        // ("Nintendo - Super Nintendo Entertainment System"). Empty: no artwork downloads.
        virtual std::string getLibretroSystem() { return ""; }

        // getCoreFolderNames() + the user's SYSTEM_FOLDER setting
        std::vector<std::string> getNetworkFolderNames();

        std::vector<c2d::Input::ButtonMapping> getKeyboardMapping(int player, bool isGame = false);

        std::vector<c2d::Input::ButtonMapping> getJoystickMapping(int player, bool isGame = false);

        c2d::Vector2i getJoystickAxisLeftMapping(int player, bool isGame = false);

        c2d::Vector2i getJoystickAxisRightMapping(int player, bool isGame = false);

        int getJoystickDeadZone(int player, bool isGame = false);

        // custom cores overrides
        virtual std::string getCoreVersion() { return "UNKNOWN"; }

        virtual std::vector<std::string> getCoreSupportedExt() { return {".zip"}; }

        virtual std::vector<int> getCoreHiddenOptionToEnable() { return {}; }

    private:
        Config *p_game_config = nullptr;
        c2d::Renderer *p_renderer = nullptr;
    };
}

#define CFG_ID_ROMS (PEMUConfig::OptId::END + 1)

#endif //PEMU_CONFIG_H
