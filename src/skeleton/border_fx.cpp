//
// pppEmu: animated borders (see border_fx.h)
//

#include <chrono>
#include <cmath>
#include "cross2d/c2d.h"
#include "border_fx.h"

using namespace c2d;

namespace {
    // the lines' vertices are kept by Shape for its derived classes: reached with pointers to
    // members formed through a derived class (allowed for protected members)
    struct ShapeAccess : public Shape {
        static VertexArray Shape::*outline() { return &ShapeAccess::m_outlineVertices; }

        static bool Shape::*dirty() { return &ShapeAccess::m_shape_dirty; }

        static void (Shape::*rebuild())() { return &ShapeAccess::update; }
    };

    // kiwi logo colours, in a loop
    const float PALETTE[][3] = {
            {247, 59,  146},    // pink
            {160, 75,  235},    // purple
            {52,  156, 211},    // blue
            {40,  210, 230},    // cyan
            {249, 212, 59},     // yellow
            {242, 110, 60},     // orange
    };
    const int PALETTE_SIZE = sizeof(PALETTE) / sizeof(PALETTE[0]);

    // turns per second of the colours around a box, pulses per second of the selection
    float s_wave = 0.12f;
    float s_pulse = 0.8f;
    bool s_still = false;

    std::chrono::steady_clock::time_point s_start = std::chrono::steady_clock::now();

    const float TWO_PI = 6.28318530718f;

    float fract(float v) { return v - std::floor(v); }

    // position 0..1 in the palette loop -> colour (blended between two colours)
    void paletteColor(float pos, float *rgb) {
        const float p = fract(pos) * (float) PALETTE_SIZE;
        const int i = (int) p % PALETTE_SIZE;
        const int j = (i + 1) % PALETTE_SIZE;
        float f = p - std::floor(p);
        f = f * f * (3 - 2 * f);    // smooth
        for (int k = 0; k < 3; k++) rgb[k] = PALETTE[i][k] + (PALETTE[j][k] - PALETTE[i][k]) * f;
    }

    uint8_t clampByte(float v) { return (uint8_t) (v < 0 ? 0 : v > 255 ? 255 : v); }

    void colorShape(Shape *shape, float offset, float time) {
        // geometry changed since the last frame: rebuilt now, else drawing would rebuild
        // it (and its colours) after this
        if (shape->*ShapeAccess::dirty()) (shape->*ShapeAccess::rebuild())();

        VertexArray &lines = shape->*ShapeAccess::outline();
        const size_t count = lines.getVertexCount();
        if (count == 0) return;

        const FloatRect b = shape->getLocalBounds();
        const float cx = b.left + b.width / 2, cy = b.top + b.height / 2;
        const bool selected = shape->getName() == pemu::borderfx::SELECTED_NAME;
        const float phase = s_still ? 0 : time * s_wave;
        const float pulse = s_still ? 1 : 0.5f + 0.5f * std::sin(time * s_pulse * TWO_PI);
        const uint8_t alpha = shape->getOutlineColor().a;

        float rgb[3];
        for (size_t i = 0; i < count; i++) {
            // colour by angle around the box: the whole loop once around it
            const Vector2f &p = lines[i].position;
            const float angle = std::atan2(p.y - cy, p.x - cx) / (TWO_PI);
            paletteColor(angle - phase - offset, rgb);
            if (selected) {
                // pulse: dimmer .. brighter (towards white)
                for (float &c: rgb) c = (c * (0.55f + 0.45f * pulse)) + (255 - c) * 0.35f * pulse;
            }
            lines[i].color = {clampByte(rgb[0]), clampByte(rgb[1]), clampByte(rgb[2]), alpha};
        }
        lines.update();

        if (selected) {
            // the selection's fill: same colours, pulsing (its alpha is the skin's at most)
            VertexArray *fill = shape->getVertexArray();
            const uint8_t fillAlpha = shape->getFillColor().a;
            if (fill && fill->getVertexCount() > 0 && fillAlpha > 0) {
                paletteColor(-phase - offset, rgb);
                const Color c = {clampByte(rgb[0]), clampByte(rgb[1]), clampByte(rgb[2]),
                                 clampByte((float) fillAlpha * (0.45f + 0.55f * pulse))};
                for (size_t i = 0; i < fill->getVertexCount(); i++) (*fill)[i].color = c;
                fill->update();
            }
        }
    }

    void walk(C2DObject *obj, float x, float y, float width, float height, float time) {
        if (!obj || !obj->isVisible()) return;
        const Vector2f &pos = obj->getPosition();
        x += pos.x;
        y += pos.y;

        // shapes and textures are Shape objects (no dynamic_cast: rtti may be off)
        const Type type = obj->getType();
        auto *shape = type == Type::Shape || type == Type::Texture ? static_cast<Shape *>(obj) : nullptr;
        if (shape && shape->getOutlineThickness() > 0) {
            const Color &c = shape->getOutlineColor();
            // black outlines (text like borders, logo) are left as they are
            if (c.a > 0 && (int) c.r + c.g + c.b >= 60) {
                // boxes further right / down get the colours a little later: the wave travels
                colorShape(shape, (x / width + y / height) * 0.35f, time);
            }
        }

        for (auto *child: obj->getChilds()) {
            walk(child, x, y, width, height, time);
        }
    }
}

void pemu::borderfx::setSpeed(const std::string &speed) {
    s_still = speed == "OFF";
    // turns per second of the colours around a box (about 3x faster than in 1.05)
    if (speed == "SLOW") {
        s_wave = 0.15f;
        s_pulse = 0.7f;
    } else if (speed == "FAST") {
        s_wave = 0.90f;
        s_pulse = 1.6f;
    } else {
        s_wave = 0.40f;
        s_pulse = 1.1f;
    }
}

float pemu::borderfx::getTime() {
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - s_start).count();
}

float pemu::borderfx::getWaveSpeed() {
    return s_still ? 0 : s_wave;
}

c2d::Color pemu::borderfx::getColor(float pos) {
    float rgb[3];
    paletteColor(pos, rgb);
    return {clampByte(rgb[0]), clampByte(rgb[1]), clampByte(rgb[2]), 255};
}

void pemu::borderfx::update(C2DObject *root, float width, float height) {
    if (!root || width <= 0 || height <= 0) return;
    const float time = getTime();
    // the root (the window) has no position of its own to add
    for (auto *child: root->getChilds()) {
        walk(child, 0, 0, width, height, time);
    }
}
