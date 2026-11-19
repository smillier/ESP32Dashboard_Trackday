#include "settings.h"

#include "nvs.h"

namespace settings {
namespace {

constexpr char kNamespace[] = "dash";
Data g_data = {LANG_EN, UNITS_KMH, 100, false};

void save_u8(const char* key, uint8_t v) {
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    nvs_set_u8(h, key, v);
    nvs_commit(h);
    nvs_close(h);
}

} // namespace

void load(bool demo_default) {
    g_data = {LANG_EN, UNITS_KMH, 100, demo_default};
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    uint8_t v;
    if (nvs_get_u8(h, "lang", &v) == ESP_OK && v < LANG_COUNT) g_data.lang = v;
    if (nvs_get_u8(h, "units", &v) == ESP_OK && v < UNITS_COUNT) g_data.units = v;
    if (nvs_get_u8(h, "bright", &v) == ESP_OK && v >= 10 && v <= 100) g_data.brightness = v;
    if (nvs_get_u8(h, "demo", &v) == ESP_OK) g_data.demo = v != 0;
    nvs_close(h);
}

const Data& get() {
    return g_data;
}

void set_lang(uint8_t lang) {
    if (lang >= LANG_COUNT) return;
    g_data.lang = lang;
    save_u8("lang", lang);
}

void set_units(uint8_t units) {
    if (units >= UNITS_COUNT) return;
    g_data.units = units;
    save_u8("units", units);
}

void set_brightness(uint8_t pct) {
    if (pct < 10) pct = 10;
    if (pct > 100) pct = 100;
    g_data.brightness = pct;
    save_u8("bright", pct);
}

void set_demo(bool on) {
    g_data.demo = on;
    save_u8("demo", on ? 1 : 0);
}

} // namespace settings
