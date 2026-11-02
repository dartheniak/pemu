//
// pppEmu: arcade romset names ("sf2.zip" -> "Street Fighter II: The World Warrior (World 910522)").
// Made at build time from FBNeo's game list (src/cores/psnes/tools/fbneo_arcade_list.pl):
// one set per line, tab separated: name, full name, parent, year, company, bios (0/1).
//

#ifndef PEMU_ROMSET_LIST_H
#define PEMU_ROMSET_LIST_H

#include <string>
#include <unordered_map>
#include <vector>

namespace pemu {
    class RomsetList {
    public:
        struct Set {
            std::string fullName, parent, year, company;
            bool bios = false;
        };

        // loaded once per path, null if the file can't be read
        static RomsetList *get(const std::string &path);

        // "sf2.zip", "SF2", "folder/sf2.zip" -> set, null if unknown
        const Set *find(const std::string &fileName) const;

        // files a game needs besides its own zip: its parents ("sf2ce" -> "sf2"),
        // not including bios files (see biosNames)
        std::vector<std::string> parents(const std::string &fileName) const;

        size_t size() const { return m_sets.size(); }

    private:
        static std::string key(const std::string &fileName);

        std::unordered_map<std::string, Set> m_sets;
    };
}

#endif //PEMU_ROMSET_LIST_H
