//
// Small debug log (see pemu_log.h)
//

#include <cstdio>
#include <cstdarg>
#include <unistd.h>
#include "pemu_log.h"

static std::string s_logPath;

void pemu::logInit(const std::string &dataPath) {
    s_logPath = dataPath + "pppsnes.log";
    if (FILE *f = fopen(s_logPath.c_str(), "w")) fclose(f);
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
