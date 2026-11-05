//
// pppEmu: animated background (see neon_background.h)
//

#include "pemu.h"
#include "pemu_log.h"
#include "nh_bg.h"
#include "neon_background.h"

using namespace c2d;
using namespace pemu;

// our console pages, in the L2 / R2 order, and the module's ids (see SYSTEMS in nh_bg.c)
static const struct {
    const char *page, *id;
} PAGES[] = {
        {"ALL",              "default"},
        {"Master System",    "sms"},
        {"Game Gear",        "gg"},
        {"Mega Drive",       "genesis"},
        {"Sega CD",          "segacd"},
        {"NES",              "nes"},
        {"Game Boy",         "gb"},
        {"Game Boy Color",   "gbc"},
        {"Game Boy Advance", "gba"},
        {"Super Nintendo",   "snes"},
        {"Arcade",           "arcade"},
};

NeonBackground::NeonBackground(UiMain *ui) {
    pMain = ui;
}

NeonBackground::~NeonBackground() {
    nh_bg_destroy(mNh);
    mNh = nullptr;
    // the texture belongs to the ui (deleted with it)
}

const char *NeonBackground::systemId(const std::string &page) {
    for (const auto &p: PAGES) {
        if (page == p.page) return p.id;
    }
    return "default";
}

void NeonBackground::start(const char *id) {
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

    const std::string dir = pMain->getIo()->getDataPath() + "cache/background";
    pMain->getIo()->create(pMain->getIo()->getDataPath() + "cache");
    pMain->getIo()->create(dir);
    mNh = nh_bg_create_async(&s, NH_ABGR8888, id, dir.c_str());
    if (!mNh) {
        logLine("background: could not be started");
        return;
    }
    mFailed = false;
    mId = id;

    // the other pages into the disk cache while idle (only missing ones are built)
    const char *ids[sizeof(PAGES) / sizeof(PAGES[0])];
    int n = 0;
    for (const auto &p: PAGES) ids[n++] = p.id;
    nh_bg_prebuild(mNh, ids, n);

    mClock.restart();
    logLine("background: started %ix%i, %s", s.w, s.h, id);
}

void NeonBackground::setPage(const std::string &page) {
    const char *id = systemId(page);
    if (!mNh) {
        if (mEnabled) start(id);
        return;
    }
    if (mId == id) return;
    mId = id;
    nh_bg_set_system(mNh, id);
    logLine("background: %s", id);
}

void NeonBackground::update(bool visible) {
    if (!mNh || !mTexture) return;
    nh_bg_set_idle_work(mNh, visible ? 1 : 0);
    if (!visible) {
        if (mTexture->isVisible()) mTexture->setVisibility(Visibility::Hidden);
        mClock.restart();
        return;
    }

    // time since the last frame (no jump after a pause or a game)
    double dt = mClock.restart().asSeconds();
    if (dt > 0.25) dt = 1.0 / 60;

    // the texture's pixels stay from frame to frame: only the changed rows (sun, grid, or
    // everything while switching page) are redrawn and sent to the GPU
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
    if (drawn && !mTexture->isVisible()) mTexture->setVisibility(Visibility::Visible);
}
