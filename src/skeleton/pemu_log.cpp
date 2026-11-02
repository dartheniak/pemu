//
// Small debug log (see pemu_log.h)
//

#include <cstdio>
#include <cstdarg>
#include <unistd.h>
#include "pemu_log.h"

static std::string s_logPath;
static bool s_keep = false;

void pemu::logKeepPrevious() {
    s_keep = true;
}

void pemu::logInit(const std::string &dataPath) {
    s_logPath = dataPath + "pppemu.log";
    if (!s_keep) {
        if (FILE *f = fopen(s_logPath.c_str(), "w")) fclose(f);
    }
    s_keep = false;
}

void pemu::logLine(const char *fmt, ...) {
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    printf("%s\n", msg);
    if (s_logPath.empty()) return;
    if (FILE *f = fopen(s_logPath.c_str(), "a")) {
        fprintf(f, "%s\n", msg);
        fflush(f);
        fsync(fileno(f));
        fclose(f);
    }
}
