//
// pppEmu: games of other consoles run in another program of the package (see launcher.h)
//

#include <cstring>
#include <algorithm>
#include <cerrno>
#include <unistd.h>
#include <sys/stat.h>
#include "pemu.h"
#include "launcher.h"
#include "pemu_log.h"
#include "romset_list.h"

#ifdef __SMB__
#include "smb_io.h"
#endif

#ifdef __PS4__
extern "C" int sceSystemServiceLoadExec(const char *path, const char *args[]);
#endif

using namespace c2d;
using namespace pemu;

#define ARG_ROM "--pppemu-rom="
#define ARG_RETURN "--pppemu-return="
#define ARG_SELECT "--pppemu-select="
#define ARG_SETTINGS "--pppemu-settings="
#define ARG_SYSTEM "--pppemu-system="
#define ARG_STATE "--pppemu-state="
#define ARG_DEMO "--pppemu-demo="
#define ARG_DEMO_NEXT "--pppemu-demo-next"
#define SHARED_SETTINGS_FILE "shared_settings.cfg"
// desktop builds only: data folder of the program to go back to (its working directory)
#define ARG_CWD "--pppemu-cwd="

static bool startsWith(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

std::string pemu::hidePassword(const std::string &text) {
    // network paths carry the share password ("smb://user:password@..."): not written to the log
    std::string shown = text;
    const size_t scheme = shown.find("://");
    const size_t at = scheme == std::string::npos ? scheme : shown.find('@', scheme);
    const size_t colon = at == std::string::npos ? at : shown.find(':', scheme + 3);
    if (colon != std::string::npos && colon < at) shown.replace(colon + 1, at - colon - 1, "***");
    return shown;
}

LaunchArgs pemu::parseLaunchArgs(int argc, char **argv) {
    LaunchArgs args;
    std::string cwd;
    // every entry is checked: the ps4 may or may not put the program path first
    for (int i = 0; i < argc; i++) {
        if (!argv[i]) continue;
        if (startsWith(argv[i], ARG_ROM)) {
            args.rom = argv[i] + strlen(ARG_ROM);
            args.pppemu = true;
        } else if (startsWith(argv[i], ARG_RETURN)) {
            args.returnTo = argv[i] + strlen(ARG_RETURN);
            args.pppemu = true;
        } else if (startsWith(argv[i], ARG_SELECT)) {
            args.select = argv[i] + strlen(ARG_SELECT);
            args.pppemu = true;
        } else if (startsWith(argv[i], ARG_DEMO_NEXT)) {
            args.demoNext = true;
            args.pppemu = true;
        } else if (startsWith(argv[i], ARG_DEMO)) {
            // "<seconds>,<sound 0/1>"
            const char *v = argv[i] + strlen(ARG_DEMO);
            args.demoSeconds = atoi(v);
            const char *comma = strchr(v, ',');
            args.demoSound = !comma || atoi(comma + 1) != 0;
        } else if (startsWith(argv[i], ARG_STATE)) {
            args.state = argv[i] + strlen(ARG_STATE);
        } else if (startsWith(argv[i], ARG_SYSTEM)) {
            args.system = argv[i] + strlen(ARG_SYSTEM);
        } else if (startsWith(argv[i], ARG_SETTINGS)) {
            args.settings = argv[i] + strlen(ARG_SETTINGS);
        } else if (startsWith(argv[i], ARG_CWD)) {
            cwd = argv[i] + strlen(ARG_CWD);
        }
    }
    // pEMU's own command line: "program <rom file>"
    if (!args.pppemu && argc > 1 && argv[1] && argv[1][0] != '-') {
        args.rom = argv[1];
    }
#ifndef __PS4__
    // keep the launcher's data folder (desktop builds use the working directory)
    if (!cwd.empty()) args.returnTo += std::string("\n") + cwd;
#endif
    return args;
}

std::string pemu::selfProgramPath() {
#ifdef __PS4__
    return "/app0/eboot.bin";
#else
    char buf[1024];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = 0;
    return buf;
#endif
}

bool pemu::execProgram(const std::string &program, const std::vector<std::string> &args, std::string *error) {
    logLine("launcher: starting %s", program.c_str());
    for (const auto &a: args) logLine("launcher:   %s", hidePassword(a).c_str());

    std::vector<const char *> argv;
#ifdef __PS4__
    for (const auto &a: args) argv.push_back(a.c_str());
    argv.push_back(nullptr);
    const int res = sceSystemServiceLoadExec(program.c_str(), argv.data());
    // only returns on errors
    logLine("launcher: sceSystemServiceLoadExec failed (0x%08x)", (unsigned) res);
    if (error) {
        char msg[64];
        snprintf(msg, sizeof(msg), "could not start the program (0x%08x)", (unsigned) res);
        *error = msg;
    }
#else
    argv.push_back(program.c_str());
    for (const auto &a: args) argv.push_back(a.c_str());
    argv.push_back(nullptr);
    execv(program.c_str(), (char *const *) argv.data());
    logLine("launcher: exec failed (%s)", strerror(errno));
    if (error) *error = std::string("could not start the program (") + strerror(errno) + ")";
#endif
    return false;
}

// file names in a ".cue" sheet: FILE "Game (Track 01).bin" BINARY
static std::vector<std::string> cueTracks(const std::string &cuePath) {
    std::vector<std::string> tracks;
    FILE *f = fopen(cuePath.c_str(), "r");
    if (!f) return tracks;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        std::string l = line;
        const size_t k = l.find_first_not_of(" \t");
        if (k == std::string::npos || Utility::toLower(l.substr(k, 5)) != "file ") continue;
        std::string name;
        const size_t q1 = l.find('"', k);
        const size_t q2 = q1 == std::string::npos ? q1 : l.find('"', q1 + 1);
        if (q2 != std::string::npos) {
            name = l.substr(q1 + 1, q2 - q1 - 1);
        } else {
            // no quotes: FILE track.bin BINARY
            const size_t s = l.find_first_not_of(" \t", k + 5);
            const size_t e = s == std::string::npos ? s : l.find_first_of(" \t\r\n", s);
            if (s != std::string::npos) name = l.substr(s, e == std::string::npos ? std::string::npos : e - s);
        }
        for (auto &c: name) if (c == '\\') c = '/';
        if (!name.empty() && std::find(tracks.begin(), tracks.end(), name) == tracks.end()) {
            tracks.push_back(name);
        }
    }
    fclose(f);
    return tracks;
}

// a launch waiting for the ui to be closed (see main.cpp)
static std::string s_pendingProgram;
static std::vector<std::string> s_pendingArgs;

std::string pemu::programDataPath(UiMain *ui, const ss_api::Game &game) {
    for (const auto &sys: ui->getConfig()->getSystems()) {
        if (sys.system.id != game.system.id || sys.program.empty()) continue;
        std::string name = sys.program.substr(sys.program.find_last_of('/') + 1);
#ifdef __PS4__
        // "/app0/pgen.bin" -> "/data/pppemu/pgen/" (see the programs' io classes)
        if (Utility::endsWith(name, ".bin")) name = name.substr(0, name.size() - 4);
        return ui->getIo()->getDataPath() + name + "/";
#else
        // desktop builds: the program's folder (its working directory, see runPendingLaunch)
        return sys.program.substr(0, sys.program.find_last_of('/') + 1);
#endif
    }
    return "";
}

bool pemu::launchInOtherProgram(UiMain *ui, const ss_api::Game &game, const std::string &state) {
    // console of this game, run by another program?
    std::string program;
    for (const auto &sys: ui->getConfig()->getSystems()) {
        if (sys.system.id == game.system.id) {
            program = sys.program;
            break;
        }
    }
    if (program.empty()) return false;

    struct stat st{};
    if (stat(program.c_str(), &st) != 0) {
        ui->getUiMessageBox()->show("ERROR", "The program for " + game.system.name +
                                             " games is missing (" + program + ").", "OK");
        return true;
    }

    std::string fullPath = game.romsPath + game.path;
#ifdef __SMB__
    if (SmbIo::isSmbPath(game.romsPath)) {
        // network game: downloaded to the local cache first, the other program opens a local file
        auto progress = ui->getUiProgressBox();
        progress->setTitle(game.name);
        progress->setMessage("Downloading...");
        progress->setProgress(0);
        progress->setVisibility(Visibility::Visible);
        progress->setLayer(1000);
        ui->flip();
        std::string error;
        const std::string cacheDir = SmbIo::getCacheDir(ui->getUiRomList()->getRomList()->getSmbCachePath(),
                                                        game.romsPath);
        fullPath = SmbIo::fetchToCache(game.romsPath, game.path, cacheDir,
                                       [ui](uint64_t done, uint64_t total) {
                                           const float p = total > 0 ? (float) done / (float) total : 1.0f;
                                           ui->getUiProgressBox()->setProgress(p);
                                           ui->flip();
                                           return true;
                                       }, &error);
        // arcade: the game's parent zips and the bios / device files of its folder are needed
        // too (fbneo looks for them next to the game). Missing ones are skipped.
        const PEMUConfig::CoreSystem *sysDef = nullptr;
        const auto systems = ui->getConfig()->getSystems();
        for (const auto &sys: systems) {
            if (sys.system.id == game.system.id) sysDef = &sys;
        }
        if (!fullPath.empty() && sysDef && !sysDef->romsetList.empty()) {
            const std::string folder = game.path.find('/') != std::string::npos
                                       ? game.path.substr(0, game.path.find_last_of('/') + 1) : "";
            std::vector<std::string> extra;
            if (RomsetList *list = RomsetList::get(sysDef->romsetList)) {
                for (const auto &p: list->parents(game.path)) extra.push_back(folder + p + ".zip");
            }
            for (const auto &h: ui->getUiRomList()->getRomList()->hiddenFiles) {
                if (h.romsPath == game.romsPath) extra.push_back(h.path);
            }
            for (size_t i = 0; i < extra.size(); i++) {
                progress->setMessage("Downloading " + extra[i].substr(extra[i].find_last_of('/') + 1) + "...");
                progress->setProgress(0);
                ui->flip();
                std::string err;
                if (SmbIo::fetchToCache(game.romsPath, extra[i], cacheDir,
                                        [ui](uint64_t done, uint64_t total) {
                                            const float p = total > 0 ? (float) done / (float) total : 1.0f;
                                            ui->getUiProgressBox()->setProgress(p);
                                            ui->flip();
                                            return true;
                                        }, &err).empty()) {
                    logLine("launcher: %s not downloaded (%s)", extra[i].c_str(), err.c_str());
                }
            }
        }

        // cd image as ".cue": its track files ("FILE "Game (Track 1).bin" BINARY") are needed too
        if (!fullPath.empty() && Utility::endsWith(Utility::toLower(fullPath), ".cue")) {
            const std::string folder = game.path.find('/') != std::string::npos
                                       ? game.path.substr(0, game.path.find_last_of('/') + 1) : "";
            const auto tracks = cueTracks(fullPath);
            for (size_t i = 0; i < tracks.size() && !fullPath.empty(); i++) {
                progress->setMessage("Downloading track " + std::to_string(i + 1) + " / " +
                                     std::to_string(tracks.size()) + "...");
                progress->setProgress(0);
                ui->flip();
                if (SmbIo::fetchToCache(game.romsPath, folder + tracks[i], cacheDir,
                                        [ui](uint64_t done, uint64_t total) {
                                            const float p = total > 0 ? (float) done / (float) total : 1.0f;
                                            ui->getUiProgressBox()->setProgress(p);
                                            ui->flip();
                                            return true;
                                        }, &error).empty()) {
                    error = tracks[i] + ": " + error;
                    fullPath.clear();
                }
            }
        }
        progress->setVisibility(Visibility::Hidden);
        if (fullPath.empty()) {
            ui->getUiMessageBox()->show("NETWORK ERROR", error.empty() ? "DOWNLOAD FAILED" : error, "OK");
            return true;
        }
    }
#endif

    // the list as it is now: reused when coming back (no network scan)
    ui->getUiRomList()->getRomList()->saveListCache();

    // the other program uses the same display and controller settings
    const std::string settings = ui->getIo()->getDataPath() + SHARED_SETTINGS_FILE;
    writeSharedSettings(ui->getConfig(), settings);

    s_pendingProgram = program;
    s_pendingArgs = {
            ARG_ROM + fullPath,
            ARG_RETURN + selfProgramPath(),
            ARG_SELECT + game.romsPath + game.path,
            ARG_SETTINGS + settings,
            ARG_SYSTEM + ui->getConfig()->get(PEMUConfig::OptId::UI_FILTER_SYSTEM)->getString(),
    };
    if (!state.empty()) s_pendingArgs.push_back(ARG_STATE + state);
    if (demo::isActive()) {
        s_pendingArgs.push_back(ARG_DEMO + std::to_string(demo::getSeconds()) + "," + (demo::hasSound() ? "1" : "0"));
    }
#ifndef __PS4__
    s_pendingArgs.push_back(ARG_CWD + ui->getIo()->getDataPath());
#endif
    logLine("launcher: %s game, closing the launcher for %s", game.system.name.c_str(), program.c_str());

    // the settings (last game, filters...) are saved and the ui closed as usual,
    // then main() starts the other program (see runPendingLaunch)
    ui->getConfig()->save();
    ui->done = true;
    return true;
}

bool pemu::runPendingLaunch() {
    {
        if (s_pendingProgram.empty()) return false;
#ifndef __PS4__
        // desktop builds: the other program uses its own folder as data folder
        const std::string dir = s_pendingProgram.substr(0, s_pendingProgram.find_last_of('/') + 1);
        if (!dir.empty() && chdir(dir.c_str()) != 0) {
            logLine("launcher: chdir %s failed", dir.c_str());
        }
#endif
        std::string error;
        execProgram(s_pendingProgram, s_pendingArgs, &error);
        return false;
    }
}

void pemu::returnToLauncher(const LaunchArgs &args) {
    if (args.returnTo.empty()) return;
    std::string program = args.returnTo;
#ifndef __PS4__
    const size_t nl = program.find('\n');
    if (nl != std::string::npos) {
        const std::string cwd = program.substr(nl + 1);
        program = program.substr(0, nl);
        if (!cwd.empty() && chdir(cwd.c_str()) != 0) {
            logLine("launcher: chdir %s failed", cwd.c_str());
        }
    }
#endif
    std::vector<std::string> a;
    if (!args.select.empty()) a.push_back(ARG_SELECT + args.select);
    if (!args.settings.empty()) a.push_back(ARG_SETTINGS + args.settings);
    if (!args.system.empty()) a.push_back(ARG_SYSTEM + args.system);
    // demo mode: the time of this game is up, the launcher starts the next one
    if (demo::isActive() && demo::isNextWanted()) a.push_back(ARG_DEMO_NEXT);
    std::string error;
    execProgram(program, a, &error);
}

// options shared by the programs: display options common to all the emulators, and the controls
static bool isSharedOption(const std::string &group, const std::string &name) {
    if (group == "EMULATION") {
        return name == "SCALING" || name == "SCALING_MODE" || name == "FILTER" ||
               name == "EFFECT" || name == "SHOW_FPS";
    }
    return group == "GAMEPAD" || group == "KEYBOARD";
}

bool pemu::writeSharedSettings(config::Config *cfg, const std::string &path) {
    FILE *f = fopen(path.c_str(), "w");
    if (!f) {
        logLine("settings: could not write %s", path.c_str());
        return false;
    }
    int count = 0;
    for (auto &group: *cfg->getGroups()) {
        for (auto &opt: *group.getOptions()) {
            if (!isSharedOption(group.getName(), opt.getName())) continue;
            if (opt.getType() == config::Option::Type::Integer) {
                fprintf(f, "%s/%s=%i\n", group.getName().c_str(), opt.getName().c_str(), opt.getInteger());
            } else if (opt.getType() == config::Option::Type::Array ||
                       opt.getType() == config::Option::Type::String) {
                fprintf(f, "%s/%s=%s\n", group.getName().c_str(), opt.getName().c_str(), opt.getString().c_str());
            } else {
                continue;
            }
            count++;
        }
    }
    fclose(f);
    logLine("settings: %i shared settings written", count);
    return true;
}

bool pemu::applySharedSettings(config::Config *cfg, const std::string &path) {
    FILE *f = fopen(path.c_str(), "r");
    if (!f) return false;
    int count = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        std::string l = line;
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        const size_t slash = l.find('/');
        const size_t eq = l.find('=');
        if (slash == std::string::npos || eq == std::string::npos || eq < slash) continue;
        const std::string groupName = l.substr(0, slash);
        const std::string name = l.substr(slash + 1, eq - slash - 1);
        const std::string value = l.substr(eq + 1);
        if (!isSharedOption(groupName, name)) continue;
        for (auto &group: *cfg->getGroups()) {
            if (group.getName() != groupName) continue;
            for (auto &opt: *group.getOptions()) {
                if (opt.getName() != name) continue;
                if (opt.getType() == config::Option::Type::Integer) {
                    opt.setInteger(atoi(value.c_str()));
                    count++;
                } else if (opt.getType() == config::Option::Type::Array) {
                    // only a value this program knows (a shader both programs have...)
                    const auto values = opt.getArray();
                    if (std::find(values.begin(), values.end(), value) != values.end()) {
                        opt.setArrayIndex(value);
                        count++;
                    }
                } else if (opt.getType() == config::Option::Type::String) {
                    opt.setString(value);
                    count++;
                }
            }
        }
    }
    fclose(f);
    cfg->save();
    logLine("settings: %i shared settings applied", count);
    return true;
}
