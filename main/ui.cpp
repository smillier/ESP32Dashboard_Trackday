#include "ui.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "lvgl.h"

#include "app_state.h"
#include "config_ui.h"
#include "fonts.h"
#include "heart_rate_ble.h"
#include "i18n.h"
#include "racebox_ble.h"
#include "settings.h"
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

constexpr int kLeanRange = 60;

// Layout grid: 8 px outer margin, two columns (left x = 8, right x = 248).
constexpr int kMargin = 8;
constexpr int kLeftX = kMargin;
constexpr int kRightX = 248;
constexpr int kIconY = 38;       // icon + caption row
constexpr int kBigY = 58;        // big value row (heart rate / current lap)
constexpr int kRow1Y = 106;      // BEST / SPEED
constexpr int kRow2Y = 142;      // GAP / GPS
constexpr int kValueDx = 72;     // value column offset from the row label
constexpr int kBoxW = 150;       // chips and lap cards
constexpr int kBoxPitch = 157;

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
lv_obj_t *g_lean_bar, *g_lean, *g_max_l, *g_max_r;
lv_obj_t *g_hr_caption, *g_hr_bpm, *g_hr_icon, *g_speed, *g_gps;
lv_obj_t *g_lap_cap, *g_lap_icon, *g_current, *g_best, *g_gap;
lv_obj_t* g_scr = nullptr;
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
    lv_obj_set_size(box, kBoxW, 28);
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

    c->name = make_label(box, fonts::text(), COL_TEXT, 24, 3);
    lv_label_set_text(c->name, name);
    c->state = make_label(box, fonts::small(), COL_CAPTION, 0, 5);
    lv_obj_set_width(c->state, 56);
    lv_obj_set_style_text_align(c->state, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_x(c->state, 86);
}

void chip_set(Chip* c, lv_color_t dot, const char* state, bool blink_on) {
    lv_obj_set_style_bg_color(c->dot, blink_on ? dot : COL_GREY, 0);
    set_text(c->state, state);
}

void build_card(lv_obj_t* scr, Card* c, int x) {
    c->box = lv_obj_create(scr);
    lv_obj_remove_style_all(c->box);
    lv_obj_set_pos(c->box, x, 238);
    lv_obj_set_size(c->box, kBoxW, 76);
    lv_obj_set_style_bg_color(c->box, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(c->box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c->box, 6, 0);
    lv_obj_set_style_border_width(c->box, 2, 0);
    lv_obj_set_style_border_color(c->box, COL_TEXT, 0);
    c->lap = make_label(c->box, fonts::text(), COL_CAPTION, 8, 4);
    c->delta = make_label(c->box, fonts::text(), COL_TEXT, 0, 4);
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

// Small icons: a shape test is rasterised once into an anti-aliased
// white-on-transparent canvas that is recoloured at runtime.
constexpr int kIconW = 24, kIconH = 22;
alignas(4) uint8_t g_heart_buf[kIconW * kIconH * 4];
alignas(4) uint8_t g_clock_buf[kIconW * kIconH * 4];

// `inside(x, y)` takes square coordinates, y in [-1.2, 1.2] (y down), x centred.
template <typename F>
lv_obj_t* make_icon(lv_obj_t* scr, uint8_t* buf, int x, int y, F inside) {
    lv_obj_t* c = lv_canvas_create(scr);
    lv_canvas_set_buffer(c, buf, kIconW, kIconH, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_pos(c, x, y);
    constexpr int kSub = 4;
    for (int py = 0; py < kIconH; ++py) {
        for (int px = 0; px < kIconW; ++px) {
            int hits = 0;
            for (int sy = 0; sy < kSub; ++sy) {
                for (int sx = 0; sx < kSub; ++sx) {
                    const float fx = ((px + (sx + 0.5f) / kSub) / kIconH) * 2.4f - 1.2f * kIconW / kIconH;
                    const float fy = ((py + (sy + 0.5f) / kSub) / kIconH) * 2.4f - 1.2f;
                    hits += inside(fx, fy) ? 1 : 0;
                }
            }
            lv_canvas_set_px(c, px, py, lv_color_white(), (lv_opa_t)(hits * 255 / (kSub * kSub)));
        }
    }
    lv_obj_set_style_image_recolor_opa(c, LV_OPA_COVER, 0);
    return c;
}

float dist_to_segment(float px, float py, float ax, float ay, float bx, float by) {
    const float dx = bx - ax, dy = by - ay;
    float t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    const float ex = ax + t * dx - px, ey = ay + t * dy - py;
    return std::sqrt(ex * ex + ey * ey);
}

lv_obj_t* make_heart_icon(lv_obj_t* scr, int x, int y) {
    // Classic heart curve, y up: (x^2 + y^2 - 1)^3 - x^2 y^3 <= 0
    return make_icon(scr, g_heart_buf, x, y, [](float fx, float fy) {
        fx *= 1.08f;
        fy = 0.05f - fy * 1.08f;
        const float t = fx * fx + fy * fy - 1.0f;
        return t * t * t - fx * fx * fy * fy * fy <= 0.0f;
    });
}

lv_obj_t* make_clock_icon(lv_obj_t* scr, int x, int y) {
    // Ring plus a minute hand (12 o'clock) and an hour hand (4 o'clock).
    return make_icon(scr, g_clock_buf, x, y, [](float fx, float fy) {
        const float r = std::sqrt(fx * fx + fy * fy);
        if (r >= 0.80f && r <= 1.08f) return true;
        return dist_to_segment(fx, fy, 0.0f, 0.0f, 0.0f, -0.62f) <= 0.10f ||
               dist_to_segment(fx, fy, 0.0f, 0.0f, 0.38f, 0.22f) <= 0.10f;
    });
}

// Left block mirrors the right one: icon + caption, a big value, then two
// label/value rows (SPEED and GPS against BEST and GAP).
void build_heart_rate(lv_obj_t* scr) {
    g_hr_icon = make_heart_icon(scr, kLeftX, kIconY);
    lv_obj_set_style_image_recolor(g_hr_icon, COL_GREY, 0);
    g_hr_caption = make_label(scr, fonts::text(), COL_CAPTION, kLeftX + 30, kIconY + 3);
    lv_obj_set_width(g_hr_caption, 200);
    lv_label_set_long_mode(g_hr_caption, LV_LABEL_LONG_CLIP);
    lv_label_set_text(g_hr_caption, i18n::tr(i18n::HEART_RATE));

    g_hr_bpm = make_label(scr, &lv_font_montserrat_48, COL_TEXT, kLeftX, kBigY);
    lv_obj_set_width(g_hr_bpm, 96);
    lv_label_set_long_mode(g_hr_bpm, LV_LABEL_LONG_CLIP);
    lv_obj_t* unit = make_label(scr, fonts::title(), COL_CAPTION, kLeftX + 98, kBigY + 26);
    lv_label_set_text(unit, "bpm");

    lv_obj_t* l = make_label(scr, fonts::text(), COL_CAPTION, kLeftX, kRow1Y + 10);
    lv_label_set_text(l, i18n::tr(i18n::SPEED));
    g_speed = make_label(scr, &lv_font_montserrat_28, COL_TEXT, kLeftX + kValueDx, kRow1Y);
    l = make_label(scr, fonts::text(), COL_CAPTION, kLeftX, kRow2Y + 10);
    lv_label_set_text(l, "GPS");
    g_gps = make_label(scr, &lv_font_montserrat_28, COL_TEXT, kLeftX + kValueDx, kRow2Y);
}

void build_lean(lv_obj_t* scr) {
    g_max_l = make_label(scr, fonts::title(), COL_TEXT, kLeftX, 176);
    g_max_r = make_label(scr, fonts::title(), COL_TEXT, 0, 176);
    lv_obj_set_width(g_max_r, 110);
    lv_obj_set_style_text_align(g_max_r, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_x(g_max_r, 480 - kMargin - 110);
    g_lean = make_label(scr, &lv_font_montserrat_28, COL_TEXT, 0, 170);
    lv_obj_set_width(g_lean, 220);
    lv_obj_set_style_text_align(g_lean, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_x(g_lean, 130);

    g_lean_bar = lv_bar_create(scr);
    lv_obj_set_pos(g_lean_bar, kMargin, 206);
    lv_obj_set_size(g_lean_bar, 480 - 2 * kMargin, 22);
    lv_bar_set_mode(g_lean_bar, LV_BAR_MODE_SYMMETRICAL);
    lv_bar_set_range(g_lean_bar, -kLeanRange, kLeanRange);
    lv_bar_set_value(g_lean_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_anim_duration(g_lean_bar, 0, 0);
    lv_obj_set_style_radius(g_lean_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(g_lean_bar, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(g_lean_bar, lv_color_hex(0xC4C8D0), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_lean_bar, COL_GREEN, LV_PART_INDICATOR);

    lv_obj_t* tick = lv_obj_create(scr);
    lv_obj_remove_style_all(tick);
    lv_obj_set_pos(tick, 239, 202);
    lv_obj_set_size(tick, 3, 30);
    lv_obj_set_style_bg_color(tick, COL_TEXT, 0);
    lv_obj_set_style_bg_opa(tick, LV_OPA_COVER, 0);
}

void build_laps(lv_obj_t* scr) {
    constexpr int x = kRightX;
    g_lap_icon = make_clock_icon(scr, x, kIconY);
    lv_obj_set_style_image_recolor(g_lap_icon, COL_GREY, 0);
    g_lap_cap = make_label(scr, fonts::text(), COL_CAPTION, x + 30, kIconY + 3);
    lv_obj_set_width(g_lap_cap, 480 - kMargin - (x + 30));
    lv_label_set_long_mode(g_lap_cap, LV_LABEL_LONG_CLIP);
    g_current = make_label(scr, &lv_font_montserrat_48, COL_TEXT, x, kBigY);

    lv_obj_t* l = make_label(scr, fonts::text(), COL_CAPTION, x, kRow1Y + 10);
    lv_label_set_text(l, i18n::tr(i18n::BEST));
    g_best = make_label(scr, &lv_font_montserrat_28, COL_PURPLE, x + kValueDx, kRow1Y);

    l = make_label(scr, fonts::text(), COL_CAPTION, x, kRow2Y + 10);
    lv_label_set_text(l, i18n::tr(i18n::GAP));
    g_gap = make_label(scr, &lv_font_montserrat_28, COL_TEXT, x + kValueDx, kRow2Y);

    for (int i = 0; i < 3; ++i) {
        build_card(scr, &g_cards[i], kMargin + i * kBoxPitch);
    }
}

bool blink(uint32_t period_ms) {
    return (lv_tick_get() / period_ms) % 2 == 0;
}

const char* mode_text(gopro_preset_group_t mode) {
    switch (mode) {
        case GOPRO_PRESET_VIDEO: return i18n::tr(i18n::MODE_VIDEO);
        case GOPRO_PRESET_PHOTO: return i18n::tr(i18n::MODE_PHOTO);
        case GOPRO_PRESET_TIMELAPSE: return i18n::tr(i18n::MODE_TIMELAPSE);
        default: return "?";
    }
}

void refresh_chips() {
    const bool slow = blink(500), fast = blink(150);

    switch (racebox_ble::state()) {
        case RaceBoxState::SCANNING: chip_set(&g_chip_rb, COL_BLUE, i18n::tr(i18n::ST_SEARCH), slow); break;
        case RaceBoxState::CONNECTING: chip_set(&g_chip_rb, COL_BLUE, i18n::tr(i18n::ST_LINK), fast); break;
        case RaceBoxState::CONNECTED_IDLE: chip_set(&g_chip_rb, COL_GREEN, i18n::tr(i18n::ST_READY), true); break;
        case RaceBoxState::RECORDING: chip_set(&g_chip_rb, COL_RED, i18n::tr(i18n::ST_REC), true); break;
        case RaceBoxState::RECORDING_PAUSED: chip_set(&g_chip_rb, COL_ORANGE, i18n::tr(i18n::ST_PAUSED), slow); break;
        case RaceBoxState::MEMORY_LOCKED: chip_set(&g_chip_rb, COL_PURPLE, i18n::tr(i18n::ST_LOCKED), fast); break;
    }

    if (app_state_get_recording()) {
        chip_set(&g_chip_gopro, COL_RED, i18n::tr(i18n::ST_REC), blink(400));
    } else {
        switch (app_state_get_conn()) {
            case CONN_STATE_SCANNING: chip_set(&g_chip_gopro, COL_BLUE, i18n::tr(i18n::ST_SEARCH), blink(400)); break;
            case CONN_STATE_CONNECTING: chip_set(&g_chip_gopro, COL_ORANGE, i18n::tr(i18n::ST_PAIRING), true); break;
            case CONN_STATE_READY: chip_set(&g_chip_gopro, COL_GREEN, mode_text(app_state_get_mode()), true); break;
        }
    }

    if (heart_rate_ble_is_connected() || ui_demo_bpm > 0) {
        chip_set(&g_chip_hr, COL_GREEN, i18n::tr(i18n::ST_LIVE), true);
    } else {
        chip_set(&g_chip_hr, COL_BLUE, i18n::tr(i18n::ST_SEARCH), slow);
    }
}

void refresh_heart_rate() {
    uint16_t bpm = 0;
    bool valid = heart_rate_ble_is_connected() && heart_rate_ble_get_bpm(&bpm);
    if (!valid && ui_demo_bpm > 0) {
        bpm = (uint16_t)ui_demo_bpm;
        valid = true;
    }
    char buf[16];
    if (valid) {
        std::snprintf(buf, sizeof(buf), "%u", bpm);
    } else {
        std::snprintf(buf, sizeof(buf), "--");
    }
    set_text(g_hr_bpm, buf);
    set_color(g_hr_bpm, valid ? COL_TEXT : COL_GREY);
    lv_obj_set_style_image_recolor(g_hr_icon, valid ? COL_RED : COL_GREY, 0);
    set_text(g_hr_caption, i18n::tr(valid ? i18n::HEART_RATE : (heart_rate_ble_is_connected() ? i18n::WAIT_PULSE : i18n::NO_STRAP)));
}

void refresh_lean(const telemetry::Snapshot& s, bool live) {
    char buf[32];
    const float lean = live ? s.lean_deg : 0;
    const int v = (int)(lean + (lean < 0 ? -0.5f : 0.5f));
    int clamped = v > kLeanRange ? kLeanRange : (v < -kLeanRange ? -kLeanRange : v);
    lv_bar_set_value(g_lean_bar, clamped, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_lean_bar, lean_color(lean), LV_PART_INDICATOR);

    if (live) {
        std::snprintf(buf, sizeof(buf), "%s %d\xC2\xB0", i18n::tr(i18n::LEAN), std::abs(v));
    } else {
        std::snprintf(buf, sizeof(buf), "%s --", i18n::tr(i18n::LEAN));
    }
    set_text(g_lean, buf);
    set_color(g_lean, live ? lean_color(lean) : COL_GREY);

    std::snprintf(buf, sizeof(buf), "%s %d\xC2\xB0", i18n::tr(i18n::LEFT_SHORT), (int)(s.max_left_deg + 0.5f));
    set_text(g_max_l, buf);
    std::snprintf(buf, sizeof(buf), "%s %d\xC2\xB0", i18n::tr(i18n::RIGHT_SHORT), (int)(s.max_right_deg + 0.5f));
    set_text(g_max_r, buf);

}

void refresh_status(const telemetry::Snapshot& s, bool live) {
    char buf[32];
    const bool mph = settings::get().units == settings::UNITS_MPH;
    const char* unit = mph ? "mph" : "km/h";
    if (live) {
        std::snprintf(buf, sizeof(buf), "%d %s", (int)(s.speed_kmh * (mph ? 0.621371f : 1.0f) + 0.5f), unit);
    } else {
        std::snprintf(buf, sizeof(buf), "-- %s", unit);
    }
    set_text(g_speed, buf);
    set_color(g_speed, live ? COL_TEXT : COL_GREY);

    if (live && s.fix_ok) {
        std::snprintf(buf, sizeof(buf), "3D  %d %s", s.sats, i18n::tr(i18n::SAT));
        set_color(g_gps, COL_GREEN);
    } else if (live) {
        std::snprintf(buf, sizeof(buf), "%s", i18n::tr(i18n::NO_FIX));
        set_color(g_gps, COL_RED);
    } else {
        std::snprintf(buf, sizeof(buf), "--");
        set_color(g_gps, COL_GREY);
    }
    set_text(g_gps, buf);
}

void refresh_laps(const telemetry::Snapshot& s, bool live) {
    char buf[32];

    lv_color_t cap_color = COL_CAPTION;
    if (!s.line_set) {
        std::snprintf(buf, sizeof(buf), "%s", i18n::tr(i18n::NO_LINE));
        cap_color = COL_RED;
    } else if (!s.timing) {
        std::snprintf(buf, sizeof(buf), "%s", i18n::tr(i18n::WAIT_LINE));
        cap_color = COL_ORANGE;
    } else if (live) {
        std::snprintf(buf, sizeof(buf), i18n::tr(i18n::CURRENT_LAP), s.lap_number);
    } else {
        std::snprintf(buf, sizeof(buf), "%s", i18n::tr(i18n::CURRENT));
    }
    set_text(g_lap_cap, buf);
    set_color(g_lap_cap, cap_color);
    lv_obj_set_style_image_recolor(g_lap_icon, s.timing && live ? COL_TEXT : COL_GREY, 0);

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
        std::snprintf(buf, sizeof(buf), "%s %d", i18n::tr(i18n::LAP), s.last_lap[i]);
        set_text(c.lap, buf);
        fmt_time(buf, sizeof(buf), s.last_ms[i]);
        set_text(c.time, buf);
        set_color(c.time, COL_TEXT);
        const bool is_best = s.last_lap[i] == s.best_lap;
        lv_obj_set_style_border_color(c.box, is_best ? COL_PURPLE : COL_TEXT, 0);
        if (is_best) {
            set_text(c.delta, i18n::tr(i18n::BEST));
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
    refresh_heart_rate();
    refresh_lean(s, show);
    refresh_status(s, show);
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
int ui_demo_bpm = 0;

namespace {

void open_settings(lv_event_t*) {
    config_ui_open();
}

void build_all() {
    lv_obj_clean(g_scr);
    build_chip(g_scr, &g_chip_rb, kMargin, "RaceBox");
    build_chip(g_scr, &g_chip_gopro, kMargin + kBoxPitch, "GoPro");
    build_chip(g_scr, &g_chip_hr, kMargin + 2 * kBoxPitch, i18n::tr(i18n::CHIP_HEART));
    build_heart_rate(g_scr);
    build_lean(g_scr);
    build_laps(g_scr);

    // Long press on the status chips opens the settings (touch only).
    lv_obj_t* hit = lv_obj_create(g_scr);
    lv_obj_remove_style_all(hit);
    lv_obj_set_pos(hit, 0, 0);
    lv_obj_set_size(hit, 480, 34);
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hit, open_settings, LV_EVENT_LONG_PRESSED, nullptr);
}

} // namespace

void ui_show_dashboard(bool rebuild) {
    if (rebuild) {
        build_all();
    }
    lv_screen_load(g_scr);
    refresh(nullptr);
}

void ui_create(void) {
    g_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(g_scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(g_scr, LV_OBJ_FLAG_SCROLLABLE);
    build_all();
    lv_screen_load(g_scr);

    lv_timer_create(refresh, 40, nullptr);
    refresh(nullptr);
}
