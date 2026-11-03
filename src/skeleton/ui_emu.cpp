//
// Created by cpasjuste on 03/02/18.
//

#include <cmath>
#include "pemu.h"
#include "border_fx.h"

UiEmu::UiEmu(UiMain *u) : RectangleShape(u->getSize()) {
    printf("UiEmu()\n");

    pMain = u;
    RectangleShape::setFillColor(Color::Transparent);

    fpsText = new Text("0123456789", (unsigned int) pMain->getFontSize(), pMain->getSkin()->getFont());
    fpsText->setFillColor(Color::Yellow);
    fpsText->setOutlineColor(Color::Black);
    fpsText->setOutlineThickness(1);
    fpsText->setString("FPS: 00/60");
    fpsText->setPosition(16, 16);
    fpsText->setVisibility(Visibility::Hidden);
    RectangleShape::add(fpsText);

    RectangleShape::setVisibility(Visibility::Hidden);
}

void UiEmu::addAudio(c2d::Audio *_audio) {
    delete (audio);
    audio = _audio;
}

// demo mode without sound: the emulators play their sound as usual, silence is sent instead
class SilentAudio : public C2DAudio {
public:
    SilentAudio(int rate, int samples, Audio::C2DAudioCallback cb) : C2DAudio(rate, samples, cb) {}

    void play(const void *data, int samples, SyncMode syncMode) override {
        const size_t count = (size_t) samples * (size_t) getChannels();
        if (silence.size() < count) silence.resize(count, 0);
        C2DAudio::play(silence.data(), samples, syncMode);
    }

private:
    std::vector<int16_t> silence;
};

void UiEmu::addAudio(int rate, int samples, Audio::C2DAudioCallback cb) {
    c2d::Audio *aud;
    if (demo::isActive() && !demo::hasSound()) {
        aud = new SilentAudio(rate, samples, cb);
    } else {
        aud = new C2DAudio(rate, samples, cb);
    }
    addAudio(aud);
}

void UiEmu::addVideo(C2DUIVideo *v) {
    delete (video);
    video = v;
    video->setShader(pMain->getConfig()->get(PEMUConfig::OptId::EMU_SHADER, true)->getArrayIndex());
    video->setFilter((Texture::Filter) pMain->getConfig()->get(PEMUConfig::OptId::EMU_FILTER, true)->getArrayIndex());
    video->updateScaling();
    add(video);
}

void UiEmu::addVideo(uint8_t **pixels, int *pitch,
                     const c2d::Vector2i &size, const c2d::Vector2i &aspect, Texture::Format format) {
    auto *v = new C2DUIVideo(pMain, pixels, pitch, size, aspect, format);
    addVideo(v);
}

int UiEmu::load(const Game &game) {
    printf("UiEmu::load: name: %s, path: %s\n",
           game.path.c_str(), game.romsPath.c_str());
    if (demo::isActive()) {
        mDemoClock.restart();
        createDemoOverlay();
    } else {
        pMain->getUiStatusBox()->show("TIPS: PRESS MENU1 + MENU2 BUTTONS FOR IN GAME MENU...");
    }
    currentGame = game;

    // set fps text on top
    getFpsText()->setLayer(2);

    setVisibility(Visibility::Visible);
    pMain->getUiProgressBox()->setVisibility(Visibility::Hidden);
    pMain->getUiRomList()->setVisibility(Visibility::Hidden);
    pMain->getUiRomList()->getBlur()->setVisibility(Visibility::Hidden);

    resume();

    return 0;
}

void UiEmu::pause() {
    printf("UiEmu::pause()\n");
    if (audio != nullptr) {
        audio->pause(1);
    }
    // set ui input configuration
    pMain->updateInputMapping(false);
    // enable auto repeat delay
    pMain->getInput()->setRepeatDelay(INPUT_DELAY);

    paused = true;
}

void UiEmu::resume() {
    printf("UiEmu::resume()\n");
    // set per rom input configuration
    pMain->updateInputMapping(true);
    // disable auto repeat delay
    pMain->getInput()->setRepeatDelay(0);

#ifdef __VITA__
    config::Option *option = pMain->getConfig()->get(PEMUConfig::OptId::EMU_WAIT_RENDERING, true);
    if (option) {
        ((PSP2Renderer *) pMain)->setWaitRendering(option->getInteger());
    }
#endif

    if (audio) {
        audio->pause(0);
    }

    paused = false;
}

void UiEmu::stop() {
    printf("UiEmu::stop()\n");
    if (audio) {
        printf("Closing audio...\n");
        audio->pause(1);
        delete (audio);
        audio = nullptr;
    }

    if (video) {
        printf("Closing video...\n");
        delete (video);
        video = nullptr;
    }

#ifdef __VITA__
    ((PSP2Renderer *) pMain)->setWaitRendering(true);
#endif

    pMain->updateInputMapping(false);
    setVisibility(Visibility::Hidden);
    // nothing runs anymore (emulators check isPaused() before running a frame)
    paused = true;

    if (mExitOnStop) {
        pMain->setVisibility(Visibility::Hidden);
        getUi()->done = true;
    }
}

bool UiEmu::onInput(c2d::Input::Player *players) {
    if (pMain->getUiMenu()->isVisible() || pMain->getUiStateMenu()->isVisible()) {
        return C2DObject::onInput(players);
    }

    // demo mode: any button ends it (after a short delay: the buttons that started it may
    // still be held), the game gets no input
    if (demo::isActive()) {
        const unsigned int buttons = players[0].buttons & ~(Input::Button::Delay | Input::Button::Touch);
        if (buttons && isVisible() && mDemoClock.getElapsedTime().asSeconds() > 1.5f) {
            stopDemo(false);
        }
        return true;
    }

    // look for player 1 menu combo
    if (((players[0].buttons & Input::Button::Menu1) && (players[0].buttons & Input::Button::Menu2))) {
        pause();
        pMain->getUiMenu()->load(true);
        pMain->getInput()->clear();
        return true;
    }

    return C2DObject::onInput(players);
}

void UiEmu::stopDemo(bool next) {
    printf("UiEmu::stopDemo(%s)\n", next ? "next game" : "button pressed");
    // this game's list entry: selected again in the launcher's list
    const Game game = currentGame;
    if (next) demo::setNextWanted(true);
    // paused first, like when quitting from the in-game menu: some emulators (pgen...) would
    // otherwise run one more frame after stop(), with the video already deleted (crash)
    pause();
    // stopped while the demo is still on: the emulators don't write the game's saves
    stop();
    if (!next) demo::end();
    if (!mExitOnStop) {
        // game run by the launcher itself: back in the list, or the next demo game
        pMain->getUiRomList()->onDemoGameDone(game);
    }
    // else: started by the launcher, stop() closes this program, main() goes back to it
}

void UiEmu::createDemoOverlay() {
    if (mDemoBox) return;
    static const std::string message = "DEMO MODE  -  PRESS ANY BUTTON TO PLAY";
    const float s = pMain->getScaling().y;
    const auto size = (unsigned int) ((float) pMain->getFontSize() * 1.1f);
    Font *font = pMain->getSkin()->getFont();

    // letter positions from the whole line
    Text line(message, size, font);
    line.setOutlineThickness(2 * s);
    const FloatRect bounds = line.getLocalBounds();
    const float padX = 20 * s, padY = 10 * s;

    mDemoBox = new RectangleShape({bounds.width + padX * 2, (float) size + padY * 2});
    mDemoBox->setFillColor({0, 0, 0, 170});
    mDemoBox->setOutlineColor({247, 59, 146, 255});
    mDemoBox->setOutlineThickness(2 * s * borderfx::BORDER_SCALE);
    mDemoBox->setOrigin(Origin::Bottom);
    mDemoBox->setPosition(pMain->getSize().x / 2, pMain->getSize().y - 28 * s);
    mDemoCharsY = padY - bounds.top + (float) size * 0.05f;
    for (size_t i = 0; i < message.size(); i++) {
        if (message[i] == ' ') continue;
        auto *t = new Text(std::string(1, message[i]), size, font);
        t->setOutlineThickness(2 * s);
        t->setOutlineColor(Color::Black);
        const float x = padX + line.findCharacterPos(i).x - bounds.left;
        t->setPosition(x, mDemoCharsY);
        mDemoBox->add(t);
        mDemoChars.push_back(t);
        mDemoCharsX.push_back(x);
    }
    add(mDemoBox);
    mDemoBox->setLayer(10);     // over the game (after add: add() sorts before inserting)
    mDemoBox->setVisibility(Visibility::Hidden);

    // the logo, as in the game list (skin: MAIN > TITLE), at the bottom right
    const auto logo = pMain->getSkin()->getRectangleShape({"MAIN", "TITLE"});
    if (!logo.texture.empty() && logo.rect.width > 0 && logo.rect.height > 0) {
        for (const auto &dir: pMain->getSkin()->getPaths()) {
            if (!pMain->getIo()->exist(dir + logo.texture)) continue;
            auto *tex = new C2DTexture(dir + logo.texture);
            if (!tex->available || tex->getSize().x <= 0 || tex->getSize().y <= 0) {
                delete (tex);
                break;
            }
            tex->setFilter(Texture::Filter::Linear);
            tex->setScale(logo.rect.width / tex->getSize().x, logo.rect.height / tex->getSize().y);
            tex->setOrigin(Origin::BottomRight);
            tex->setPosition(pMain->getSize().x - logo.rect.left, pMain->getSize().y - logo.rect.top);
            add(tex);
            tex->setLayer(10);
            tex->setVisibility(Visibility::Hidden);
            mDemoLogo = tex;
            break;
        }
    }
}

void UiEmu::updateDemoOverlay() {
    if (!mDemoBox) return;
    const bool show = demo::isActive() && isVisible();
    if (show != mDemoBox->isVisible()) {
        mDemoBox->setVisibility(show ? Visibility::Visible : Visibility::Hidden);
        if (mDemoLogo) mDemoLogo->setVisibility(show ? Visibility::Visible : Visibility::Hidden);
    }
    if (!show) return;

    if (const auto opt = pMain->getConfig()->get(PEMUConfig::OptId::UI_BORDER_ANIMATION)) {
        borderfx::setSpeed(opt->getString());
    }
    const float time = borderfx::getTime();
    const float speed = borderfx::getWaveSpeed();
    const float s = pMain->getScaling().y;
    for (size_t i = 0; i < mDemoChars.size(); i++) {
        // rainbow flowing along the letters, and a small wave (still if the animation is OFF)
        mDemoChars[i]->setFillColor(borderfx::getColor((float) i * 0.04f - time * speed));
        const float bob = speed > 0 ? std::sin(time * 6.0f - (float) i * 0.45f) * 3 * s : 0;
        mDemoChars[i]->setPosition(mDemoCharsX[i], mDemoCharsY + bob);
    }
    // the box's border: colours like the ui's (the ui's own animation is off during games)
    borderfx::update(this, pMain->getSize().x, pMain->getSize().y);
}

void UiEmu::onUpdate() {
    C2DObject::onUpdate();
    updateDemoOverlay();

    // demo mode: time is up, next game
    if (demo::isActive() && isVisible() && !paused
        && mDemoClock.getElapsedTime().asSeconds() > (float) demo::getSeconds()) {
        stopDemo(true);
        return;
    }

    if (!isPaused()) {
        // fps
        bool showFps = pMain->getConfig()->get(PEMUConfig::OptId::EMU_SHOW_FPS, true)->getInteger();
        if (showFps) {
            if (!fpsText->isVisible()) {
                fpsText->setVisibility(c2d::Visibility::Visible);
            }
            sprintf(fpsString, "FPS: %.1f/%.1f", pMain->getFps(), targetFps);
            fpsText->setString(fpsString);
        } else {
            if (fpsText->isVisible()) {
                fpsText->setVisibility(c2d::Visibility::Hidden);
            }
        }
    }
}

UiMain *UiEmu::getUi() {
    return pMain;
}

C2DUIVideo *UiEmu::getVideo() {
    return video;
}

c2d::Audio *UiEmu::getAudio() {
    return audio;
}

c2d::Text *UiEmu::getFpsText() {
    return fpsText;
}

bool UiEmu::isPaused() {
    return paused;
}
