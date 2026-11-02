//
// pppEmu: share password stored encrypted (see secret.h)
//

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <vector>
#include "secret.h"
#include "pemu_log.h"

#define SECRET_PREFIX "enc1:"
#define SECRET_KEY_FILE ".pppemu_key"

// ChaCha20 (RFC 8439)
static inline uint32_t rotl(uint32_t v, int c) { return (v << c) | (v >> (32 - c)); }

#define QR(a, b, c, d) \
    a += b; d ^= a; d = rotl(d, 16); \
    c += d; b ^= c; b = rotl(b, 12); \
    a += b; d ^= a; d = rotl(d, 8);  \
    c += d; b ^= c; b = rotl(b, 7);

static void chacha20Block(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter, uint8_t out[64]) {
    uint32_t s[16], x[16];
    s[0] = 0x61707865;
    s[1] = 0x3320646e;
    s[2] = 0x79622d32;
    s[3] = 0x6b206574;
    for (int i = 0; i < 8; i++) {
        s[4 + i] = (uint32_t) key[i * 4] | ((uint32_t) key[i * 4 + 1] << 8) |
                   ((uint32_t) key[i * 4 + 2] << 16) | ((uint32_t) key[i * 4 + 3] << 24);
    }
    s[12] = counter;
    for (int i = 0; i < 3; i++) {
        s[13 + i] = (uint32_t) nonce[i * 4] | ((uint32_t) nonce[i * 4 + 1] << 8) |
                    ((uint32_t) nonce[i * 4 + 2] << 16) | ((uint32_t) nonce[i * 4 + 3] << 24);
    }
    memcpy(x, s, sizeof(s));
    for (int i = 0; i < 10; i++) {
        QR(x[0], x[4], x[8], x[12])
        QR(x[1], x[5], x[9], x[13])
        QR(x[2], x[6], x[10], x[14])
        QR(x[3], x[7], x[11], x[15])
        QR(x[0], x[5], x[10], x[15])
        QR(x[1], x[6], x[11], x[12])
        QR(x[2], x[7], x[8], x[13])
        QR(x[3], x[4], x[9], x[14])
    }
    for (int i = 0; i < 16; i++) {
        const uint32_t v = x[i] + s[i];
        out[i * 4] = (uint8_t) v;
        out[i * 4 + 1] = (uint8_t) (v >> 8);
        out[i * 4 + 2] = (uint8_t) (v >> 16);
        out[i * 4 + 3] = (uint8_t) (v >> 24);
    }
}

static void chacha20(const uint8_t key[32], const uint8_t nonce[12], std::vector<uint8_t> &data) {
    uint8_t block[64];
    for (size_t i = 0; i < data.size(); i++) {
        if (i % 64 == 0) chacha20Block(key, nonce, 1 + (uint32_t) (i / 64), block);
        data[i] ^= block[i % 64];
    }
}

static bool randomBytes(uint8_t *out, size_t len) {
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        const size_t n = fread(out, 1, len, f);
        fclose(f);
        if (n == len) return true;
    }
    // fallback, not great but better than nothing
    uint32_t seed = (uint32_t) time(nullptr) ^ (uint32_t) (uintptr_t) out ^ (uint32_t) clock();
    for (size_t i = 0; i < len; i++) {
        seed = seed * 1103515245u + 12345u;
        out[i] = (uint8_t) (seed >> 16);
    }
    return false;
}

// the key file: made once (32 random bytes)
static bool loadKey(const std::string &dataPath, uint8_t key[32], bool create) {
    const std::string path = dataPath + SECRET_KEY_FILE;
    if (FILE *f = fopen(path.c_str(), "rb")) {
        const size_t n = fread(key, 1, 32, f);
        fclose(f);
        if (n == 32) return true;
    }
    if (!create) return false;
    randomBytes(key, 32);
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) {
        pemu::logLine("secret: could not write %s", path.c_str());
        return false;
    }
    const bool ok = fwrite(key, 1, 32, f) == 32;
    fclose(f);
    pemu::logLine("secret: new key file %s", path.c_str());
    return ok;
}

static std::string toHex(const uint8_t *p, size_t len) {
    static const char hex[] = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < len; i++) {
        out += hex[p[i] >> 4];
        out += hex[p[i] & 15];
    }
    return out;
}

static bool fromHex(const std::string &s, std::vector<uint8_t> *out) {
    if (s.size() % 2) return false;
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    out->clear();
    for (size_t i = 0; i < s.size(); i += 2) {
        const int h = val(s[i]), l = val(s[i + 1]);
        if (h < 0 || l < 0) return false;
        out->push_back((uint8_t) (h * 16 + l));
    }
    return true;
}

bool pemu::secret::isEncrypted(const std::string &value) {
    return value.rfind(SECRET_PREFIX, 0) == 0;
}

std::string pemu::secret::encrypt(const std::string &dataPath, const std::string &plain) {
    if (plain.empty()) return "";
    uint8_t key[32], nonce[12];
    if (!loadKey(dataPath, key, true)) return plain;    // no key: kept as before rather than lost
    randomBytes(nonce, sizeof(nonce));
    std::vector<uint8_t> data(plain.begin(), plain.end());
    chacha20(key, nonce, data);
    return SECRET_PREFIX + toHex(nonce, sizeof(nonce)) + toHex(data.data(), data.size());
}

bool pemu::secret::decrypt(const std::string &dataPath, const std::string &value, std::string *plain) {
    if (!isEncrypted(value)) {
        if (plain) *plain = value;
        return true;
    }
    std::vector<uint8_t> bytes;
    uint8_t key[32];
    if (!fromHex(value.substr(strlen(SECRET_PREFIX)), &bytes) || bytes.size() < 12
        || !loadKey(dataPath, key, false)) {
        logLine("secret: could not decrypt the password (key file missing?)");
        if (plain) plain->clear();
        return false;
    }
    std::vector<uint8_t> data(bytes.begin() + 12, bytes.end());
    chacha20(key, bytes.data(), data);
    if (plain) *plain = std::string(data.begin(), data.end());
    return true;
}
