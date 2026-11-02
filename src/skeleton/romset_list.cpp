//
// pppEmu: arcade romset names (see romset_list.h)
//

#include <cstdio>
#include <cstring>
#include <map>
#include "romset_list.h"
#include "pemu_log.h"

using namespace pemu;

RomsetList *RomsetList::get(const std::string &path) {
    static std::map<std::string, RomsetList *> lists;
    auto it = lists.find(path);
    if (it != lists.end()) return it->second;

    RomsetList *list = nullptr;
    if (FILE *f = fopen(path.c_str(), "r")) {
        list = new RomsetList();
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            std::vector<std::string> fields;
            std::string cur;
            for (char *c = line; *c && *c != '\n' && *c != '\r'; c++) {
                if (*c == '\t') {
                    fields.push_back(cur);
                    cur.clear();
                } else {
                    cur += *c;
                }
            }
            fields.push_back(cur);
            if (fields.size() < 6 || fields[0].empty()) continue;
            Set set;
            set.fullName = fields[1];
            set.parent = fields[2];
            set.year = fields[3];
            set.company = fields[4];
            set.bios = fields[5] == "1";
            list->m_sets.emplace(fields[0], set);   // first one wins ("neogeo" is listed twice)
        }
        fclose(f);
        logLine("RomsetList: %zu sets in %s", list->m_sets.size(), path.c_str());
    } else {
        logLine("RomsetList: could not open %s", path.c_str());
    }
    lists[path] = list;
    return list;
}

std::string RomsetList::key(const std::string &fileName) {
    std::string name = fileName.substr(fileName.find_last_of('/') + 1);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    for (auto &c: name) c = (char) tolower((unsigned char) c);
    return name;
}

const RomsetList::Set *RomsetList::find(const std::string &fileName) const {
    auto it = m_sets.find(key(fileName));
    return it == m_sets.end() ? nullptr : &it->second;
}

std::vector<std::string> RomsetList::parents(const std::string &fileName) const {
    std::vector<std::string> out;
    std::string name = key(fileName);
    for (int depth = 0; depth < 4; depth++) {
        auto it = m_sets.find(name);
        if (it == m_sets.end() || it->second.parent.empty()) break;
        name = it->second.parent;
        out.push_back(name);
    }
    return out;
}
