//
// Created by cpasjuste on 28/09/18.
//

#include "skeleton/pemu.h"
#include "skeleton/pemu_log.h"
#include "pgen_ui_menu_state.h"

extern "C" {
#include "shared.h"
}

bool PGENUIStateMenu::loadStateCore(const char *path) {
    bool ret = false;
    const auto buf = (uint8_t *) calloc(1, STATE_SIZE);
    // states are saved with their real size (smaller than STATE_SIZE, see saveStateCore):
    // the old "size == STATE_SIZE" check refused every saved state
    const auto size = buf ? getUi()->getIo()->read(path, (char *) buf, STATE_SIZE) : 0;
    if (size > 0) {
        ret = state_load(buf) > 0;
    }
    pemu::logLine("PGENUIStateMenu::loadStateCore: %s (%i bytes): %s", path, (int) size, ret ? "ok" : "failed");

    if (buf) {
        free(buf);
    }

    return ret;
}

bool PGENUIStateMenu::saveStateCore(const char *path) {
    const auto buf = (uint8_t *) malloc(STATE_SIZE);
    bool ret = false;
    if (buf) {
        const auto len = state_save(buf);
        ret = getUi()->getIo()->write(path, (const char *) buf, len);
        free(buf);
    }

    return ret;
}
