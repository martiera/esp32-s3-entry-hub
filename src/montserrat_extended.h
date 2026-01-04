#ifndef MONTSERRAT_EXTENDED_H
#define MONTSERRAT_EXTENDED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// Custom Montserrat fonts with Latin Extended character support
// Includes: Basic Latin, Latin-1 Supplement, and Latin Extended-A
// Character ranges: U+0020-U+007F, U+00A0-U+00FF, U+0100-U+017F
// This covers characters like: ā, ē, ī, ō, ū, ć, ś, ż, etc.

extern const lv_font_t montserrat_extended_20;

#ifdef __cplusplus
}
#endif

#endif // MONTSERRAT_EXTENDED_H
