//
// pppEmu: animated background (see neon_background.h)
//

#include <cstring>
#include "pemu.h"
#include "pemu_log.h"
#include "nh_bg.h"
#include "border_fx.h"
#include "neon_background.h"

using namespace c2d;
using namespace pemu;

// the grey scene (SYSTEMS in nh_bg.c)
#define SCENE_ID "mono"

// colour loop: this many times slower than the borders' colours (NORMAL: about 8 s per loop)
#define CYCLE_SLOWER 10.0f

NeonBackground::NeonBackground(UiMain *ui) {
    pMain = ui;
}

NeonBackground::~NeonBackground() {
    nh_bg_destroy(mNh);
    mNh = nullptr;
    // the texture belongs to the ui (deleted with it)
}

void NeonBackground::start() {
    if (mNh || mFailed) return;
    mFailed = true;

    // the screen's size; colours in memory order R, G, B, A (the texture is GL_RGBA /
    // GL_UNSIGNED_BYTE: 0xAABBGGRR as a little endian uint32 = NH_ABGR8888)
    nh_settings s = *nh_default_settings();
    s.w = (int) pMain->getSize().x;
    s.h = (int) pMain->getSize().y;
    mTexture = new C2DTexture(Vector2i(s.w, s.h), Texture::Format::RGBA8);
    if (!mTexture->available) {
        delete (mTexture);
        mTexture = nullptr;
        logLine("background: texture not available");
        return;
    }
    mTexture->setFilter(Texture::Filter::Point);
    mTexture->setVisibility(Visibility::Hidden);    // until the first frame is drawn
    pMain->add(mTexture);
    mTexture->setLayer(-1000);  // behind everything (after add: add() sorts before inserting)

    // disk cache, shared by all of pppEmu's programs ("/data/pppemu/cache/background", not
    // "/data/pppemu/pgen/cache/..."): only the grey scene is used now, older files go
    std::string root = pMain->getIo()->getDataPath();
    for (const char *p: {"pgen/", "pnes/", "pgba/", "pfbneo/"}) {
        if (root.size() > strlen(p) && root.compare(root.size() - strlen(p), strlen(p), p) == 0
            && root[root.size() - strlen(p) - 1] == '/') {
            root.resize(root.size() - strlen(p));
            break;
        }
    }
    const std::string dir = root + "cache/background";
    pMain->getIo()->create(root + "cache");
    pMain->getIo()->create(dir);
    for (const auto &f: pMain->getIo()->getDirList(dir)) {
        if (f.type == c2d::Io::Type::File && f.name.rfind(SCENE_ID "_", 0) != 0) {
            pMain->getIo()->removeFile(dir + "/" + f.name);
        }
    }

    mNh = nh_bg_create_async(&s, NH_ABGR8888, SCENE_ID, dir.c_str());
    if (!mNh) {
        logLine("background: could not be started");
        return;
    }
    mFailed = false;
    mClock.restart();
    logLine("background: started %ix%i", s.w, s.h);
}

void NeonBackground::update(bool visible) {
    if (visible && !mNh) start();
    if (!mNh || !mTexture) return;
    if (!visible) {
        if (mTexture->isVisible()) mTexture->setVisibility(Visibility::Hidden);
        mClock.restart();
        return;
    }

    // time since the last frame (no jump after a pause or a game)
    double dt = mClock.restart().asSeconds();
    if (dt > 0.25) dt = 1.0 / 60;

    // the texture's pixels stay from frame to frame: only the changed rows (sun, grid) are
    // redrawn and sent to the GPU
    uint8_t *pixels;
    int pitch;
    const auto size = mTexture->getTextureRect();
    mTexture->lock(&pixels, &pitch, {0, 0, size.width, size.height});
    int y0 = 0, y1 = 0;
    const int drawn = nh_bg_render_rows(mNh, dt, (uint32_t *) pixels, pitch / 4, &y0, &y1);
    if (y1 > y0) {
        mTexture->lock(&pixels, &pitch, {0, y0, size.width, y1 - y0});
        mTexture->unlock();
    }

    // rainbow: the grey scene times the cycling colour (done by the GPU)
    const float pos = borderfx::getTime() * borderfx::getWaveSpeed() / CYCLE_SLOWER;
    mTexture->setFillColor(borderfx::getColor(pos));

    if (drawn && !mTexture->isVisible()) mTexture->setVisibility(Visibility::Visible);
}
