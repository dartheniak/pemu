//
// Text entry for pEMU menus.
//
// PS4: opens the system on-screen keyboard (ImeDialog) while the app keeps rendering.
// Other platforms: development fallback reading a line from the terminal (stdin).
//

#ifndef PEMU_TEXT_INPUT_H
#define PEMU_TEXT_INPUT_H

#include <string>

namespace pemu {
    class UiMain;

    class TextInput {
    public:
        enum class Kind {
            Text,       // any text (folder names with accents...)
            Latin,      // basic latin only (addresses, user names)
            Password    // basic latin, hidden while typing
        };

        // returns true if the user confirmed, "out" then holds the new text
        static bool get(UiMain *ui, const std::string &title, const std::string &initial,
                        Kind kind, std::string *out);
    };
}

#endif //PEMU_TEXT_INPUT_H
