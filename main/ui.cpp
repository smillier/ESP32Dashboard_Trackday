#include "ui.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "lvgl.h"

#include "app_state.h"
#include "heart_rate_ble.h"
#include "racebox_ble.h"
#include "status_led.h"
#include "telemetry.h"

namespace {

#define COL_BG        lv_color_hex(0xE8EAEE)
#define COL_PANEL     lv_color_hex(0xF8F9FA)
#define COL_TEXT      lv_color_hex(0x101010)
#define COL_CAPTION   lv_color_hex(0x505560)
#define COL_RED       lv_color_hex(0xE02020)
#define COL_GREEN     lv_color_hex(0x1FAA10)
#define COL_BLUE      lv_color_hex(0x1E78E0)
#define COL_ORANGE    lv_color_hex(0xF07818)
#define COL_PURPLE    lv_color_hex(0x9020D0)
#define COL_GREY      lv_color_hex(0x9AA0AA)

constexpr int kArcSize = 226;
constexpr int kArcX = 6, kArcY = 36;
constexpr int kLeanRange = 60;

struct Chip {
    lv_obj_t* dot;
    lv_obj_t* name;
    lv_obj_t* state;
};

struct Card {
    lv_obj_t* box;
    lv_obj_t* lap;
    lv_obj_t* time;
    lv_obj_t* delta;
};

Chip g_chip_rb, g_chip_gopro, g_chip_hr;
Card g_cards[3];
lv_obj_t *g_arc, *g_lean, *g_max_l, *g_max_r, *g_info;
lv_obj_t *g_lap_cap, *g_current, *g_best, *g_gap, *g_hint;
uint32_t g_last_lap_count = 0;

lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color, int x, int y) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, "");
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

// Avoids invalidating (and redrawing) a label whose text did not change.
void set_text(lv_obj_t* l, const char* text) {
    if (std::strcmp(lv_label_get_text(l), text) != 0) {
        lv_label_set_text(l, text);
    }
}

void set_color(lv_obj_t* l, lv_color_t c) {
    lv_obj_set_style_text_color(l, c, 0);
}

void fmt_time(char* buf, size_t n, uint32_t ms) {
    if (ms == 0) {
        std::snprintf(buf, n, "-:--.---");
        return;
    }
    std::snprintf(buf, n, "%u:%02u.%03u", (unsigned)(ms / 60000), (unsigned)(ms / 1000 % 60), (unsigned)(ms % 1000));
}

void fmt_delta(char* buf, size_t n, int32_t ms) {
    const int32_t a = std::abs(ms);
    std::snprintf(buf, n, "%c%d.%03d", ms < 0 ? '-' : '+', (int)(a / 1000), (int)(a % 1000));
}

void build_chip(lv_obj_t* scr, Chip* c, int x, const char* name) {
    lv_obj_t* box = lv_obj_create(scr);
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, x, 3);
    lv_obj_set_size(box, 154, 28);
    lv_obj_set_style_bg_color(box, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, 6, 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_border_color(box, COL_TEXT, 0);

    c->dot = lv_obj_create(box);
    lv_obj_remove_style_all(c->dot);
    lv_obj_set_pos(c->dot, 6, 6);
    lv_obj_set_size(c->dot, 12, 12);
    lv_obj_set_style_radius(c->dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(c->dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(c->dot, COL_GREY, 0);

    c->name = make_label(box, &lv_font_montserrat_14, COL_TEXT, 24, 3);
    lv_label_set_text(c->name, name);
    c->state = make_label(box, &lv_font_montserrat_14, COL_CAPTION, 0, 3);
    lv_obj_set_width(c->state, 70);
    lv_obj_set_style_text_align(c->state, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_x(c->state, 70);
}

void chip_set(Chip* c, lv_color_t dot, const char* state, bool blink_on) {
    lv_obj_set_style_bg_color(c->dot, blink_on ? dot : COL_GREY, 0);
    set_text(c->state, state);
}

void build_card(lv_obj_t* scr, Card* c, int x) {
    c->box = lv_obj_create(scr);
    lv_obj_remove_style_all(c->box);
    lv_obj_set_pos(c->box, x, 238);
    lv_obj_set_size(c->box, 150, 76);
    lv_obj_set_style_bg_color(c->box, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(c->box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c->box, 6, 0);
    lv_obj_set_style_border_width(c->box, 2, 0);
    lv_obj_set_style_border_color(c->box, COL_TEXT, 0);
    c->lap = make_label(c->box, &lv_font_montserrat_14, COL_CAPTION, 8, 4);
    c->delta = make_label(c->box, &lv_font_montserrat_14, COL_TEXT, 0, 4);
    lv_obj_set_width(c->delta, 80);
    lv_obj_set_style_text_align(c->delta, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_x(c->delta, 56);
    c->time = make_label(c->box, &lv_font_montserrat_28, COL_TEXT, 0, 28);
    lv_obj_set_width(c->time, 140);
    lv_obj_set_style_text_align(c->time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_x(c->time, 3);
}

lv_color_t lean_color(float a) {
    a = a < 0 ? -a : a;
    if (a < 30) return COL_GREEN;
    if (a < 45) return COL_ORANGE;
    return COL_RED;
}

void build_lean(lv_obj_t* scr) {
    g_arc = lv_arc_create(scr);
    lv_obj_set_size(g_arc, kArcSize, kArcSize);
    lv_obj_set_pos(g_arc, kArcX, kArcY);
    lv_arc_set_rotation(g_arc, 0);
    lv_arc_set_bg_angles(g_arc, 270 - kLeanRange * 3 / 2, 270 + kLeanRange * 3 / 2);
    lv_arc_set_range(g_arc, -kLeanRange, kLeanRange);
    lv_arc_set_mode(g_arc, LV_ARC_MODE_SYMMETRICAL);
    lv_arc_set_value(g_arc, 0);
    lv_obj_remove_style(g_arc, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(g_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(g_arc, 24, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g_arc, 24, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g_arc, lv_color_hex(0xC4C8D0), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(g_arc, false, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(g_arc, false, LV_PART_INDICATOR);

    lv_obj_t* l = make_label(scr, &lv_font_montserrat_20, COL_TEXT, 8, 118);
    lv_label_set_text(l, "L");
    l = make_label(scr, &lv_font_montserrat_20, COL_TEXT, 216, 118);
    lv_label_set_text(l, "R");

    lv_obj_t* tick = lv_obj_create(scr);
    lv_obj_remove_style_all(tick);
    lv_obj_set_pos(tick, kArcX + kArcSize / 2 - 1, kArcY - 6);
    lv_obj_set_size(tick, 3, 8);
    lv_obj_set_style_bg_color(tick, COL_TEXT, 0);
    lv_obj_set_style_bg_opa(tick, LV_OPA_COVER, 0);

    g_lean = make_label(scr, &lv_font_montserrat_48, COL_TEXT, 0, 82);
    lv_obj_set_width(g_lean, 150);
    lv_obj_set_style_text_align(g_lean, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_x(g_lean, kArcX + kArcSize / 2 - 75);
    l = make_label(scr, &lv_font_montserrat_14, COL_CAPTION, 0, 138);
    lv_label_set_text(l, "LEAN ANGLE");
    lv_obj_set_width(l, 150);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_x(l, kArcX + kArcSize / 2 - 75);

    g_max_l = make_label(scr, &lv_font_montserrat_20, COL_TEXT, 10, 166);
    g_max_r = make_label(scr, &lv_font_montserrat_20, COL_TEXT, 128, 166);
    g_info = make_label(scr, &lv_font_montserrat_14, COL_CAPTION, 10, 198);
}

void build_laps(lv_obj_t* scr) {
    constexpr int x = 246;
    g_lap_cap = make_label(scr, &lv_font_montserrat_14, COL_CAPTION, x, 38);
    g_current = make_label(scr, &lv_font_montserrat_48, COL_TEXT, x, 56);

    lv_obj_t* l = make_label(scr, &lv_font_montserrat_14, COL_CAPTION, x, 122);
    lv_label_set_text(l, "BEST");
    g_best = make_label(scr, &lv_font_montserrat_28, COL_PURPLE, x + 62, 112);

    l = make_label(scr, &lv_font_montserrat_14, COL_CAPTION, x, 162);
    lv_label_set_text(l, "GAP");
    g_gap = make_label(scr, &lv_font_montserrat_28, COL_TEXT, x + 62, 152);

    g_hint = make_label(scr, &lv_font_montserrat_14, COL_RED, x, 198);

    l = make_label(scr, &lv_font_montserrat_14, COL_CAPTION, 8, 218);
    lv_label_set_text(l, "LAST 3 LAPS");
    for (int i = 0; i < 3; ++i) {
        build_card(scr, &g_cards[i], 6 + i * 158);
    }
}

bool blink(uint32_t period_ms) {
    return (lv_tick_get() / period_ms) % 2 == 0;
}

void refresh_chips() {
    const bool slow = blink(500), fast = blink(150);

    switch (racebox_ble::state()) {
        case RaceBoxState::SCANNING: chip_set(&g_chip_rb, COL_BLUE, "SEARCH", slow); break;
        case RaceBoxState::CONNECTING: chip_set(&g_chip_rb, COL_BLUE, "LINK", fast); break;
        case RaceBoxState::CONNECTED_IDLE: chip_set(&g_chip_rb, COL_GREEN, "READY", true); break;
        case RaceBoxState::RECORDING: chip_set(&g_chip_rb, COL_RED, "REC", true); break;
        case RaceBoxState::RECORDING_PAUSED: chip_set(&g_chip_rb, COL_ORANGE, "PAUSED", slow); break;
        case RaceBoxState::MEMORY_LOCKED: chip_set(&g_chip_rb, COL_PURPLE, "LOCKED", fast); break;
    }

    if (app_state_get_recording()) {
        chip_set(&g_chip_gopro, COL_RED, "REC", blink(400));
    } else {
        switch (app_state_get_conn()) {
            case CONN_STATE_SCANNING: chip_set(&g_chip_gopro, COL_BLUE, "SEARCH", blink(400)); break;
            case CONN_STATE_CONNECTING: chip_set(&g_chip_gopro, COL_ORANGE, "PAIRING", true); break;
            case CONN_STATE_READY: chip_set(&g_chip_gopro, COL_GREEN, app_state_mode_str(app_state_get_mode()), true); break;
        }
    }

    uint16_t bpm = 0;
    if (heart_rate_ble_is_connected()) {
        char buf[16];
        if (heart_rate_ble_get_bpm(&bpm)) {
            std::snprintf(buf, sizeof(buf), "%u bpm", bpm);
        } else {
            std::snprintf(buf, sizeof(buf), "-- bpm");
        }
        chip_set(&g_chip_hr, COL_GREEN, buf, true);
    } else {
        chip_set(&g_chip_hr, COL_BLUE, "SEARCH", slow);
    }
}

void refresh_lean(const telemetry::Snapshot& s, bool live) {
    char buf[32];
    const float lean = live ? s.lean_deg : 0;
    const int v = (int)(lean + (lean < 0 ? -0.5f : 0.5f));
    int clamped = v > kLeanRange ? kLeanRange : (v < -kLeanRange ? -kLeanRange : v);
    lv_arc_set_value(g_arc, clamped);
    lv_obj_set_style_arc_color(g_arc, lean_color(lean), LV_PART_INDICATOR);

    if (live) {
        std::snprintf(buf, sizeof(buf), "%d\xC2\xB0", std::abs(v));
    } else {
        std::snprintf(buf, sizeof(buf), "--");
    }
    set_text(g_lean, buf);
    set_color(g_lean, live ? lean_color(lean) : COL_GREY);

    std::snprintf(buf, sizeof(buf), "L %d\xC2\xB0", (int)(s.max_left_deg + 0.5f));
    set_text(g_max_l, buf);
    std::snprintf(buf, sizeof(buf), "R %d\xC2\xB0", (int)(s.max_right_deg + 0.5f));
    set_text(g_max_r, buf);

    if (live) {
        std::snprintf(buf, sizeof(buf), "%d km/h  GPS %s %d  RB %d%%", (int)(s.speed_kmh + 0.5f), s.fix_ok ? "3D" : "--",
                      s.sats, s.battery_pct);
    } else {
        std::snprintf(buf, sizeof(buf), "Waiting for RaceBox...");
    }
    set_text(g_info, buf);
}

void refresh_laps(const telemetry::Snapshot& s, bool live) {
    char buf[32];

    if (s.timing && live) {
        std::snprintf(buf, sizeof(buf), "CURRENT  LAP %d", s.lap_number);
    } else {
        std::snprintf(buf, sizeof(buf), "CURRENT");
    }
    set_text(g_lap_cap, buf);

    fmt_time(buf, sizeof(buf), s.timing && live ? s.current_ms : 0);
    set_text(g_current, buf);

    fmt_time(buf, sizeof(buf), s.best_ms);
    set_text(g_best, buf);

    if (s.has_gap) {
        fmt_delta(buf, sizeof(buf), s.gap_ms);
        set_text(g_gap, buf);
        set_color(g_gap, s.gap_ms <= 0 ? COL_GREEN : COL_RED);
    } else {
        set_text(g_gap, "--.---");
        set_color(g_gap, COL_GREY);
    }

    if (!s.line_set) {
        set_text(g_hint, "No start line: press MARK on the line");
    } else if (!s.timing) {
        set_text(g_hint, "Waiting for the line...");
    } else {
        set_text(g_hint, "");
    }

    for (int i = 0; i < 3; ++i) {
        Card& c = g_cards[i];
        if (s.last_ms[i] == 0) {
            set_text(c.lap, "");
            set_text(c.time, "-:--.---");
            set_text(c.delta, "");
            set_color(c.time, COL_GREY);
            lv_obj_set_style_border_color(c.box, COL_TEXT, 0);
            continue;
        }
        std::snprintf(buf, sizeof(buf), "LAP %d", s.last_lap[i]);
        set_text(c.lap, buf);
        fmt_time(buf, sizeof(buf), s.last_ms[i]);
        set_text(c.time, buf);
        set_color(c.time, COL_TEXT);
        const bool is_best = s.last_lap[i] == s.best_lap;
        lv_obj_set_style_border_color(c.box, is_best ? COL_PURPLE : COL_TEXT, 0);
        if (is_best) {
            set_text(c.delta, "BEST");
            set_color(c.delta, COL_PURPLE);
        } else {
            fmt_delta(buf, sizeof(buf), (int32_t)s.last_ms[i] - (int32_t)s.best_ms);
            set_text(c.delta, buf);
            set_color(c.delta, COL_RED);
        }
    }
}

void refresh(lv_timer_t*) {
    telemetry::Snapshot s;
    telemetry::snapshot(&s);
    const RaceBoxState rb = racebox_ble::state();
    const bool live = racebox_ble::is_connected() || rb == RaceBoxState::RECORDING ||
                      rb == RaceBoxState::CONNECTED_IDLE || rb == RaceBoxState::RECORDING_PAUSED;
    const bool show = live || ui_demo_active;

    refresh_chips();
    refresh_lean(s, show);
    refresh_laps(s, show);

    if (s.lap_count != g_last_lap_count) {
        if (s.lap_count > g_last_lap_count) {
            status_led::flash_lap(s.last_event);
        }
        g_last_lap_count = s.lap_count;
    }
}

} // namespace

bool ui_demo_active = false;

void ui_create(void) {
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    build_chip(scr, &g_chip_rb, 3, "RaceBox");
    build_chip(scr, &g_chip_gopro, 163, "GoPro");
    build_chip(scr, &g_chip_hr, 323, "Heart");
    build_lean(scr);
    build_laps(scr);

    lv_timer_create(refresh, 40, nullptr);
    refresh(nullptr);
}
