#pragma once
#include <stddef.h>
#include <string.h>

/* One list for Settings, KDE binding, and the real-desktop regression test. */
typedef struct {
    const char *trigger;
    const char *label;
    unsigned keysym;       /* XKB logical symbol; zero selects a physical key. */
    unsigned scan_code;   /* Linux evdev code, also used by the private test. */
    unsigned modifiers;   /* Control=1, Alt=2, Shift=4, Super=8. */
} ShortcutPreset;
static const ShortcutPreset shortcut_presets[] = {
    {"CTRL+ALT+space", "Ctrl+Alt+Space", 0x20, 57, 3},
    {"CTRL+SHIFT+space", "Ctrl+Shift+Space", 0x20, 57, 5},
    {"LOGO+space", "Super+Space", 0x20, 57, 8},
    {"CTRL+ALT+r", "Ctrl+Alt+R", 'r', 19, 3},
    {"Hangul", "한/영 키 (Hangul)", 0xff31, 122, 0},
    {"Alt_R", "오른쪽 Alt", 0, 100, 0},
    {"Control_R", "오른쪽 Ctrl", 0, 97, 0},
};
#define SHORTCUT_PRESET_COUNT (sizeof(shortcut_presets) / sizeof(shortcut_presets[0]))
static inline const ShortcutPreset *shortcut_preset(const char *trigger) {
    if (trigger)
        for (size_t i = 0; i < SHORTCUT_PRESET_COUNT; i++)
            if (!strcmp(trigger, shortcut_presets[i].trigger)) return &shortcut_presets[i];
    return NULL;
}
