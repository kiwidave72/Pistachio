#pragma once

// Font Awesome 5/6 Free — Regular style (fa-regular-400.ttf) codepoints,
// UTF-8 encoded so they can be dropped straight into ImGui label strings.
//
// Only icons that actually ship in the free *Regular* weight are listed here.
// (fa-save / the floppy-disk glyph is Solid-only in the Free tier, so it's
// deliberately not included — use folder-open / file instead.)

#define ICON_FA_FOLDER_OPEN "\xef\x81\xbc"  // U+F07C — used for "Load"
#define ICON_FA_FILE        "\xef\x85\x9b"  // U+F15B — used for "Save" (blank doc)
#define ICON_FA_FLOPPY_DISK_REG "\xef\x83\x87" // U+F0C7 — NOT in Regular; kept as a reminder, don't use
#define ICON_FA_UNDO         "\xef\x83\xa2"  // U+F0E2 — used for "Undo"
#define ICON_FA_REDO         "\xef\x80\x9e"  // U+F01E — used for "Redo"

#define ICON_FA_CUBES         "\xef\x86\xb3"
#define ICON_FA_TH            "\xef\x80\x8a"
// Font Awesome's private-use-area icon range. Needed so ImGui's font atlas
// reserves glyph slots for these codepoints when merging the icon font.
#define ICON_MIN_FA 0xf000
#define ICON_MAX_FA 0xf8ff
