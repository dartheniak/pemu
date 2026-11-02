//
// pppEmu: the network share password is not stored as plain text in config.cfg.
//
// It's stored encrypted ("enc1:" + hex(nonce + ChaCha20 ciphertext)) with a random key made on
// first use and kept in its own file ("<data>/.pppemu_key"), never in config.cfg. So config.cfg
// can be copied, backed up or shown to someone without giving away the password.
// It doesn't protect against someone who can read the whole data folder (key file included):
// the ps4 has no secure key storage homebrew can use.
//

#ifndef PEMU_SECRET_H
#define PEMU_SECRET_H

#include <string>

namespace pemu {
    namespace secret {
        // "enc1:..." values
        bool isEncrypted(const std::string &value);

        // plain -> "enc1:...". "dataPath": folder of the key file. Empty in, empty out.
        std::string encrypt(const std::string &dataPath, const std::string &plain);

        // "enc1:..." -> plain (a plain value is returned as is). False if it can't be decrypted
        // (key file missing or different: the password has to be typed again).
        bool decrypt(const std::string &dataPath, const std::string &value, std::string *plain);
    }
}

#endif //PEMU_SECRET_H
