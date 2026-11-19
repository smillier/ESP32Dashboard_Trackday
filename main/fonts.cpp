#include "fonts.h"

#include "esp_log.h"

extern "C" {
extern const uint8_t ttf_start[] asm("_binary_Montserrat_Medium_ttf_start");
extern const uint8_t ttf_end[] asm("_binary_Montserrat_Medium_ttf_end");
}

namespace fonts {
namespace {

lv_font_t *g_small, *g_text, *g_title;

lv_font_t* make(int px, const lv_font_t* builtin) {
    lv_font_t* f = lv_tiny_ttf_create_data(ttf_start, (size_t)(ttf_end - ttf_start), px);
    if (f == nullptr) {
        ESP_LOGE("fonts", "Tiny TTF %d px failed, using the built-in font", px);
        return const_cast<lv_font_t*>(builtin);
    }
    f->fallback = builtin;
    return f;
}

} // namespace

void init() {
    g_small = make(12, &lv_font_montserrat_12);
    g_text = make(14, &lv_font_montserrat_14);
    g_title = make(20, &lv_font_montserrat_20);
}

const lv_font_t* small() { return g_small; }
const lv_font_t* text() { return g_text; }
const lv_font_t* title() { return g_title; }

} // namespace fonts
