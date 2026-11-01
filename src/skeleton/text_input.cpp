//
// Text entry for pEMU menus (see text_input.h)
//

#include <cstdio>
#include <cstring>
#include <cstdint>
#include "pemu.h"
#include "text_input.h"

#ifdef __PS4__
#include <orbis/Sysmodule.h>
#include <orbis/CommonDialog.h>
#include <orbis/ImeDialog.h>

// ime option bits (SCE_IME_OPTION_*)
#define PEMU_IME_OPTION_NO_AUTO_CAPITALIZATION 0x00000002
#define PEMU_IME_OPTION_PASSWORD 0x00000004
#define PEMU_IME_MAX_TEXT 255
#define PEMU_IME_USER_ID 254 // "everyone": the user holding the controller

// the ps4 ime works with utf-16 (wchar_t is 32 bits with clang on this target)
static size_t utf8ToUtf16(const std::string &in, uint16_t *out, size_t max) {
    size_t n = 0;
    for (size_t i = 0; i < in.size() && n < max;) {
        uint32_t c = (unsigned char) in[i];
        int len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
        if (len == 2) c &= 0x1F; else if (len == 3) c &= 0x0F; else if (len == 4) c &= 0x07;
        for (int k = 1; k < len && i + k < in.size(); k++) c = (c << 6) | ((unsigned char) in[i + k] & 0x3F);
        i += len;
        if (c >= 0x10000) {
            if (n + 2 > max) break;
            c -= 0x10000;
            out[n++] = (uint16_t) (0xD800 | (c >> 10));
            out[n++] = (uint16_t) (0xDC00 | (c & 0x3FF));
        } else {
            out[n++] = (uint16_t) c;
        }
    }
    out[n] = 0;
    return n;
}

static std::string utf16ToUtf8(const uint16_t *in) {
    std::string out;
    for (size_t i = 0; in[i]; i++) {
        uint32_t c = in[i];
        if (c >= 0xD800 && c <= 0xDBFF && in[i + 1] >= 0xDC00 && in[i + 1] <= 0xDFFF) {
            c = 0x10000 + ((c - 0xD800) << 10) + (in[++i] - 0xDC00);
        }
        if (c < 0x80) {
            out += (char) c;
        } else if (c < 0x800) {
            out += (char) (0xC0 | (c >> 6));
            out += (char) (0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += (char) (0xE0 | (c >> 12));
            out += (char) (0x80 | ((c >> 6) & 0x3F));
            out += (char) (0x80 | (c & 0x3F));
        } else {
            out += (char) (0xF0 | (c >> 18));
            out += (char) (0x80 | ((c >> 12) & 0x3F));
            out += (char) (0x80 | ((c >> 6) & 0x3F));
            out += (char) (0x80 | (c & 0x3F));
        }
    }
    return out;
}

bool TextInput::get(UiMain *ui, const std::string &title, const std::string &initial,
                    Kind kind, std::string *out) {
    static bool loaded = false;
    if (!loaded) {
        int ret = sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG);
        printf("TextInput: load ime dialog module: 0x%08x\n", ret);
        ret = sceCommonDialogInitialize();
        printf("TextInput: common dialog init: 0x%08x\n", ret); // "already initialized" is fine
        loaded = true;
    }

    static uint16_t textBuffer[PEMU_IME_MAX_TEXT + 1];
    static uint16_t titleBuffer[128];
    memset(textBuffer, 0, sizeof(textBuffer));
    utf8ToUtf16(initial, textBuffer, PEMU_IME_MAX_TEXT);
    utf8ToUtf16(title, titleBuffer, 127);

    OrbisImeDialogSetting param;
    memset(&param, 0, sizeof(param));
    param.userId = PEMU_IME_USER_ID;
    param.type = kind == Kind::Text ? ORBIS_TYPE_DEFAULT : ORBIS_TYPE_BASIC_LATIN;
    param.enterLabel = ORBIS_BUTTON_LABEL_DEFAULT;
    param.inputMethod = ORBIS__DEFAULT;
    param.option = PEMU_IME_OPTION_NO_AUTO_CAPITALIZATION;
    if (kind == Kind::Password) param.option |= PEMU_IME_OPTION_PASSWORD;
    param.maxTextLength = PEMU_IME_MAX_TEXT;
    param.inputTextBuffer = (wchar_t *) textBuffer;
    param.title = (const wchar_t *) titleBuffer;
    param.posx = 1920.0f / 2;
    param.posy = 1080.0f / 2;
    param.horizontalAlignment = ORBIS_H_CENTER;
    param.verticalAlignment = ORBIS_V_CENTER;

    int ret = sceImeDialogInit(&param, nullptr);
    if (ret < 0) {
        // retry without option bits in case this firmware rejects them
        printf("TextInput: sceImeDialogInit failed (0x%08x), retrying without options\n", ret);
        param.option = 0;
        ret = sceImeDialogInit(&param, nullptr);
    }
    if (ret < 0) {
        printf("TextInput: sceImeDialogInit failed (0x%08x)\n", ret);
        ui->getUiMessageBox()->show("KEYBOARD", "COULD NOT OPEN THE PS4 KEYBOARD", "OK");
        return false;
    }

    // keep rendering (without handling our own inputs) while the keyboard is shown
    int frames = 0;
    while (true) {
        OrbisDialogStatus status = sceImeDialogGetStatus();
        if (status == ORBIS_DIALOG_STATUS_STOPPED) break;
        if (status == ORBIS_DIALOG_STATUS_NONE && ++frames > 30) break;
        ui->flip(true, false);
    }

    OrbisDialogResult result;
    memset(&result, 0, sizeof(result));
    sceImeDialogGetResult(&result);
    sceImeDialogTerm();
    ui->getInput()->clear();

    if (result.endstatus != ORBIS_DIALOG_OK) return false;
    if (out) *out = utf16ToUtf8(textBuffer);
    return true;
}

#else

// development fallback: type the value in the terminal the app was started from
bool TextInput::get(UiMain *ui, const std::string &title, const std::string &initial,
                    Kind kind, std::string *out) {
    ui->getUiStatusBox()->show("TYPE THE VALUE IN THE TERMINAL");
    ui->flip(true, false);
    printf("\n[TextInput] %s (current: \"%s\", empty line = cancel): ",
           title.c_str(), kind == Kind::Password ? "***" : initial.c_str());
    fflush(stdout);
    char line[512];
    if (!fgets(line, sizeof(line), stdin)) return false;
    std::string value = line;
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) value.pop_back();
    if (value.empty()) return false;
    if (value == "-") value.clear(); // "-" clears the field
    if (out) *out = value;
    ui->getInput()->clear();
    return true;
}

#endif
