//
// pppEmu attract mode (see demo.h)
//

#include <cstdlib>
#include "demo.h"

static bool s_active = false;
static int s_seconds = 60;
static bool s_sound = true;
static bool s_next = false;

void pemu::demo::begin(int seconds, bool sound) {
    s_active = true;
    s_seconds = seconds > 0 ? seconds : 60;
    s_sound = sound;
    s_next = false;
}

void pemu::demo::end() {
    s_active = false;
    s_next = false;
}

bool pemu::demo::isActive() {
    return s_active;
}

int pemu::demo::getSeconds() {
    return s_seconds;
}

bool pemu::demo::hasSound() {
    return s_sound;
}

void pemu::demo::setNextWanted(bool value) {
    s_next = value;
}

bool pemu::demo::isNextWanted() {
    return s_next;
}

int pemu::demo::parseDuration(const std::string &value) {
    const int n = atoi(value.c_str());
    if (n <= 0) return 0;
    return value.find("MIN") != std::string::npos ? n * 60 : n;
}
