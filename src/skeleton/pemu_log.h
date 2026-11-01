//
// Small debug log written to "<data path>/pppsnes.log", flushed to disk at each line:
// on the PS4 the app's printf output isn't visible, and a crash kills the app at once,
// so the last line of this file shows the last step that ran.
//

#ifndef PEMU_LOG_H
#define PEMU_LOG_H

#include <string>

namespace pemu {
    // start a new log (previous session's log is replaced)
    void logInit(const std::string &dataPath);

    // printf-like, also printed to stdout
    void logLine(const char *fmt, ...);
}

#endif //PEMU_LOG_H
