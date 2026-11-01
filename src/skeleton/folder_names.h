//
// Console folder names matching, shared by network, internal drive and usb roms.
//

#ifndef PEMU_FOLDER_NAMES_H
#define PEMU_FOLDER_NAMES_H

#include <string>
#include <vector>
#include <cctype>

namespace pemu {
    // "Nintendo - Super Nintendo Entertainment System" -> "nintendosupernintendoentertainmentsystem"
    inline std::string folderNameKey(const std::string &name) {
        std::string key;
        for (unsigned char c: name) {
            if (isalnum(c)) key += (char) tolower(c);
        }
        return key;
    }

    // folder names comparison ignoring case, spaces and punctuation
    inline bool sameFolderName(const std::string &a, const std::string &b) {
        const std::string ka = folderNameKey(a);
        return !ka.empty() && ka == folderNameKey(b);
    }

    inline bool matchesFolderName(const std::string &name, const std::vector<std::string> &names) {
        for (const auto &n: names) {
            if (sameFolderName(name, n)) return true;
        }
        return false;
    }
}

#endif //PEMU_FOLDER_NAMES_H
