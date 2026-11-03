//
// pppEmu: animated borders in the kiwi logo's colours (UI_OPTIONS > BORDER_ANIMATION).
//
// Every visible rectangle border of the ui (game list, boxes, menus, message boxes) is drawn
// with the logo's colours (pink, purple, blue, cyan, yellow, orange) flowing around it, each box
// a little later than the one before so the colours travel across the screen ("colour wave").
// The selection (game list, menus) also pulses: brighter / dimmer, its fill in the same colour.
// Only the colours of the drawn lines change (no shape, size or skin value is changed), so the
// rest of the ui is not affected. OFF: the same colours, still.
//

#ifndef PEMU_BORDER_FX_H
#define PEMU_BORDER_FX_H

#include <string>
#include "cross2d/skeleton/sfml/Color.hpp"

namespace c2d {
    class C2DObject;
}

namespace pemu {
    namespace borderfx {
        // box borders are drawn this many times thicker than the skin says
        static const float BORDER_SCALE = 1.0f;

        // name given to the selection rectangles (they pulse)
        static const char *const SELECTED_NAME = "pppemu_selected";

        // "OFF", "SLOW", "NORMAL", "FAST"
        void setSpeed(const std::string &speed);

        // seconds since start, colours turns per second (0 if OFF)
        float getTime();

        float getWaveSpeed();

        // kiwi logo colour at "pos" in the colour loop (0..1, repeats)
        c2d::Color getColor(float pos);

        // colours of the visible borders under "root" (called once per frame, before drawing)
        void update(c2d::C2DObject *root, float width, float height);
    }
}

#endif //PEMU_BORDER_FX_H
