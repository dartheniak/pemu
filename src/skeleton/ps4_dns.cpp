//
// Host name lookup with the PS4's own resolver (see ps4_dns.h)
//
// Note: <orbis/Net.h> is NOT included here, it declares the resolver functions
// without parameters (void f()), the real prototypes are declared below.
//

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <SDL2/SDL_mutex.h>
#include "ps4_dns.h"
#include "pemu_log.h"

bool pemu::urlHost(const std::string &url, std::string *host, int *port) {
    size_t start = url.find("://");
    if (start == std::string::npos) return false;
    const std::string scheme = url.substr(0, start);
    start += 3;
    size_t end = url.find_first_of("/?#", start);
    std::string hostPort = url.substr(start, end == std::string::npos ? std::string::npos : end - start);
    const size_t at = hostPort.rfind('@');
    if (at != std::string::npos) hostPort = hostPort.substr(at + 1);
    int p = scheme == "https" ? 443 : 80;
    const size_t colon = hostPort.rfind(':');
    if (colon != std::string::npos) {
        p = atoi(hostPort.c_str() + colon + 1);
        hostPort = hostPort.substr(0, colon);
    }
    if (hostPort.empty()) return false;
    if (host) *host = hostPort;
    if (port) *port = p;
    return true;
}

static bool isNumericIp(const std::string &host) {
    unsigned a, b, c, d;
    char extra;
    return sscanf(host.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) == 4;
}

#ifdef __PS4__

extern "C" {
int sceNetPoolCreate(const char *name, int size, int flags);
int sceNetPoolDestroy(int memid);
int sceNetResolverCreate(const char *name, int memid, int flags);
int sceNetResolverStartNtoa(int rid, const char *hostname, uint32_t *addr, int timeout, int retry, int flags);
int sceNetResolverDestroy(int rid);
}

#define DNS_POOL_SIZE (4 * 1024)
#define DNS_TIMEOUT (5 * 1000 * 1000) // microseconds
#define DNS_RETRY 2

static SDL_mutex *dnsMutex() {
    static SDL_mutex *m = SDL_CreateMutex(); // thread safe static init
    return m;
}

static int s_pool = -1;
static std::map<std::string, std::string> s_cache;

bool pemu::resolveHost(const std::string &host, std::string *ip, std::string *error) {
    if (isNumericIp(host)) {
        if (ip) *ip = host;
        return true;
    }

    SDL_mutex *mutex = dnsMutex();
    SDL_LockMutex(mutex);

    auto it = s_cache.find(host);
    if (it != s_cache.end()) {
        if (ip) *ip = it->second;
        SDL_UnlockMutex(mutex);
        return true;
    }

    char msg[128] = {0};
    bool ok = false;
    logLine("dns: resolving %s", host.c_str());
    if (s_pool < 0) {
        s_pool = sceNetPoolCreate("pppsnes_dns", DNS_POOL_SIZE, 0);
        logLine("dns: pool = 0x%08x", (unsigned) s_pool);
    }
    if (s_pool < 0) {
        snprintf(msg, sizeof(msg), "network pool error 0x%08x", (unsigned) s_pool);
    } else {
        const int rid = sceNetResolverCreate("pppsnes_resolver", s_pool, 0);
        if (rid < 0) {
            snprintf(msg, sizeof(msg), "resolver error 0x%08x", (unsigned) rid);
        } else {
            uint32_t addr = 0; // network byte order
            const int res = sceNetResolverStartNtoa(rid, host.c_str(), &addr, DNS_TIMEOUT, DNS_RETRY, 0);
            sceNetResolverDestroy(rid);
            if (res < 0 || addr == 0) {
                snprintf(msg, sizeof(msg), "could not find server %s (0x%08x)", host.c_str(), (unsigned) res);
            } else {
                const auto *b = (const unsigned char *) &addr;
                char buf[16];
                snprintf(buf, sizeof(buf), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
                s_cache[host] = buf;
                if (ip) *ip = buf;
                ok = true;
            }
        }
    }

    if (ok) logLine("dns: %s = %s", host.c_str(), s_cache[host].c_str());
    else {
        logLine("dns: %s", msg);
        if (error) *error = msg;
    }
    SDL_UnlockMutex(mutex);
    return ok;
}

#else

bool pemu::resolveHost(const std::string &host, std::string *ip, std::string *error) {
    if (isNumericIp(host)) {
        if (ip) *ip = host;
        return true;
    }
    if (error) *error = "not implemented on this platform";
    return false;
}

#endif
