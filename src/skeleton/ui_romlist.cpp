//
// Created by cpasjuste on 22/11/16.
//
#include <algorithm>
#include <ctime>
#include "pemu.h"
#include "launcher.h"
#include "pemu_log.h"
#include "ss_api.h"

using namespace c2d;
using namespace c2d::config;
using namespace pemu;
using namespace ss_api;

UIRomList::UIRomList(UiMain *main, RomList *romList, const c2d::Vector2f &size)
        : SkinnedRectangle(main, {"SKIN_CONFIG", "MAIN"}) {
    printf("UIRomList\n");
    pMain = main;
    pRomList = romList;
    Skin *skin = pMain->getSkin();

    // add title image if available
    auto *title = new SkinnedRectangle(pMain, {"SKIN_CONFIG", "MAIN", "TITLE"});
    UIRomList::add(title);

    // add help
    auto *help = new UiHelp(pMain);
    UIRomList::add(help);

    // add rom info ui
    pRomInfo = new UIRomInfo(pMain, this, skin->getFont(), pMain->getFontSize());
    UIRomList::add(pRomInfo);

    // add rom list title (system text + rom count)
    pTitleText = new SkinnedText(pMain, {"SKIN_CONFIG", "MAIN", "ROM_LIST", "SYSTEM_TEXT"});
    if (pTitleText->available) {
        UIRomList::add(pTitleText);
        // pppEmu: a long title is made smaller to fit, not cut
        mTitleMaxWidth = skin->getText({"MAIN", "ROM_LIST", "SYSTEM_TEXT"}).rect.width;
        pTitleText->setSizeMax(4096, 4096);
    } else {
        delete (pTitleText);
        pTitleText = nullptr;
    }
    mShowSystemText = !(pMain->getConfig()->getOption(PEMUConfig::OptId::UI_FILTER_SYSTEM)->getFlags()
                        & PEMUConfig::Flags::HIDDEN);

    // add rom listing ui
    Skin::TextGroup textGroup = skin->getText({"SKIN_CONFIG", "MAIN", "ROM_LIST", "TEXT"});
    config::Group *grp = skin->getConfig()->getGroup("ROM_LIST")->getGroup("TEXT");
    config::Option *opt = grp->getOption("color_missing");
    Color colorMissing;
    if (opt->getType() == config::Option::Type::String) {
        opt = skin->getConfig()->getOption("COLORS", opt->getString());
        if (opt) {
            colorMissing = opt->getColor();
        }
    } else {
        colorMissing = grp->getOption("color_missing")->getColor();
    }

    bool highlightUseFileColors = grp->getOption("highlight_use_text_color")->getInteger() == 1;

    // add rom list ui
    Skin::RectangleShapeGroup romListGroup = skin->getRectangleShape({"MAIN", "ROM_LIST"});
    pListBox = new UIListBox(pMain, skin->getFont(), (int) textGroup.size,
                             romListGroup.rect, mGameList.games);
    pListBox->colorMissing = colorMissing;
    pListBox->colorAvailable = textGroup.color;
    pListBox->setFillColor(romListGroup.color);
    pListBox->setOutlineColor(romListGroup.outlineColor);
    pListBox->setOutlineThickness((float) romListGroup.outlineSize);
    pListBox->setSelection(0);
    // rom item
    pListBox->setTextOutlineColor(textGroup.outlineColor);
    pListBox->setTextOutlineThickness((float) textGroup.outlineSize);
    // hihglight
    Skin::RectangleShapeGroup rectShape = skin->getRectangleShape({"SKIN_CONFIG", "HIGHLIGHT"});
    pListBox->getHighlight()->setFillColor(rectShape.color);
    pListBox->getHighlight()->setOutlineColor(rectShape.outlineColor);
    pListBox->getHighlight()->setOutlineThickness((float) rectShape.outlineSize);
    pListBox->setHighlightUseFileColor(highlightUseFileColors);
    UIRomList::add(pListBox);

    // add blur
    pBlur = new RectangleShape(UIRomList::getLocalBounds());
    pBlur->setFillColor(Color::Gray);
    pBlur->add(new TweenAlpha(0, 230, 0.2));
    pBlur->setVisibility(Visibility::Hidden);
    UIRomList::add(pBlur);

    int delay = pMain->getConfig()->get(PEMUConfig::OptId::UI_VIDEO_SNAP_DELAY)->getInteger();
    UIRomList::setVideoSnapDelay(delay);
}

std::string UIRomList::getPreview(const Game &game, UIRomList::PreviewType type) {
    std::string mediaType = type == Tex ? "mixrbv2" : "video";
    std::string mediaExt = type == Tex ? ".png" : ".mp4";

    // downloaded artwork (<data>/media/boxart, title or snap), as chosen in the ARTWORK menu
    Artwork *artwork = type == Tex ? pRomList->getArtwork(game) : nullptr;
    if (artwork) {
        const std::string art = artwork->findImage(game, getArtworkType());
        if (!art.empty()) return art;
    }

    // try media path from database
    std::string path = game.romsPath + game.getMedia(mediaType).url;
    printf("getPreview(%s)\n", path.c_str());
    if (pMain->getIo()->getFile(path).isFile()) {
        return path;
    }

    // try from filename
    path = game.romsPath + "media/" + mediaType + "/" + Utility::removeExt(game.path) + mediaExt;
    printf("getPreview(%s)\n", path.c_str());
    if (pMain->getIo()->getFile(path).isFile()) {
        return path;
    }

    // for non arcade game, search for a "screenscraper" game with same name
    if (!game.isClone()) {
        std::vector<Game> clones = pRomList->gameList->findGamesByName(game);
        for (const auto &g: clones) {
            path = g.romsPath + g.getMedia(mediaType).url;
            printf("getPreview(%s)\n", path.c_str());
            if (pMain->getIo()->getFile(path).isFile()) {
                return path;
            }
        }
    }

    // now try parent image if rom is a clone
    if (game.isClone()) {
        Game parent = pRomList->gameList->findGameByPath(game.cloneOf);
        path = parent.romsPath + parent.getMedia(mediaType).url;
        printf("getPreview(%s)\n", path.c_str());
        if (pMain->getIo()->getFile(path).isFile()) {
            return path;
        }
        // try from filename
        path = parent.romsPath + "media/" + mediaType + "/" + Utility::removeExt(parent.path) + mediaExt;
        printf("getPreview(%s)\n", path.c_str());
        if (pMain->getIo()->getFile(path).isFile()) {
            return path;
        }
    }

    return {};
}

void UIRomList::updateBackground() {
    // the animated background's colours follow the console page (L2 / R2)
    pMain->getBackground()->setPage(pMain->getConfig()->get(PEMUConfig::OptId::UI_FILTER_SYSTEM)->getString());
}

void UIRomList::updateRomList() {
    filterRomList();
    sortRomList();
    updateBackground();

    std::string sys = pMain->getConfig()->get(PEMUConfig::OptId::UI_FILTER_SYSTEM)->getString();
    int systemId = sys == "ALL" ? -1 : pRomList->gameList->systemList.findByName(sys).id;
    if (mShowSystemText && pTitleText) {
        pTitleText->setString(sys + " - " +
                              std::to_string(mGameList.getAvailableCount()) + "/" +
                              std::to_string(pRomList->gameList->getCount(systemId)));
    } else if (pTitleText) {
        pTitleText->setString(std::to_string(mGameList.getAvailableCount()) + "/" +
                              std::to_string(pRomList->gameList->getCount(systemId)));
    }
    if (pTitleText && mTitleMaxWidth > 0) {
        pTitleText->setScale(1, 1);
        const float width = pTitleText->getLocalBounds().width;
        const float scale = width > mTitleMaxWidth ? mTitleMaxWidth / width : 1;
        pTitleText->setScale(scale, scale);
    }

    if (pListBox) {
        pListBox->setGames(mGameList.games);
    }

    if (pRomInfo) {
        pRomInfo->load(Game());
        mTimerLoadInfoDone = 0;
        mTimerLoadInfo.restart();
        mTimerLoadVideoDone = 0;
        mTimerLoadVideo.restart();
    }
}

void UIRomList::filterRomList() {
    auto cfg = pMain->getConfig();
    ss_api::GameList *list = cfg->get(PEMUConfig::OptId::UI_FILTER_FAVORITES)->getInteger() ?
                             pRomList->gameListFav : pRomList->gameList;
    std::string system = cfg->get(PEMUConfig::OptId::UI_FILTER_SYSTEM)->getString();
    int systemId = system == "ALL" ? -1 : list->systemList.findByName(system).id;
    std::string editor = cfg->get(PEMUConfig::OptId::UI_FILTER_EDITOR)->getString();
    int editorId = editor == "ALL" ? -1 : list->findEditorByName(editor).id;
    std::string dev = cfg->get(PEMUConfig::OptId::UI_FILTER_DEVELOPER)->getString();
    int devId = dev == "ALL" ? -1 : list->findDeveloperByName(dev).id;
    int players = Utility::parseInt(cfg->get(PEMUConfig::OptId::UI_FILTER_PLAYERS)->getString(), -1);
    int rating = Utility::parseInt(cfg->get(PEMUConfig::OptId::UI_FILTER_RATING)->getString(), -1);
    std::string genre = cfg->get(PEMUConfig::OptId::UI_FILTER_GENRE)->getString();
    int genreId = genre == "ALL" ? -1 : list->findGenreByName(genre).id;

    //printf("UIRomList::filterRomList: system: %s (0x%08x)\n", system.c_str(), systemId);
    mGameList = list->filter(
            cfg->get(PEMUConfig::OptId::UI_FILTER_AVAILABLE)->getInteger(),
            !cfg->get(PEMUConfig::OptId::UI_FILTER_CLONES)->getInteger(),
            systemId, -1, editorId, devId, players, rating, -1, genreId, "ALL",
            cfg->get(PEMUConfig::OptId::UI_FILTER_DATE)->getString()
    );
}

void UIRomList::sortRomList() {
    bool byZipName = pMain->getConfig()->get(PEMUConfig::OptId::UI_SHOW_ZIP_NAMES)->getInteger();
    mGameList.sortAlpha(byZipName);
}

void UIRomList::setVideoSnapDelay(int delay) {
    mTimerLoadVideoDelay = delay * 1000;
}

Game UIRomList::getSelection() {
    return pListBox->getSelection();
}

RomList *UIRomList::getRomList() {
    return pRomList;
}

void UIRomList::setGames(const std::vector<ss_api::Game> &games) {
    pListBox->setGames(games);
}

void UIRomList::setVisibility(c2d::Visibility visibility, bool tweenPlay) {
    if (visibility == c2d::Visibility::Hidden) {
        pRomInfo->load();
        mTimerLoadInfoDone = 0;
        mTimerLoadVideoDone = 0;
    } else {
        mTimerLoadInfo.restart();
        mTimerLoadVideo.restart();
        mIdleClock.restart();
    }

    RectangleShape::setVisibility(visibility, tweenPlay);
}

bool UIRomList::onInput(c2d::Input::Player *players) {
    if (pMain->getUiMenu()->isVisible()
        || pMain->getUiStateMenu()->isVisible()
        || pMain->getUiProgressBox()->isVisible()) {
        return false;
    }

    unsigned int buttons = players[0].buttons;
    if (buttons && buttons != Input::Button::Delay) mIdleClock.restart();

    // demo mode: L3 + R3
    if ((buttons & Input::Button::LS) && (buttons & Input::Button::RS)) {
        pMain->getInput()->clear();
        startDemo();
        return true;
    }

    if (buttons & Input::Button::Up) {
        pListBox->up();
        pRomInfo->load();
        mTimerLoadInfoDone = 0;
        mTimerLoadVideoDone = 0;
    } else if (buttons & Input::Button::Down) {
        pListBox->down();
        pRomInfo->load();
        mTimerLoadInfoDone = 0;
        mTimerLoadVideoDone = 0;
    } else if (buttons & Input::Button::Right) {
        pListBox->setSelection(pListBox->getIndex() + pListBox->getMaxLines());
        pRomInfo->load();
        mTimerLoadInfoDone = 0;
        mTimerLoadVideoDone = 0;
    } else if (buttons & Input::Button::Left) {
        pListBox->setSelection(pListBox->getIndex() - pListBox->getMaxLines());
        pRomInfo->load();
        mTimerLoadInfoDone = 0;
        mTimerLoadVideoDone = 0;
    } else if (buttons & Input::Button::A) {
        Game game = getSelection();
        if (game.available) {
#ifdef __MPV__
            pRomInfo->mpvTexture->setVisibility(c2d::Visibility::Hidden);
            pRomInfo->mpv->stop();
#endif
            // pppEmu: games of other consoles run in their own program
            if (launchInOtherProgram(pMain, game)) return true;
            pMain->getConfig()->loadGame(game);
            pMain->getUiEmu()->load(game);
            return true;
        }
    } else if (buttons & Input::Button::X) {
        // add to favorites
        Game game = getSelection();
        if (game.id > 0 && !pRomList->gameListFav->exist(game.id)) {
            int res = pMain->getUiMessageBox()->show(
                    "FAVORITES", "Add to favorites ?", "OK", "CANCEL");
            if (res == MessageBox::LEFT) {
                pRomList->addFav(game);
            }
        } else if (game.id > 0 && pRomList->gameListFav->exist(game.id)) {
            int res = pMain->getUiMessageBox()->show(
                    "FAVORITES", "Remove from favorites ?", "OK", "CANCEL");
            if (res == MessageBox::LEFT) {
                pRomList->removeFav(game);
                if (pMain->getConfig()->get(PEMUConfig::OptId::UI_FILTER_FAVORITES)->getInteger()) {
                    // update list if we are in favorites
                    updateRomList();
                }
            }
        }
    } else if (buttons & Input::Button::Menu1) {
        pMain->getUiMenu()->load();
    } else if (buttons & Input::Button::Menu2) {
        if (getSelection().id > 0) {
            pMain->getUiMenu()->load(true);
        }
    }

    // only allow system switch if skin contains romlist title
    if (pTitleText && mShowSystemText) {
        if (buttons & Input::Button::LT) {
            Option *sysOpt = pMain->getConfig()->get(PEMUConfig::OptId::UI_FILTER_SYSTEM);
            size_t sysCount = sysOpt->getArray().size();
            if (sysCount > 1) {
                sysOpt->setArrayMovePrev();
                updateRomList();
            }
        } else if (buttons & Input::Button::RT) {
            Option *sysOpt = pMain->getConfig()->get(PEMUConfig::OptId::UI_FILTER_SYSTEM);
            size_t sysCount = sysOpt->getArray().size();
            if (sysCount > 1) {
                sysOpt->setArrayMoveNext();
                updateRomList();
            }
        }
    }

    return true;
}

void UIRomList::onUpdate() {
    // demo mode, back from another program: the next game right away (the list stays hidden)
    if (mDemoPending) {
        if (pMain->getUiMessageBox()->isVisible()) {
            // a warning to read (network...): demo stopped, list shown
            mDemoPending = false;
            demo::end();
            setVisibility(Visibility::Visible);
        } else if (!pMain->getUiProgressBox()->isVisible()) {
            mDemoPending = false;
            startDemo();
            return;
        }
    }

    if (!isVisible() || pMain->getUiProgressBox()->isVisible()) {
        return;
    }

    // demo mode: back from another program (next game), or idle for long enough
    if (pMain->getUiMenu()->isVisible() || pMain->getUiStateMenu()->isVisible()
        || pMain->getUiMessageBox()->isVisible()) {
        mIdleClock.restart();
    } else {
        const int idle = demo::parseDuration(pMain->getConfig()->get(PEMUConfig::OptId::DEMO_START_AFTER)->getString());
        if (idle > 0 && mIdleClock.getElapsedTime().asSeconds() > (float) idle) {
            startDemo();
            return;
        }
    }

    unsigned int buttons = pMain->getInput()->getButtons();

    if (buttons > 0 && buttons != Input::Delay) {
        mTimerLoadInfo.restart();
        mTimerLoadVideo.restart();
    } else {
        if ((mTimerLoadInfoDone == 0) && mTimerLoadInfo.getElapsedTime().asMilliseconds() > mTimerLoadInfoDelay) {
            const Game game = pListBox->getSelection();
            pRomInfo->load(game);
            mTimerLoadInfoDone = 1;

            // missing images (main box and bottom box): download them in background
            // (shown as soon as they're there)
            auto artwork = pRomList->getArtwork(game);
            if (artwork && isArtworkAutoDownload() && !game.path.empty()) {
                std::vector<Artwork::Type> types;
                if (getPreview(game, PreviewType::Tex).empty() && !artwork->isMissing(game, getArtworkType())) {
                    types.push_back(getArtworkType());
                }
                if (pRomInfo->hasSecondImage() && game.synopsis.empty()) {
                    for (auto t: getSecondArtworkTypes()) {
                        if (artwork->findImage(game, t).empty() && !artwork->isMissing(game, t)) {
                            types.push_back(t);
                        }
                    }
                }
                artwork->request(game, types);
            }
        }
        if (mTimerLoadVideoDelay > 0 && mTimerLoadVideoDone == 0 &&
            mTimerLoadVideo.getElapsedTime().asMilliseconds() > mTimerLoadVideoDelay) {
            pRomInfo->loadVideo(pListBox->getSelection());
            mTimerLoadVideoDone = 1;
        }
    }

    bool dbReady = false;
    for (auto artwork: pRomList->artworks) {
        // first time (or old): get the game information database in background
        if (!mArtworkDbChecked && isArtworkAutoDownload() && artwork->needsDatabase()) {
            artwork->requestDatabase();
        }

        // game information downloaded: add it to the games
        if (artwork->takeDatabaseReady() && artwork->loadDatabase()) {
            const int updated = artwork->applyDatabase(pRomList->gameList);
            if (updated > 0) {
                pMain->getUiStatusBox()->show("GAME INFORMATION ADDED TO %i GAMES", updated);
            }
            dbReady = true;
        }

        // an image finished downloading in background: show it if its game is still selected
        const std::string name = artwork->takeDownloaded();
        if (!name.empty() && pListBox && Artwork::gameName(pListBox->getSelection()) == name) {
            pRomInfo->load(pListBox->getSelection());
        }

        // a background download problem (no internet...)
        const std::string notice = artwork->takeNotice();
        if (!notice.empty()) {
            pMain->getUiStatusBox()->show(notice);
        }
    }
    mArtworkDbChecked = true;

    // new game information: refresh the list, keep the selected game
    if (dbReady) {
        pRomList->updateFilterOptions();
        const int index = pListBox ? pListBox->getIndex() : 0;
        updateRomList();
        if (pListBox && index > 0 && index < (int) mGameList.games.size()) {
            pListBox->setSelection(index);
        }
    }

    RectangleShape::onUpdate();
}

Artwork::Type UIRomList::getArtworkType() {
    auto opt = pMain->getConfig()->get(PEMUConfig::OptId::ART_IMAGE_TYPE);
    return opt ? (Artwork::Type) opt->getArrayIndex() : Artwork::Type::Boxart;
}

bool UIRomList::selectGame(const std::string &fullPath) {
    if (!pListBox) return false;
    for (size_t i = 0; i < mGameList.games.size(); i++) {
        const Game &g = mGameList.games[i];
        if (g.romsPath + g.path == fullPath) {
            pListBox->setSelection((int) i);
            pRomInfo->load();
            mTimerLoadInfoDone = 0;
            mTimerLoadInfo.restart();
            return true;
        }
    }
    logLine("UIRomList::selectGame: %s not in the list", hidePassword(fullPath).c_str());
    return false;
}

void UIRomList::startDemo() {
    mIdleClock.restart();
    auto cfg = pMain->getConfig();

    // games to choose from
    std::vector<Game> games;
    const std::string which = cfg->get(PEMUConfig::OptId::DEMO_GAMES)->getString();
    const auto systems = cfg->getSystems();
    const std::vector<Game> &source = which == "CURRENT PAGE" ? mGameList.games
                                      : which == "FAVORITES" ? pRomList->gameListFav->games
                                      : pRomList->gameList->games;
    for (const auto &g: source) {
        if (!g.available || g.path.empty()) continue;
        // "SUPER NINTENDO ONLY" (pppEmu): this program's own console only, no program switching
        if (Utility::endsWith(which, " ONLY") && !systems.empty() && g.system.id != systems[0].system.id) continue;
        // cd images: big downloads and a bios, not for a demo
        const std::string lower = Utility::toLower(g.path);
        if (Utility::endsWith(lower, ".chd") || Utility::endsWith(lower, ".cue") || Utility::endsWith(lower, ".iso")) {
            continue;
        }
        games.push_back(g);
    }
    if (games.empty()) {
        demo::end();
        setVisibility(Visibility::Visible);
        pMain->getUiStatusBox()->show("DEMO MODE: NO GAMES TO SHOW");
        return;
    }

    // a random one, not the one shown last
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int) time(nullptr));
        seeded = true;
    }
    size_t pick = (size_t) rand() % games.size();
    if (games.size() > 1 && games[pick].romsPath + games[pick].path == mDemoLastGame) {
        pick = (pick + 1 + (size_t) rand() % (games.size() - 1)) % games.size();
    }
    const Game game = games[pick];
    mDemoLastGame = game.romsPath + game.path;

    const int seconds = demo::parseDuration(cfg->get(PEMUConfig::OptId::DEMO_TIME)->getString());
    const bool sound = cfg->get(PEMUConfig::OptId::DEMO_SOUND)->getInteger() != 0;
    logLine("demo: %s (%s), %i seconds", game.name.c_str(), game.system.name.c_str(), seconds);
    demo::begin(seconds, sound);

    // selected in the list: a button press during the demo comes back to it
    selectGame(game.romsPath + game.path);

    if (launchInOtherProgram(pMain, game)) {
        // run by another program: this one closes (or an error was shown)
        if (!pMain->done) {
            demo::end();
            setVisibility(Visibility::Visible);
        }
        return;
    }
#ifdef __MPV__
    pRomInfo->mpvTexture->setVisibility(c2d::Visibility::Hidden);
    pRomInfo->mpv->stop();
#endif
    cfg->loadGame(game);
    pMain->getUiEmu()->load(game);
}

void UIRomList::onDemoGameDone(const Game &game) {
    setVisibility(Visibility::Visible);
    if (demo::isActive() && demo::isNextWanted()) {
        demo::setNextWanted(false);
        startDemo();
        return;
    }
    // button pressed: back in the list on that game
    demo::end();
    selectGame(game.romsPath + game.path);
    pMain->getInput()->clear();
    mIdleClock.restart();
}

std::vector<Artwork::Type> UIRomList::getSecondArtworkTypes() {
    std::vector<Artwork::Type> types;
    for (auto t: {Artwork::Type::Boxart, Artwork::Type::Title, Artwork::Type::Snap}) {
        if (t != getArtworkType()) types.push_back(t);
    }
    return types;
}

bool UIRomList::isArtworkAutoDownload() {
    auto opt = pMain->getConfig()->get(PEMUConfig::OptId::ART_AUTO_DOWNLOAD);
    return opt && opt->getInteger();
}

UIRomList::~UIRomList() {
    printf("~UIRomList\n");
    delete (pRomList);
}
