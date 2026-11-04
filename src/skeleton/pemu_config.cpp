//
// Created by cpasjuste on 13/09/23.
//

#include "pemu.h"
#include "secret.h"
#include "pemu_config.h"

using namespace c2d::config;
using namespace ss_api;

PEMUConfig::PEMUConfig(Renderer *renderer, const std::string &name, int version)
    : Config(name, renderer->getIo()->getDataPath() + "config.cfg", version) {
    p_renderer = renderer;

    /*
     * ROMS PATHS
     */
    Group roms("ROMS", CFG_ID_ROMS);
    addGroup(roms);

    /// UI_FILTERING
    Group ui_filtering("UI_FILTERING", UI_FILTERING);
    ui_filtering.addOption({"FILTER_FAVORITES", {"OFF", "ON"}, 0, UI_FILTER_FAVORITES});
    ui_filtering.addOption({"FILTER_MISSING", {"OFF", "ON"}, 1, UI_FILTER_AVAILABLE});
    ui_filtering.addOption({"FILTER_CLONES", {"OFF", "ON"}, 1, UI_FILTER_CLONES})->setFlags(HIDDEN);
    ui_filtering.addOption({"FILTER_SYSTEM", {"ALL"}, 0, UI_FILTER_SYSTEM})->setFlags(HIDDEN);
    ui_filtering.addOption({"FILTER_GENRE", {"ALL"}, 0, UI_FILTER_GENRE});
    ui_filtering.addOption({"FILTER_DATE", {"ALL"}, 0, UI_FILTER_DATE});
    ui_filtering.addOption({"FILTER_EDITOR", {"ALL"}, 0, UI_FILTER_EDITOR});
    ui_filtering.addOption({"FILTER_DEVELOPER", {"ALL"}, 0, UI_FILTER_DEVELOPER});
    ui_filtering.addOption({"FILTER_PLAYERS", {"ALL"}, 0, UI_FILTER_PLAYERS});
    ui_filtering.addOption({"FILTER_RATING", {"ALL"}, 0, UI_FILTER_RATING});
    addGroup(ui_filtering);

    /// UI_OPTIONS
    Group main("UI_OPTIONS", UI_OPTIONS);
    main.addOption({"SHOW_ZIP_NAMES", {"OFF", "ON"}, 1, UI_SHOW_ZIP_NAMES});
#ifdef __FULLSCREEN__
    main.addOption({"FULLSCREEN", {"OFF", "ON"}, 1, UI_FULLSCREEN, PEMU_CONFIG_RESTART_NEEDED});
#endif
    // pppEmu: no 4/3 skin layout (SKIN_ASPECT removed), the 16/9 one is always used
    Vector2i displaySize = C2DDevice::getDisplaySize();
    main.addOption({
        "FONT_SCALING", {"0", "1", "2", "3", "4", "5"}, 0,
        UI_FONT_SCALING, PEMU_CONFIG_RESTART_NEEDED
    });
    main.addOption({"VIDEO_SNAP_DELAY", {"3", "5", "7", "10"}, 1, UI_VIDEO_SNAP_DELAY});
    // pppEmu: borders in the kiwi logo colours, flowing (OFF: still), see border_fx.h
    main.addOption({"BORDER_ANIMATION", {"OFF", "SLOW", "NORMAL", "FAST"}, 2, UI_BORDER_ANIMATION});

    // build  skin list
    std::vector<std::string> skins;
    // add default skins from romfs
    skins.emplace_back("default");
    // add skins from romfs dir
    auto files = p_renderer->getIo()->getDirList(p_renderer->getIo()->getRomFsPath() + "skins/", true);
    for (auto &file: files) {
        if (file.type != c2d::Io::Type::Directory || file.name[0] == '.') {
            continue;
        }
        // only append skin name if not found yet
        if (std::find(skins.begin(), skins.end(), file.name) == skins.end()) {
            skins.emplace_back(file.name);
            printf("skin found: %s\n", file.path.c_str());
        }
    }
    // add skins from data dir
    files = p_renderer->getIo()->getDirList(p_renderer->getIo()->getDataPath() + "skins/", true);
    for (auto &file: files) {
        if (file.type != c2d::Io::Type::Directory || file.name[0] == '.') {
            continue;
        }
        // only append skin name if not found yet
        if (std::find(skins.begin(), skins.end(), file.name) == skins.end()) {
            skins.emplace_back(file.name);
            printf("skin found: %s\n", file.path.c_str());
        }
    }
    // set "default" skin index
    int index = 0;
    for (size_t i = 0; i < skins.size(); i++) {
        if (skins.at(i) == "default") {
            index = (int) i;
        }
    }
    main.addOption({"SKIN", skins, index, UI_SKIN});
    addGroup(main);

#ifdef __SMB__
    /// NETWORK (roms on a smb / windows share)
    Group network("NETWORK", NETWORK);
    network.addOption({"SERVER", std::string(""), NET_SMB_SERVER,
                       "PC OR NAS ADDRESS (EXAMPLE: 192.168.1.20)"});
    network.addOption({"FOLDER", std::string(""), NET_SMB_FOLDER,
                       "YOUR ROMS FOLDER (EXAMPLE: ROMS) - THE CONSOLE'S SUB FOLDER IS FOUND BY NAME"});
    network.addOption({"USERNAME", std::string(""), NET_SMB_USER,
                       "LEAVE EMPTY FOR GUEST ACCESS"});
    network.addOption({"PASSWORD", std::string(""), NET_SMB_PASSWORD});
    network.addOption({"CLEAR_CACHE_ON_START", {"OFF", "ON"}, 1, NET_SMB_CLEAR_CACHE,
                       "ON: DELETE DOWNLOADED NETWORK GAMES WHEN THE APP STARTS"});
    network.addOption({"SYSTEM_FOLDER", std::string(""), NET_SMB_SYSTEM_FOLDER,
                       "OPTIONAL: NAME OF THIS CONSOLE'S FOLDER IF IT IS NOT FOUND AUTOMATICALLY"});
    addGroup(network);
#endif

    /// ARTWORK (libretro thumbnails + game information, stored in <data>/media)
    Group artwork("ARTWORK", ARTWORK);
    artwork.addOption({"IMAGE", {"BOXART", "TITLE", "SNAP"}, 0, ART_IMAGE_TYPE,
                       "IMAGE SHOWN FOR EACH GAME (BOX, TITLE SCREEN OR IN-GAME SCREENSHOT)"});
    artwork.addOption({"AUTO_DOWNLOAD", {"OFF", "ON"}, 1, ART_AUTO_DOWNLOAD,
                       "ON: DOWNLOAD THE IMAGE OF THE SELECTED GAME WHILE BROWSING"});
    addGroup(artwork);

    /// DEMO (attract mode: random games shown one after the other, see demo.h)
    Group demo("DEMO_MODE", DEMO);
    demo.addOption({"START_AFTER", {"OFF", "1 MIN", "2 MIN", "5 MIN", "10 MIN"}, 3, DEMO_START_AFTER,
                    "IDLE TIME IN THE LIST BEFORE THE DEMO STARTS (L3 + R3: START IT NOW)"});
    // pppEmu 1.06: was TIME_PER_GAME (1 MIN): new name, so the new default (2 MIN) is used
    demo.addOption({"GAME_TIME", {"30 SEC", "1 MIN", "2 MIN", "5 MIN", "10 MIN"}, 2, DEMO_TIME,
                    "HOW LONG EACH GAME IS SHOWN"});
    demo.addOption({"GAMES", {"ALL", "CURRENT PAGE", "FAVORITES"}, 0, DEMO_GAMES,
                    "GAMES SHOWN: ALL, THE CONSOLE PAGE (L2 / R2) OR THE FAVORITES"});
    demo.addOption({"SOUND", {"OFF", "ON"}, 1, DEMO_SOUND});
    addGroup(demo);

    /*
     * Emulators options
     */
    Group emu_grp("EMULATION", EMULATION);
    if (displaySize.y > 1080) {
        emu_grp.addOption(
            {
                "SCALING", {"NONE", "2X", "3X", "4X", "5X", "6X", "7X", "8X", "9X", "FIT", "FULL"}, 9,
                EMU_SCALING
            });
    } else if (displaySize.y > 720) {
        emu_grp.addOption({"SCALING", {"NONE", "2X", "3X", "4X", "FIT", "FULL"}, 4, EMU_SCALING});
    } else if (displaySize.y > 544) {
        emu_grp.addOption({"SCALING", {"NONE", "2X", "3X", "FIT", "FULL"}, 3, EMU_SCALING});
    } else if (displaySize.y > 240) {
        emu_grp.addOption({"SCALING", {"NONE", "2X", "FIT", "FULL"}, 2, EMU_SCALING});
    } else {
        emu_grp.addOption({"SCALING", {"NONE", "FIT", "FULL"}, 1, EMU_SCALING});
    }

    emu_grp.addOption({"SCALING_MODE", {"AUTO", "ASPECT", "INTEGER"}, 1, EMU_SCALING_MODE});
    emu_grp.addOption({"FILTER", {std::string("POINT"), std::string("LINEAR")}, 0, EMU_FILTER});
    if (const auto shaderList = p_renderer->getShaderList()) {
        emu_grp.addOption({"EFFECT", shaderList->getNames(), 0, EMU_SHADER});
    } else {
        emu_grp.addOption({"EFFECT", {"c2d-texture"}, 0, EMU_SHADER})->setFlags(HIDDEN);
    }
#ifdef __VITA__
    emu_grp.addOption({"WAIT_RENDERING", {"OFF", "ON"}, 1, EMU_WAIT_RENDERING});
#endif
    emu_grp.addOption({"SHOW_FPS", {"OFF", "ON"}, 0, EMU_SHOW_FPS});
    addGroup(emu_grp);

    /*
     * Inputs options
     */
    Group joy_grp("GAMEPAD", GAMEPAD);
    joy_grp.addOption({"JOY_UP", KEY_JOY_UP_DEFAULT, JOY_UP})->setFlags(INPUT);
    joy_grp.addOption({"JOY_DOWN", KEY_JOY_DOWN_DEFAULT, JOY_DOWN})->setFlags(INPUT);
    joy_grp.addOption({"JOY_LEFT", KEY_JOY_LEFT_DEFAULT, JOY_LEFT})->setFlags(INPUT);
    joy_grp.addOption({"JOY_RIGHT", KEY_JOY_RIGHT_DEFAULT, JOY_RIGHT})->setFlags(INPUT);
    joy_grp.addOption({"JOY_A", KEY_JOY_A_DEFAULT, JOY_A})->setFlags(INPUT);
    joy_grp.addOption({"JOY_B", KEY_JOY_B_DEFAULT, JOY_B})->setFlags(INPUT);
    joy_grp.addOption({"JOY_X", KEY_JOY_X_DEFAULT, JOY_X})->setFlags(INPUT);
    joy_grp.addOption({"JOY_Y", KEY_JOY_Y_DEFAULT, JOY_Y})->setFlags(INPUT);
    joy_grp.addOption({"JOY_LT", KEY_JOY_LT_DEFAULT, JOY_LT})->setFlags(INPUT);
    joy_grp.addOption({"JOY_RT", KEY_JOY_RT_DEFAULT, JOY_RT})->setFlags(INPUT);
    joy_grp.addOption({"JOY_LB", KEY_JOY_LB_DEFAULT, JOY_LB})->setFlags(INPUT);
    joy_grp.addOption({"JOY_RB", KEY_JOY_RB_DEFAULT, JOY_RB})->setFlags(INPUT);
    joy_grp.addOption({"JOY_SELECT", KEY_JOY_SELECT_DEFAULT, JOY_SELECT})->setFlags(INPUT);
    joy_grp.addOption({"JOY_START", KEY_JOY_START_DEFAULT, JOY_START})->setFlags(INPUT);
    joy_grp.addOption({"JOY_MENU1", KEY_JOY_MENU1_DEFAULT, JOY_MENU1})->setFlags(INPUT);
    joy_grp.addOption({"JOY_MENU2", KEY_JOY_MENU2_DEFAULT, JOY_MENU2})->setFlags(INPUT);
    joy_grp.addOption({"JOY_AXIS_LX", KEY_JOY_AXIS_LX, JOY_AXIS_LX})->setFlags(INPUT | HIDDEN);
    joy_grp.addOption({"JOY_AXIS_LY", KEY_JOY_AXIS_LY, JOY_AXIS_LY})->setFlags(INPUT | HIDDEN);
    joy_grp.addOption({"JOY_AXIS_RX", KEY_JOY_AXIS_RX, JOY_AXIS_RX})->setFlags(INPUT | HIDDEN);
    joy_grp.addOption({"JOY_AXIS_RY", KEY_JOY_AXIS_RY, JOY_AXIS_RY})->setFlags(INPUT | HIDDEN);
    joy_grp.addOption({
        "JOY_DEADZONE", {
            "2000", "4000", "6000", "8000", "10000", "12000", "14000", "16000",
            "18000", "20000", "22000", "24000", "26000", "28000", "30000"
        },
        3, JOY_DEADZONE
    });
    addGroup(joy_grp);
#ifndef NO_KEYBOARD
    Group kb_grp("KEYBOARD", KEYBOARD);
    kb_grp.addOption({"KEY_UP", KEY_KB_UP_DEFAULT, KEY_UP})->setFlags(INPUT);
    kb_grp.addOption({"KEY_DOWN", KEY_KB_DOWN_DEFAULT, KEY_DOWN})->setFlags(INPUT);
    kb_grp.addOption({"KEY_LEFT", KEY_KB_LEFT_DEFAULT, KEY_LEFT})->setFlags(INPUT);
    kb_grp.addOption({"KEY_RIGHT", KEY_KB_RIGHT_DEFAULT, KEY_RIGHT})->setFlags(INPUT);
    kb_grp.addOption({"KEY_A", KEY_KB_A_DEFAULT, KEY_A})->setFlags(INPUT);
    kb_grp.addOption({"KEY_B", KEY_KB_B_DEFAULT, KEY_B})->setFlags(INPUT);
    kb_grp.addOption({"KEY_X", KEY_KB_X_DEFAULT, KEY_X})->setFlags(INPUT);
    kb_grp.addOption({"KEY_Y", KEY_KB_Y_DEFAULT, KEY_Y})->setFlags(INPUT);
    kb_grp.addOption({"KEY_LT", KEY_KB_LT_DEFAULT, KEY_LT})->setFlags(INPUT);
    kb_grp.addOption({"KEY_RT", KEY_KB_RT_DEFAULT, KEY_RT})->setFlags(INPUT);
    kb_grp.addOption({"KEY_LB", KEY_KB_LB_DEFAULT, KEY_LB})->setFlags(INPUT);
    kb_grp.addOption({"KEY_RB", KEY_KB_RB_DEFAULT, KEY_RB})->setFlags(INPUT);
    kb_grp.addOption({"KEY_SELECT", KEY_KB_SELECT_DEFAULT, KEY_SELECT})->setFlags(INPUT);
    kb_grp.addOption({"KEY_START", KEY_KB_START_DEFAULT, KEY_START})->setFlags(INPUT);
    kb_grp.addOption({"KEY_MENU1", KEY_KB_MENU1_DEFAULT, KEY_MENU1})->setFlags(INPUT);
    kb_grp.addOption({"KEY_MENU2", KEY_KB_MENU2_DEFAULT, KEY_MENU2})->setFlags(INPUT);
    addGroup(kb_grp);
#endif
}

bool PEMUConfig::loadGame(const Game &game) {
    Group group;

    // free game config if needed
    delete (p_game_config);

    // create game configuration
    p_renderer->getIo()->create(getEmuDataPath() + "configs");
    std::string path = getEmuDataPath() + "configs/" + Utility::removeExt(Utility::baseName(game.path)) + ".cfg";
    p_game_config = new Config(getName() + "_GAME", path, getAppVersion());

    // copy game emulation config section from main config for default options
    getGroup(EMULATION)->copy(&group);
    p_game_config->addGroup(group);

    // copy game gamepad config from main config for default options
    getGroup(GAMEPAD)->copy(&group);
    p_game_config->addGroup(group);

#ifndef NO_KEYBOARD
    // copy game keyboard config from main config for default options
    getGroup(KEYBOARD)->copy(&group);
    p_game_config->addGroup(group);
#endif

    // pppEmu: names of the options changed for this game ("EMULATION/FILTER,GAMEPAD/JOY_A")
    Group changed(GAME_CHANGED_GROUP, GAME_CHANGED_GROUP_ID);
    changed.addOption({GAME_CHANGED_OPTION, std::string(""), GAME_CHANGED_OPTION_ID});
    p_game_config->addGroup(changed);

    p_game_config->load();

    // the other options follow the global settings: a global change applies to every game,
    // except the games where that option was changed (older files: no list, all global)
    const std::string list = "," + p_game_config->getOption(GAME_CHANGED_OPTION_ID)->getString() + ",";
    int kept = 0;
    for (const int id: {EMULATION, GAMEPAD, KEYBOARD}) {
        Group *main = getGroup(id), *game = p_game_config->getGroup(id);
        if (!main || !game) continue;
        for (auto &opt: *game->getOptions()) {
            Option *global = main->getOption(opt.getName());
            if (!global) continue;
            if (list.find("," + game->getName() + "/" + opt.getName() + ",") != std::string::npos) {
                kept++;
                continue;
            }
            copyOptionValue(global, &opt);
        }
    }
    printf("PEMUConfig::loadGame: %i option(s) changed for this game\n", kept);

    return true;
}

bool PEMUConfig::saveGame() {
    if (!p_game_config) return false;

    // pppEmu: remember which options differ from the global settings (changed for this game)
    std::string list;
    for (const int id: {EMULATION, GAMEPAD, KEYBOARD}) {
        Group *main = getGroup(id), *game = p_game_config->getGroup(id);
        if (!main || !game) continue;
        for (auto &opt: *game->getOptions()) {
            Option *global = main->getOption(opt.getName());
            if (global && !sameOptionValue(global, &opt)) {
                list += (list.empty() ? "" : ",") + game->getName() + "/" + opt.getName();
            }
        }
    }
    if (Option *o = p_game_config->getOption(GAME_CHANGED_OPTION_ID)) o->setString(list);

    return p_game_config->save();
}

void PEMUConfig::copyOptionValue(Option *from, Option *to) {
    switch (from->getType()) {
        case Option::Type::Integer:
            to->setInteger(from->getInteger());
            break;
        case Option::Type::Float:
            to->setFloat(from->getFloat());
            break;
        case Option::Type::Array:
            to->setArrayIndex(from->getArrayIndex());
            break;
        case Option::Type::String:
            to->setString(from->getString());
            break;
        default:
            break;
    }
}

bool PEMUConfig::sameOptionValue(Option *a, Option *b) {
    switch (a->getType()) {
        case Option::Type::Integer:
            return a->getInteger() == b->getInteger();
        case Option::Type::Float:
            return a->getFloat() == b->getFloat();
        case Option::Type::Array:
            return a->getArrayIndex() == b->getArrayIndex();
        case Option::Type::String:
            return a->getString() == b->getString();
        default:
            return true;
    }
}

bool PEMUConfig::addRomPath(const std::string &name, const std::string &path, const ss_api::System &system) {
    printf("PEMUConfig::addRomPath: %s (system: %s, path: %s)\n",
           name.c_str(), system.name.c_str(), path.c_str());
    auto roms = config_lookup(libConfigGetInstance(), std::string(getName() + ".ROMS").c_str());
    if (!roms) {
        // first launch: no config file yet, so no "ROMS" section in memory and the rom path
        // couldn't be added (upstream "roms not detected on first launch" bug). Create it.
        Config::save();
        roms = config_lookup(libConfigGetInstance(), std::string(getName() + ".ROMS").c_str());
        if (!roms) {
            printf("PEMUConfig::addRomPath: could not create the ROMS section\n");
            return false;
        }
    }
    if (config_setting_get_member(roms, name.c_str())) {
        printf("PEMUConfig::addRomPath: %s found in config file, skipping\n", name.c_str());
        return false;
    }

    auto setting = config_setting_add(roms, name.c_str(), CONFIG_TYPE_LIST);
    if (setting) {
        // add system name
        auto opt = config_setting_add(setting, "SYSTEM_NAME", CONFIG_TYPE_STRING);
        config_setting_set_string(opt, system.name.c_str());
        // add system id
        char sys[32];
        snprintf(sys, 31, "0x%08x", system.id);
        opt = config_setting_add(setting, "SYSTEM_ID", CONFIG_TYPE_STRING);
        config_setting_set_string(opt, sys);
        // add roms path
        opt = config_setting_add(setting, "PATH", CONFIG_TYPE_STRING);
        config_setting_set_string(opt, path.c_str());
        return true;
    }

    return false;
}

std::vector<PEMUConfig::RomPath> PEMUConfig::getRomPaths() {
    std::vector<RomPath> romPaths;
    auto roms = config_lookup(libConfigGetInstance(), std::string(getName() + ".ROMS").c_str());
    if (!roms) {
        printf("PEMUConfig::getRomPaths: config_lookup failed\n");
        return romPaths;
    }

    int count = config_setting_length(roms);
    for (int i = 0; i < count; ++i) {
        auto setting = config_setting_get_elem(roms, i);
        auto sysName = config_setting_get_elem(setting, 0);
        auto sysId = config_setting_get_elem(setting, 1);
        auto path = config_setting_get_elem(setting, 2);
        if (sysName && sysId && path) {
            std::string p = config_setting_get_string(path);
            if (!Utility::endsWith(p, "/")) p += "/";
            const char *s = config_setting_get_string(sysId);
            if (s) {
                auto id = Utility::parseHex(s);
                romPaths.push_back({p, {(int) id, 0, config_setting_get_string(sysName)}});
            }
        }
    }

    return romPaths;
}

std::string PEMUConfig::getNetworkRomPath() {
#ifdef __SMB__
    auto server = get(NET_SMB_SERVER);
    auto folder = get(NET_SMB_FOLDER);
    auto user = get(NET_SMB_USER);
    if (!server || !folder || !user) return "";
    return SmbIo::buildUrl(server->getString(), folder->getString(), user->getString(), getNetworkPassword());
#else
    return "";
#endif
}

std::string PEMUConfig::getNetworkPassword() {
    auto pass = get(NET_SMB_PASSWORD);
    if (!pass || pass->getString().empty()) return "";
    const std::string dataPath = p_renderer->getIo()->getDataPath();
    std::string plain;
    secret::decrypt(dataPath, pass->getString(), &plain);
    // a password from an older version (plain text): stored encrypted from now on
    if (!secret::isEncrypted(pass->getString())) {
        setNetworkPassword(plain);
        save();
    }
    return plain;
}

void PEMUConfig::setNetworkPassword(const std::string &plain) {
    auto pass = get(NET_SMB_PASSWORD);
    if (pass) pass->setString(secret::encrypt(p_renderer->getIo()->getDataPath(), plain));
}

std::vector<std::string> PEMUConfig::getNetworkFolderNames() {
    auto names = getCoreFolderNames();
    auto custom = get(NET_SMB_SYSTEM_FOLDER);
    if (custom && !custom->getString().empty()) {
        // also accept a path ("Roms/My SNES"): keep its last folder name
        std::string name = custom->getString();
        for (auto &c: name) if (c == '\\') c = '/';
        while (!name.empty() && name.back() == '/') name.pop_back();
        names.insert(names.begin(), name.substr(name.find_last_of('/') + 1));
    }
    return names;
}

std::string PEMUConfig::getEmuDataPath() {
    return p_renderer->getIo()->getDataPath();
}

std::vector<PEMUConfig::CoreSystem> PEMUConfig::getSystems() {
    return {{getNetworkSystem(), getNetworkFolderNames(), getCoreSupportedExt(), getLibretroSystem(), ""}};
}

ss_api::System PEMUConfig::getNetworkSystem() {
    // single system cores (psnes, pnes...) only have their own rom path
    const auto paths = getRomPaths();
    for (const auto &p: paths) {
        if (!Utility::startWith(Utility::toLower(p.path), "smb://")) return p.system;
    }
    return paths.empty() ? ss_api::System{0, 0, "NETWORK"} : paths.at(0).system;
}

Option *PEMUConfig::getOption(int id, bool isGame) {
    if (isGame && p_game_config) {
        return p_game_config->getOption(id);
    }

    return Config::getOption(id);
}

Option *PEMUConfig::get(int id, bool isGame) {
    return getOption(id, isGame);
}

std::vector<Input::ButtonMapping> PEMUConfig::getKeyboardMapping(int player, bool isGame) {
#ifndef NO_KEYBOARD
    return {
        {Input::Button::Up, getOption(KEY_UP, isGame)->getInteger()},
        {Input::Button::Down, getOption(KEY_DOWN, isGame)->getInteger()},
        {Input::Button::Left, getOption(KEY_LEFT, isGame)->getInteger()},
        {Input::Button::Right, getOption(KEY_RIGHT, isGame)->getInteger()},
        {Input::Button::Select, getOption(KEY_SELECT, isGame)->getInteger()},
        {Input::Button::Start, getOption(KEY_START, isGame)->getInteger()},
        {Input::Button::A, getOption(KEY_A, isGame)->getInteger()},
        {Input::Button::B, getOption(KEY_B, isGame)->getInteger()},
        {Input::Button::X, getOption(KEY_X, isGame)->getInteger()},
        {Input::Button::Y, getOption(KEY_Y, isGame)->getInteger()},
        {Input::Button::LT, getOption(KEY_LT, isGame)->getInteger()},
        {Input::Button::RT, getOption(KEY_RT, isGame)->getInteger()},
        {Input::Button::LB, getOption(KEY_LB, isGame)->getInteger()},
        {Input::Button::RB, getOption(KEY_RB, isGame)->getInteger()},
        {Input::Button::Menu1, getOption(KEY_MENU1, isGame)->getInteger()},
        {Input::Button::Menu2, getOption(KEY_MENU2, isGame)->getInteger()}
    };
#else
    return {};
#endif
}

std::vector<Input::ButtonMapping> PEMUConfig::getJoystickMapping(int player, bool isGame) {
    // TODO: allow per player config
    return {
        {Input::Button::Up, getOption(JOY_UP, isGame)->getInteger()},
        {Input::Button::Down, getOption(JOY_DOWN, isGame)->getInteger()},
        {Input::Button::Left, getOption(JOY_LEFT, isGame)->getInteger()},
        {Input::Button::Right, getOption(JOY_RIGHT, isGame)->getInteger()},
        {Input::Button::Select, getOption(JOY_SELECT, isGame)->getInteger()},
        {Input::Button::Start, getOption(JOY_START, isGame)->getInteger()},
        {Input::Button::A, getOption(JOY_A, isGame)->getInteger()},
        {Input::Button::B, getOption(JOY_B, isGame)->getInteger()},
        {Input::Button::X, getOption(JOY_X, isGame)->getInteger()},
        {Input::Button::Y, getOption(JOY_Y, isGame)->getInteger()},
        {Input::Button::LT, getOption(JOY_LT, isGame)->getInteger()},
        {Input::Button::RT, getOption(JOY_RT, isGame)->getInteger()},
        {Input::Button::LB, getOption(JOY_LB, isGame)->getInteger()},
        {Input::Button::RB, getOption(JOY_RB, isGame)->getInteger()},
        {Input::Button::Menu1, getOption(JOY_MENU1, isGame)->getInteger()},
        {Input::Button::Menu2, getOption(JOY_MENU2, isGame)->getInteger()}
    };
}

Vector2i PEMUConfig::getJoystickAxisLeftMapping(int player, bool isGame) {
    return {
        getOption(JOY_AXIS_LX, isGame)->getInteger(),
        getOption(JOY_AXIS_LY, isGame)->getInteger()
    };
}

Vector2i PEMUConfig::getJoystickAxisRightMapping(int player, bool isGame) {
    return {
        getOption(JOY_AXIS_RX, isGame)->getInteger(),
        getOption(JOY_AXIS_RY, isGame)->getInteger()
    };
}

int PEMUConfig::getJoystickDeadZone(int player, bool isGame) {
    return getOption(JOY_DEADZONE, isGame)->getInteger();
}

PEMUConfig::~PEMUConfig() {
    printf("PEMUConfig::~PEMUConfig()\n");
    delete (p_game_config);
}
