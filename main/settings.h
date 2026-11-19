#pragma once

#include <cstdint>

// User settings kept in flash (NVS namespace "dash"). Changed from the
// configuration screen.
namespace settings {

enum Lang : uint8_t { LANG_EN, LANG_FR, LANG_DE, LANG_ES, LANG_IT, LANG_COUNT };
enum Units : uint8_t { UNITS_KMH, UNITS_MPH, UNITS_COUNT };

struct Data {
    uint8_t lang;        // Lang
    uint8_t units;       // Units, for the speed
    uint8_t brightness;  // backlight, 10..100 %
    bool demo;           // simulated telemetry instead of the RaceBox
};

// Reads the saved values; `demo_default` is used until the user changes it.
void load(bool demo_default);
const Data& get();

void set_lang(uint8_t lang);
void set_units(uint8_t units);
void set_brightness(uint8_t pct);
void set_demo(bool on);

} // namespace settings
