#pragma once

#include "lvgl.h"

// Text fonts with the Latin-1 accents (é, ü, ñ ...) that the built-in LVGL
// Montserrat fonts lack. Rendered at runtime from an embedded Montserrat
// TrueType file (Tiny TTF). They fall back to the built-in font of the same
// size for the LV_SYMBOL_* icons. The big digits (28 and 48) stay built-in.
namespace fonts {

void init();                 // call once after lv_init()
const lv_font_t* small();    // 12 px
const lv_font_t* text();     // 14 px
const lv_font_t* title();    // 20 px

} // namespace fonts
