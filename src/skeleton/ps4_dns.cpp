//
// Host name lookup for the PS4 (see ps4_dns.h)
//

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstdint>
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

// Tiny DNS client (one "A" query over udp to public DNS servers).
// It only uses socket/fcntl/poll/sendto/recvfrom, the same calls libsmb2 uses (known to work
// on the ps4). The ps4 system resolver (sceNetResolver*) crashed the app.

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define DNS_PORT 53
#define DNS_TIMEOUT_MS 2500
#define DNS_TRIES 2

static const char *dnsServers[] = {"1.1.1.1", "8.8.8.8", "9.9.9.9"};

static SDL_mutex *dnsMutex() {
    static SDL_mutex *m = SDL_CreateMutex(); // thread safe static init
    return m;
}

static std::map<std::string, std::string> s_cache;

// build a query, returns its size (0 on error)
static size_t dnsQuery(const std::string &host, uint16_t id, unsigned char *buf, size_t size) {
    if (host.size() > 250 || size < host.size() + 18) return 0;
    size_t n = 0;
    buf[n++] = id >> 8;
    buf[n++] = id & 0xff;
    buf[n++] = 0x01; // recursion desired
    buf[n++] = 0x00;
    buf[n++] = 0x00;
    buf[n++] = 0x01; // 1 question
    for (int i = 0; i < 6; i++) buf[n++] = 0; // no answer, authority, additional
    size_t start = 0;
    while (start < host.size()) {
        size_t dot = host.find('.', start);
        if (dot == std::string::npos) dot = host.size();
        const size_t len = dot - start;
        if (len == 0 || len > 63) return 0;
        buf[n++] = (unsigned char) len;
        memcpy(buf + n, host.data() + start, len);
        n += len;
        start = dot + 1;
    }
    buf[n++] = 0;
    buf[n++] = 0x00;
    buf[n++] = 0x01; // type A
    buf[n++] = 0x00;
    buf[n++] = 0x01; // class IN
    return n;
}

// skip a (possibly compressed) name, returns the new offset or 0 on error
static size_t dnsSkipName(const unsigned char *buf, size_t len, size_t pos) {
    while (pos < len) {
        const unsigned char c = buf[pos];
        if (c == 0) return pos + 1;
        if ((c & 0xc0) == 0xc0) return pos + 2 <= len ? pos + 2 : 0;
        pos += c + 1;
    }
    return 0;
}

// first "A" record of an answer, 0 if none
static bool dnsParse(const unsigned char *buf, size_t len, uint16_t id, uint32_t *addr, int *rcode) {
    if (len < 12) return false;
    if (((buf[0] << 8) | buf[1]) != id) return false;
    *rcode = buf[3] & 0x0f;
    const int qd = (buf[4] << 8) | buf[5];
    const int an = (buf[6] << 8) | buf[7];
    size_t pos = 12;
    for (int i = 0; i < qd; i++) {
        pos = dnsSkipName(buf, len, pos);
        if (!pos || pos + 4 > len) return false;
        pos += 4;
    }
    for (int i = 0; i < an; i++) {
        pos = dnsSkipName(buf, len, pos);
        if (!pos || pos + 10 > len) return false;
        const int type = (buf[pos] << 8) | buf[pos + 1];
        const int cls = (buf[pos + 2] << 8) | buf[pos + 3];
        const int rdlen = (buf[pos + 8] << 8) | buf[pos + 9];
        pos += 10;
        if (pos + rdlen > len) return false;
        if (type == 1 && cls == 1 && rdlen == 4) { // CNAME records are skipped
            memcpy(addr, buf + pos, 4);
            return true;
        }
        pos += rdlen;
    }
    return false;
}

static bool dnsAsk(const char *server, const std::string &host, std::string *ip, std::string *error) {
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        *error = "dns socket error " + std::to_string(errno);
        return false;
    }
    const int fl = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);

    struct sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(DNS_PORT);
    unsigned a = 0, b = 0, c = 0, d = 0;
    sscanf(server, "%u.%u.%u.%u", &a, &b, &c, &d);
    const unsigned char bytes[4] = {(unsigned char) a, (unsigned char) b, (unsigned char) c, (unsigned char) d};
    memcpy(&sa.sin_addr.s_addr, bytes, 4); // network byte order

    static uint16_t counter = 0x5150;
    const uint16_t id = (uint16_t) (counter++ ^ (uintptr_t) &sa);
    unsigned char query[300], answer[1500];
    const size_t qlen = dnsQuery(host, id, query, sizeof(query));
    if (!qlen) {
        close(fd);
        *error = "bad host name " + host;
        return false;
    }

    bool ok = false;
    for (int t = 0; t < DNS_TRIES && !ok; t++) {
        if (sendto(fd, query, qlen, 0, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
            *error = "dns send error " + std::to_string(errno);
            break;
        }
        struct pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;
        if (poll(&pfd, 1, DNS_TIMEOUT_MS) <= 0) {
            *error = std::string("no answer from dns ") + server;
            continue;
        }
        const ssize_t n = recvfrom(fd, answer, sizeof(answer), 0, nullptr, nullptr);
        if (n <= 0) {
            *error = "dns receive error " + std::to_string(errno);
            continue;
        }
        uint32_t addr = 0;
        int rcode = 0;
        if (dnsParse(answer, (size_t) n, id, &addr, &rcode)) {
            const auto *b = (const unsigned char *) &addr;
            char buf[16];
            snprintf(buf, sizeof(buf), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
            *ip = buf;
            ok = true;
        } else {
            *error = rcode == 3 ? "server " + host + " not found" : "bad dns answer (" + std::to_string(rcode) + ")";
            if (rcode == 3) break;
        }
    }
    close(fd);
    return ok;
}

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

    std::string result, err;
    bool ok = false;
    for (const char *server: dnsServers) {
        logLine("dns: asking %s for %s", server, host.c_str());
        if (dnsAsk(server, host, &result, &err)) {
            ok = true;
            break;
        }
        logLine("dns: %s", err.c_str());
        if (err.find("not found") != std::string::npos) break;
    }
    if (ok) {
        s_cache[host] = result;
        logLine("dns: %s = %s", host.c_str(), result.c_str());
        if (ip) *ip = result;
    } else if (error) {
        *error = err;
    }
    SDL_UnlockMutex(mutex);
    return ok;
}
