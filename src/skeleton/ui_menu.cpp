//
// Created by cpasjuste on 30/01/18.
//

#include "pemu.h"
#include "ui_menu.h"
#include "text_input.h"

// menu entries without a config option that can be selected ("buttons")
static bool isMenuAction(const std::string &name) {
    return name == "STATES" || name == "QUIT" || name == "TEST_CONNECTION" || name == "DOWNLOAD_ALL";
}

using namespace c2d::config;

class MenuLine : public c2d::RectangleShape {

public:
    MenuLine(UiMain *u, FloatRect &rect, Skin::TextGroup &tg) : RectangleShape(rect) {
        pMain = u;
        m_textGroup = tg;
        Font *font = pMain->getSkin()->getFont();

        p_name = new Text("OPTION NAME", m_textGroup.size, font);
        p_name->setFillColor(m_textGroup.color);
        p_name->setOutlineThickness(m_textGroup.outlineSize);
        p_name->setOutlineColor(m_textGroup.outlineColor);
        p_name->setOrigin(Origin::Left);
        p_name->setPosition(2 * pMain->getScaling().x, MenuLine::getSize().y / 2);
        p_name->setSizeMax((MenuLine::getSize().x * 0.55f), 0);
        MenuLine::add(p_name);

        p_value = new Text("OPTION VALUE", m_textGroup.size, font);
        p_value->setFillColor(m_textGroup.color);
        p_value->setOutlineThickness(m_textGroup.outlineSize);
        p_value->setOutlineColor(m_textGroup.outlineColor);
        p_value->setOrigin(Origin::Left);
        p_value->setPosition((MenuLine::getSize().x * 0.6f), MenuLine::getSize().y / 2);
        p_value->setSizeMax(MenuLine::getSize().x * 0.38f, 0);
        MenuLine::add(p_value);

        p_sprite = new Sprite();
        MenuLine::add(p_sprite);
    }

    void refresh() {
        set(p_name->getString(), p_option);
    }

    void set(const std::string &name, Option *option) {
        p_option = option;

        // reset
        setVisibility(Visibility::Visible);
        p_sprite->setVisibility(Visibility::Hidden);
        p_name->setString(name);
        p_value->setVisibility(Visibility::Visible);
        setFillColor(Color::Transparent);

        // this is a menu title (or custom option)
        if (!option) {
            // custom options
            if (isMenuAction(name)) {
                p_value->setVisibility(Visibility::Visible);
                p_value->setString("GO");
                return;
            }
            p_value->setVisibility(Visibility::Hidden);
            setFillColor(pMain->getUiMenu()->getOutlineColor());
            return;
        }

        // this is an option
        if (option->getFlags() & PEMUConfig::Flags::INPUT) {
            Skin::Button *button = pMain->getSkin()->getButton(option->getInteger());
            if (button && option->getId() < PEMUConfig::OptId::JOY_DEADZONE) {
                if (button->texture) {
                    p_sprite->setTexture(button->texture, true);
                    p_sprite->setVisibility(Visibility::Visible);
                    p_value->setVisibility(Visibility::Hidden);
                    float scaling = std::min(
                            getSize().x / (float) p_sprite->getSize().x,
                            getSize().y / (float) p_sprite->getSize().y);
                    p_sprite->setScale(scaling, scaling);
                    p_sprite->setPosition((MenuLine::getSize().x * 0.6f), MenuLine::getSize().y / 2);
                    p_sprite->setOrigin(Origin::Left);
                } else {
                    p_sprite->setVisibility(Visibility::Hidden);
                    p_value->setVisibility(Visibility::Visible);
                    p_value->setString(button->name);
                }
            } else {
                char btn[16];
                snprintf(btn, 16, "%i", option->getInteger());
                p_value->setVisibility(Visibility::Visible);
                p_value->setString(btn);
            }
        } else {
            p_value->setVisibility(Visibility::Visible);
            std::string value = option->getString();
            if (option->getId() == PEMUConfig::OptId::NET_SMB_PASSWORD && !value.empty()) {
                value = "********";
            } else if (option->getType() == Option::Type::String && value.empty()) {
                value = "-";
            }
            p_value->setString(value);
        }
    }

    UiMain *pMain = nullptr;
    Text *p_name = nullptr;
    Text *p_value = nullptr;
    Sprite *p_sprite = nullptr;
    Option *p_option = nullptr;
    Skin::TextGroup m_textGroup;
};

UiMenu::UiMenu(UiMain *uiMain) : SkinnedRectangle(uiMain, {"OPTIONS_MENU"}) {
    ui = uiMain;
    alpha = UiMenu::getAlpha();

    // menu title
    title = new SkinnedText(uiMain, {"OPTIONS_MENU", "TITLE_TEXT"});
    UiMenu::add(title);

    // retrieve skin config for options items
    textGroup = ui->getSkin()->getText({"OPTIONS_MENU", "ITEMS_TEXT"});

    // calculate number of items shown
    lineHeight = (float) textGroup.size + (2 * ui->getScaling().y);
    maxLines = (int) (UiMenu::getSize().y / lineHeight);
    if ((float) maxLines * lineHeight < UiMenu::getSize().y) {
        lineHeight = UiMenu::getSize().y / (float) maxLines;
    }

    // add selection rectangle (highlight)
    highlight = new RectangleShape({16, 16});
    ui->getSkin()->loadRectangleShape(highlight, {"SKIN_CONFIG", "HIGHLIGHT"});
    highlight->setSize(UiMenu::getSize().x - 2, lineHeight - (highlight->getOutlineThickness() * 2));
    highlight->move(1, 0);
    UiMenu::add(highlight);

    // add options items
    for (unsigned int i = 0; i < (unsigned int) maxLines; i++) {
        FloatRect r = {0, lineHeight * (float) i, UiMenu::getSize().x, lineHeight};
        auto line = new MenuLine(ui, r, textGroup);
        lines.push_back(line);
        UiMenu::add(line);
    }

    // tween
    Vector2f targetPos = {UiMenu::getPosition().x - UiMenu::getSize().x,
                          UiMenu::getPosition().y};
    tweenPosition = new TweenPosition({UiMenu::getPosition()}, targetPos, 0.2f);
    tweenPosition->setState(TweenState::Stopped);
    UiMenu::add(tweenPosition);

    // hide by default
    UiMenu::setVisibility(Visibility::Hidden);
}

void UiMenu::load(bool isGame) {
    isRomMenu = isGame;
    isEmuRunning = ui->getUiEmu()->isVisible();
    Game game = ui->getUiRomList()->getSelection();

    if (isRomMenu) {
        ui->getConfig()->loadGame(game);
        bool useZipName = ui->getConfig()->getOption(PEMUConfig::OptId::UI_SHOW_ZIP_NAMES)->getInteger();
        title->setString(useZipName ? Utility::removeExt(Utility::baseName(game.path)) : game.name);
    } else {
        title->setString("MAIN OPTIONS");
    }

    // set options items
    optionIndex = highlightIndex = 0;
    menu_options.clear();
    auto groups = ui->getConfig()->getGroups();
    for (auto &group: *groups) {
        // skip some options if needed
        if (group.getName() == "ROMS") continue;
        if (isRomMenu && (group.getName() == "UI_OPTIONS" || group.getName() == "UI_FILTERING"
                          || group.getName() == "NETWORK" || group.getName() == "ARTWORK")) continue;
        // no artwork downloads for this core
        if (group.getName() == "ARTWORK" && !ui->getUiRomList()->getRomList()->artwork) continue;
        // push group title
        menu_options.push_back({group.getName(), nullptr});
        auto options = group.getOptions();
        for (auto &option: *options) {
            // skip hidden options
            if (option.getFlags() & PEMUConfig::Flags::HIDDEN) continue;
            // push option
            auto opt = ui->getConfig()->get(option.getId(), isGame);
            menu_options.push_back({option.getName(), opt});
        }
        if (group.getName() == "NETWORK") {
            menu_options.push_back({"TEST_CONNECTION", nullptr});
        }
        if (group.getName() == "ARTWORK") {
            menu_options.push_back({"DOWNLOAD_ALL", nullptr});
        }
    }

    // add "OTHER" option menu
    menu_options.push_back({"OTHER", nullptr});
    if (isRomMenu) menu_options.push_back({"STATES", nullptr});
    menu_options.push_back({"QUIT", nullptr});

    setAlpha(isEmuRunning ? (uint8_t) (alpha - 50) : (uint8_t) alpha);

    // update options lines/items (first option should always be a menu option)
    onKeyDown();

    // finally, show me
    if (!isVisible()) {
        setLayer(1);
        setVisibility(Visibility::Visible, true);
    }
}

void UiMenu::updateLines() {
    for (unsigned int i = 0; i < (unsigned int) maxLines; i++) {
        if (optionIndex + i >= menu_options.size()) {
            lines[i]->setVisibility(Visibility::Hidden);
            continue;
        }
        // set line data
        auto menuOption = menu_options.at(optionIndex + i);
        lines[i]->set(menuOption.name, menuOption.option);
        // set highlight position and color
        if ((int) i == highlightIndex) {
            highlight->setPosition({highlight->getPosition().x,
                                    lines[i]->getPosition().y + highlight->getOutlineThickness()});
            lines[i]->p_value->setOutlineColor(getOutlineColor());
        } else {
            lines[i]->p_value->setOutlineColor(textGroup.outlineColor);
        }
    }
}

void UiMenu::onKeyUp() {
    int index = optionIndex + highlightIndex;
    int middle = maxLines / 2;

    if (highlightIndex <= middle && index - middle > 0) {
        optionIndex--;
    } else {
        highlightIndex--;
    }

    if (highlightIndex < 0) {
        highlightIndex = (int) menu_options.size() < maxLines - 1 ? (int) menu_options.size() - 1 : maxLines - 1;
        optionIndex = ((int) menu_options.size() - 1) - highlightIndex;
    }

    // skip menus
    auto opt = menu_options.at(optionIndex + highlightIndex);
    if (opt.option == nullptr && !isMenuAction(opt.name)) {
        return onKeyUp();
    }

    updateLines();
}

void UiMenu::onKeyDown() {
    int index = optionIndex + highlightIndex;
    int middle = maxLines / 2;

    if (highlightIndex >= middle && index + (maxLines - middle) < (int) menu_options.size()) {
        optionIndex++;
    } else {
        highlightIndex++;
    }

    if (highlightIndex >= maxLines || optionIndex + highlightIndex >= (int) menu_options.size()) {
        optionIndex = 0;
        highlightIndex = 0;
    }

    // skip menus
    auto opt = menu_options.at(optionIndex + highlightIndex);
    if (opt.option == nullptr && !isMenuAction(opt.name)) {
        return onKeyDown();
    }

    updateLines();
}

bool UiMenu::onInput(c2d::Input::Player *players) {
    unsigned int buttons = players[0].buttons;

    if (ui->getUiStateMenu()->isVisible()) {
        return C2DObject::onInput(players);
    }

    // UP
    if (buttons & Input::Button::Up) {
        onKeyUp();
    }

    // DOWN
    if (buttons & Input::Button::Down) {
        onKeyDown();
    }

    // LEFT /RIGHT
    if (buttons & Input::Button::Left || buttons & Input::Button::Right) {
        auto option = lines.at(highlightIndex)->p_option;
        if (!option || option->getArray().size() <= 1) return true;
        needSave = true;
        if (buttons & Input::Button::Left) {
            option->setArrayMovePrev();
        } else {
            option->setArrayMoveNext();
        }
        if (!option->getComment().empty()) {
            ui->getUiStatusBox()->show(option->getComment());
        }

        switch (option->getId()) {
            case PEMUConfig::OptId::UI_FILTER_FAVORITES:
            case PEMUConfig::OptId::UI_FILTER_AVAILABLE:
            case PEMUConfig::OptId::UI_SHOW_ZIP_NAMES:
            case PEMUConfig::OptId::UI_FILTER_CLONES:
            case PEMUConfig::OptId::UI_FILTER_SYSTEM:
            case PEMUConfig::OptId::UI_FILTER_EDITOR:
            case PEMUConfig::OptId::UI_FILTER_DEVELOPER:
            case PEMUConfig::OptId::UI_FILTER_PLAYERS:
            case PEMUConfig::OptId::UI_FILTER_RATING:
            case PEMUConfig::OptId::UI_FILTER_DATE:
            case PEMUConfig::OptId::UI_FILTER_GENRE: {
                std::string name = Utility::toUpper(option->getName());
                std::string value = Utility::toUpper(option->getString());
                if (option->getComment().empty()) {
                    ui->getUiStatusBox()->show("%s: %s", name.c_str(), value.c_str());
                }
                ui->getUiRomList()->updateRomList();
                break;
            }

            case PEMUConfig::OptId::EMU_ROTATION:
            case PEMUConfig::OptId::EMU_SCALING:
                if (isEmuRunning) {
                    ui->getUiEmu()->getVideo()->updateScaling();
                    auto gw = (float) ui->getUiEmu()->getVideo()->getTextureRect().width;
                    auto gh = (float) ui->getUiEmu()->getVideo()->getTextureRect().height;
                    float gr = std::max(
                            (float) ui->getUiEmu()->getVideo()->aspect.x /
                            (float) ui->getUiEmu()->getVideo()->aspect.y,
                            (float) ui->getUiEmu()->getVideo()->aspect.y /
                            (float) ui->getUiEmu()->getVideo()->aspect.x);
                    float ow = gw * ui->getUiEmu()->getVideo()->getScale().x;
                    float oh = gh * ui->getUiEmu()->getVideo()->getScale().y;
                    float ratio = std::max(ow / oh, oh / ow);
                    ui->getUiStatusBox()->show(
                            "GAME: %ix%i - RATIO: %.2f | OUTPUT: %ix%i - RATIO: %.2f - SCALING: %.2fx%.2f",
                            (int) gw, (int) gh, gr, (int) ow, (int) oh, ratio,
                            ui->getUiEmu()->getVideo()->getScale().x, ui->getUiEmu()->getVideo()->getScale().y);
                }
                break;
            case PEMUConfig::OptId::EMU_SCALING_MODE:
                if (option->getString() == "AUTO") {
                    ui->getUiStatusBox()->show("TRY TO KEEP INTEGER SCALING IF ASPECT RATIO IS NOT TOO DIVERGENT");
                } else if (option->getString() == "ASPECT") {
                    ui->getUiStatusBox()->show("KEEP GAME ASPECT RATIO - SOME SHADERS MAY NOT RENDER CORRECTLY");
                } else {
                    ui->getUiStatusBox()->show(
                            "FORCE INTEGER SCALING - ASPECT RATIO MAY BE WRONG BUT SHADERS WILL RENDER CORRECTLY");
                }
                if (isEmuRunning) {
                    ui->getUiEmu()->getVideo()->updateScaling();
                }
                break;
            case PEMUConfig::OptId::EMU_FILTER:
                if (isEmuRunning) {
                    ui->getUiEmu()->getVideo()->setFilter((Texture::Filter) option->getArrayIndex());
                }
                break;
            case PEMUConfig::OptId::EMU_SHADER:
                if (isEmuRunning) {
                    ui->getUiEmu()->getVideo()->setShader(option->getArrayIndex());
                    ui->getUiStatusBox()->show(option->getString());
                }
                break;
#ifdef __VITA__
                case PEMUConfig::OptId::EMU_WAIT_RENDERING:
                        if (isEmuRunning) {
                            ((PSP2Renderer *) ui)->setWaitRendering(option->getInteger());
                        }
                        break;
#endif
            case PEMUConfig::OptId::ART_IMAGE_TYPE:
                // show the newly chosen image type
                ui->getUiRomList()->updateRomList();
                break;
            case PEMUConfig::OptId::UI_VIDEO_SNAP_DELAY:
                ui->getUiRomList()->setVideoSnapDelay(option->getInteger());
                break;

            default:
                break;
        }

        // update option line
        lines.at(highlightIndex)->refresh();
    }

    // FIRE1 (ENTER)
    if (buttons & Input::Button::A) {
        auto option = lines.at(highlightIndex)->p_option;
        if (option && isNetworkTextOption(option)) {
            editNetworkOption(option);
        } else if (lines.at(highlightIndex)->p_name->getString() == "TEST_CONNECTION") {
            testConnection();
        } else if (lines.at(highlightIndex)->p_name->getString() == "DOWNLOAD_ALL") {
            downloadArtwork();
        } else if (option && option->getFlags() == PEMUConfig::Flags::INPUT) {
            int new_key = 0;
            int res = ui->getUiMessageBox()->show("NEW INPUT", "PRESS A BUTTON", "", "", &new_key, 9);
            if (res != MessageBox::TIMEOUT) {
                needSave = true;
                option->setInteger(new_key);
                lines.at(highlightIndex)->refresh();
            }
        } else if (lines.at(highlightIndex)->p_name->getString() == "STATES") {
            setVisibility(Visibility::Hidden, true);
            ui->getUiStateMenu()->setVisibility(Visibility::Visible, true);
        } else if (lines.at(highlightIndex)->p_name->getString() == "QUIT") {
            if (isEmuRunning) {
                setVisibility(Visibility::Hidden, true);
                ui->getUiEmu()->stop();
                ui->getUiRomList()->setVisibility(Visibility::Visible);
                ui->getInput()->clear();
            } else {
                // be sure options are saved before exiting
                if (isRomMenu) ui->getConfig()->saveGame();
                else ui->getConfig()->save();
                needSave = false;
                ui->done = true;
            }
        }
    }

    // FIRE2 (BACK)
    if (buttons & Input::Button::Menu1 || buttons & Input::Button::Menu2 || buttons & Input::Button::B) {
        setVisibility(Visibility::Hidden, true);
        if (isEmuRunning) {
            ui->getUiEmu()->resume();
        }
    }

    return true;
}

void UiMenu::setVisibility(Visibility visibility, bool tweenPlay) {
    if (ui->getUiRomList() && ui->getUiRomList()->isVisible()) {
        ui->getUiRomList()->getBlur()->setVisibility(visibility, true);
    }

    if (visibility == Visibility::Hidden && needSave) {
        if (isRomMenu) ui->getConfig()->saveGame();
        else ui->getConfig()->save();
        needSave = false;
    }

    RectangleShape::setVisibility(visibility, tweenPlay);
}

bool UiMenu::isNetworkTextOption(Option *option) {
    switch (option->getId()) {
        case PEMUConfig::OptId::NET_SMB_SERVER:
        case PEMUConfig::OptId::NET_SMB_FOLDER:
        case PEMUConfig::OptId::NET_SMB_USER:
        case PEMUConfig::OptId::NET_SMB_PASSWORD:
        case PEMUConfig::OptId::NET_SMB_SYSTEM_FOLDER:
            return true;
        default:
            return false;
    }
}

void UiMenu::editNetworkOption(Option *option) {
    std::string title;
    TextInput::Kind kind = TextInput::Kind::Latin;
    switch (option->getId()) {
        case PEMUConfig::OptId::NET_SMB_SERVER:
            title = "Server address (example: 192.168.1.20)";
            break;
        case PEMUConfig::OptId::NET_SMB_FOLDER:
            title = "Your roms folder (example: Roms)";
            kind = TextInput::Kind::Text;
            break;
        case PEMUConfig::OptId::NET_SMB_SYSTEM_FOLDER:
            title = "Name of this console's folder (empty: automatic)";
            kind = TextInput::Kind::Text;
            break;
        case PEMUConfig::OptId::NET_SMB_USER:
            title = "Username (empty for guest)";
            break;
        default:
            title = "Password";
            kind = TextInput::Kind::Password;
            break;
    }

    std::string value;
    if (!TextInput::get(ui, title, option->getString(), kind, &value)) return;

    // "\\192.168.1.20\Roms\SNES" pasted in the server field: split it
    if (option->getId() == PEMUConfig::OptId::NET_SMB_SERVER) {
        std::string v = value;
        for (auto &c: v) if (c == '\\') c = '/';
        while (!v.empty() && v[0] == '/') v.erase(0, 1);
        if (Utility::startWith(Utility::toLower(v), "smb://")) v = v.substr(6);
        const size_t slash = v.find('/');
        if (slash != std::string::npos) {
            std::string folder = v.substr(slash + 1);
            while (!folder.empty() && folder.back() == '/') folder.pop_back();
            if (!folder.empty()) ui->getConfig()->get(PEMUConfig::OptId::NET_SMB_FOLDER)->setString(folder);
            v = v.substr(0, slash);
        }
        value = v;
    }

    option->setString(value);
    needSave = true;
    updateLines();
    ui->getUiStatusBox()->show("USE TEST_CONNECTION TO CHECK, THEN RESTART THE APP TO LOAD THE GAMES");
}

void UiMenu::testConnection() {
#ifdef __SMB__
    auto cfg = ui->getConfig();
    const std::string url = cfg->getNetworkRomPath();
    if (url.empty()) {
        ui->getUiMessageBox()->show("TEST CONNECTION", "Please set SERVER and FOLDER first.", "OK");
        return;
    }

    // save now, so the settings are kept even if the app is closed from the ps4 menu
    cfg->save();
    needSave = false;

    const std::string display = SmbIo::toDisplayUrl(url);
    auto progress = ui->getUiProgressBox();
    progress->setTitle("TEST CONNECTION");
    progress->setMessage("Connecting to " + display + "...");
    progress->setProgress(0);
    progress->setVisibility(Visibility::Visible);
    progress->setLayer(1000);
    ui->flip(true, false);

    // same steps as the game list: find this console's folder(s), then count the games
    const std::string system = cfg->getNetworkSystem().name;
    const auto names = cfg->getNetworkFolderNames();
    SmbIo::SystemFolders folders;
    std::vector<SmbIo::File> files;
    std::string error;
    bool ok = SmbIo::findSystemFolders(url, names, &folders, &error);
    for (size_t i = 0; ok && i < folders.urls.size(); i++) {
        progress->setMessage("Looking for games in " + SmbIo::folderName(folders.urls[i]) + "...");
        progress->setProgress((float) i / (float) folders.urls.size());
        ui->flip(true, false);
        ok = SmbIo::listFiles(folders.urls[i], cfg->getCoreSupportedExt(), &files, &error,
                              folders.scanSubFolders ? SMB_MAX_FOLDER_DEPTH : 0);
    }

    progress->setProgress(1);
    ui->flip(true, false);
    progress->setVisibility(Visibility::Hidden);

    std::string where;
    for (const auto &u: folders.urls) where += (where.empty() ? "\"" : ", \"") + SmbIo::folderName(u) + "\"";

    std::string message;
    if (!ok) {
        message = SmbIo::friendlyError(error, url);
    } else if (!names.empty() && !folders.found) {
        // show two of the names we look for
        std::string known = "\"" + names[0] + "\"";
        if (names.size() > 1) known += " or \"" + names.back() + "\"";
        message = "No " + system + " folder found in \"" + SmbIo::folderName(url) + "\" (looking for names like " +
                  known + "). Set SYSTEM_FOLDER to your folder's name.";
        if (!files.empty()) {
            message += " For now, the " + std::to_string(files.size()) + (files.size() == 1 ? " game" : " games") +
                       " directly in \"" + SmbIo::folderName(url) + "\" will be used.";
        }
    } else if (files.empty()) {
        message = "Connected to " + where + ", but no games were found in it or its sub folders.";
    } else {
        message = "Found " + std::to_string(files.size()) + (files.size() == 1 ? " game" : " games") +
                  " in " + where + ". Restart the app to show them in the list.";
    }
    printf("UiMenu::testConnection: %s\n", message.c_str());
    ui->getUiMessageBox()->show(!ok ? "TEST CONNECTION: FAILED"
                                    : files.empty() ? "TEST CONNECTION: NO GAMES"
                                    : (!names.empty() && !folders.found) ? "TEST CONNECTION: CHECK FOLDER"
                                    : "TEST CONNECTION: OK", message, "OK");
#endif
}

void UiMenu::downloadArtwork() {
    auto romList = ui->getUiRomList()->getRomList();
    if (romList->artworks.empty()) return;

    // save the settings now (the app may be closed from the ps4 menu during a long download)
    ui->getConfig()->save();
    needSave = false;

    auto progress = ui->getUiProgressBox();
    progress->setTitle("DOWNLOAD ARTWORK (B / CIRCLE: CANCEL)");
    progress->setMessage("Downloading game information...");
    progress->setProgress(0);
    progress->setVisibility(Visibility::Visible);
    progress->setLayer(1000);
    ui->flip(true, false);
    ui->getInput()->clear();

    // redraw, and tell if the user wants to cancel
    auto keepGoing = [this]() {
        ui->flip(true, false);
        auto players = ui->getInput()->update();
        return !(players && (players[0].buttons & Input::Button::B));
    };

    // 1. game information database (developer, genre, year...), one per console
    std::string error;
    bool cancelled = false;
    bool dbOk = true;
    int infoGames = 0;
    const int dbCount = (int) romList->artworks.size();
    for (int a = 0; a < dbCount && !cancelled; a++) {
        auto artwork = romList->artworks[a];
        std::string err;
        const bool ok = artwork->downloadDatabase([&](int done, int total, const std::string &) {
            progress->setMessage("Downloading game information (" + std::to_string(a * total + done) + "/" +
                                 std::to_string(dbCount * total) + ")...");
            progress->setProgress(total ? 0.1f * (float) (a * total + done) / (float) (dbCount * total) : 0);
            if (!keepGoing()) cancelled = true;
            return !cancelled;
        }, &err);
        if (!ok) {
            dbOk = false;
            if (error.empty()) error = err;
        } else if (artwork->loadDatabase()) {
            infoGames += artwork->applyDatabase(romList->gameList);
        }
    }
    romList->updateFilterOptions();

    // 2. images of the games that don't have one yet
    std::vector<Game> todo;
    const auto type = ui->getUiRomList()->getArtworkType();
    if (!cancelled) {
        for (const auto &g: romList->gameList->games) {
            auto artwork = romList->getArtwork(g);
            if (g.available && artwork && artwork->findImage(g, type).empty() && !artwork->isMissing(g, type)) {
                todo.push_back(g);
            }
        }
    }

    int downloaded = 0, notAvailable = 0, failed = 0, failedInRow = 0;
    std::string imageError;
    for (size_t i = 0; i < todo.size() && !cancelled; i++) {
        progress->setMessage("Image " + std::to_string(i + 1) + " / " + std::to_string(todo.size()) +
                             ": " + todo[i].name);
        progress->setProgress(0.1f + 0.9f * (float) i / (float) todo.size());
        if (!keepGoing()) {
            cancelled = true;
            break;
        }
        std::string err;
        auto artwork = romList->getArtwork(todo[i]);
        if (artwork->downloadImage(todo[i], type, &err)) {
            downloaded++;
            failedInRow = 0;
        } else if (artwork->isMissing(todo[i], type)) {
            notAvailable++;
            failedInRow = 0;
        } else if (++failed, ++failedInRow >= 3) {
            // network problem: stop instead of failing on every game
            imageError = err;
            break;
        }
    }

    progress->setProgress(1);
    ui->flip(true, false);
    progress->setVisibility(Visibility::Hidden);
    ui->getInput()->clear();

    // summary
    std::string message;
    if (!dbOk && !cancelled) {
        message = "Game information: download failed (" + error + "). ";
    } else if (dbOk && infoGames > 0) {
        message = "Game information added to " + std::to_string(infoGames) +
                  (infoGames == 1 ? " game. " : " games. ");
    } else if (dbOk) {
        message = "Game information is up to date. ";
    }
    message += "Images: " + std::to_string(downloaded) + " downloaded";
    if (notAvailable) message += ", " + std::to_string(notAvailable) + " not available online";
    if (failed) message += ", " + std::to_string(failed) + " failed";
    message += ".";
    if (!imageError.empty()) message += " Stopped: " + imageError + ".";
    if (cancelled) message += " Cancelled.";
    printf("UiMenu::downloadArtwork: %s\n", message.c_str());
    ui->getUiMessageBox()->show("DOWNLOAD ARTWORK", message, "OK");

    ui->getUiRomList()->updateRomList();
}

UiMenu::~UiMenu() {
    printf("~UIMenuNew\n");
}
