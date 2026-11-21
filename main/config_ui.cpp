#include "config_ui.h"

#include <cstdio>

#include "esp_log.h"
#include "esp_system.h"
#include "lvgl.h"
#include "sdkconfig.h"

#include "demo.h"
#include "display.h"
#include "fonts.h"
#include "i18n.h"
#include "peer_pins.h"
#include "settings.h"
#include "sysinfo.h"
#include "telemetry.h"
#include "ui.h"
#include "version.h"

namespace {

#define COL_BG      lv_color_hex(0xE8EAEE)
#define COL_PANEL   lv_color_hex(0xF8F9FA)
#define COL_PRESSED lv_color_hex(0xD6DAE2)
#define COL_TEXT    lv_color_hex(0x101010)
#define COL_CAPTION lv_color_hex(0x505560)
#define COL_BUTTON  lv_color_hex(0xC4C8D0)
#define COL_BLUE    lv_color_hex(0x1E78E0)

constexpr int kScreenW = 480;
constexpr int kHeaderH = 44;
constexpr int kSide = 8;
constexpr int kRowW = kScreenW - 2 * kSide;
constexpr int kRowH = 48;
constexpr uint32_t kIdleCloseMs = 45000;

lv_obj_t* g_scr = nullptr;
lv_obj_t* g_list = nullptr;
lv_obj_t* g_toast = nullptr;
lv_timer_t* g_idle = nullptr;
lv_timer_t* g_toast_timer = nullptr;
bool g_lang_changed = false;
lv_obj_t* g_sys_vals = nullptr;   // values of the System row, refreshed every second
lv_timer_t* g_sys_timer = nullptr;
bool g_cpu_ready = false;         // the CPU load needs one full interval before it means anything

// ----------------------------------------------------------------- helpers

lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color, const char* text) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, text);
    return l;
}

lv_obj_t* make_row(int height) {
    lv_obj_t* row = lv_obj_create(g_list);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, kRowW, height);
    lv_obj_set_style_bg_color(row, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_border_width(row, 2, 0);
    lv_obj_set_style_border_color(row, COL_TEXT, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    return row;
}

lv_obj_t* row_title(lv_obj_t* row, const char* text) {
    lv_obj_t* l = make_label(row, fonts::text(), COL_TEXT, text);
    lv_obj_set_width(l, 205);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 12, 0);
    return l;
}

lv_obj_t* make_arrow(lv_obj_t* row, const char* symbol, int x, lv_event_cb_t cb) {
    lv_obj_t* b = lv_button_create(row);
    lv_obj_set_size(b, 48, 38);
    lv_obj_align(b, LV_ALIGN_LEFT_MID, x, 0);
    lv_obj_set_style_bg_color(b, COL_BUTTON, 0);
    lv_obj_set_style_bg_color(b, COL_PRESSED, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_t* l = make_label(b, fonts::title(), COL_TEXT, symbol);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    return b;
}

void show_toast(const char* text) {
    lv_label_set_text(lv_obj_get_child(g_toast, 0), text);
    lv_obj_remove_flag(g_toast, LV_OBJ_FLAG_HIDDEN);
    if (g_toast_timer != nullptr) {
        lv_timer_delete(g_toast_timer);
    }
    g_toast_timer = lv_timer_create(
        [](lv_timer_t* t) {
            if (g_toast != nullptr) {
                lv_obj_add_flag(g_toast, LV_OBJ_FLAG_HIDDEN);
            }
            g_toast_timer = nullptr;
            lv_timer_delete(t);
        },
        2000, nullptr);
}

// ------------------------------------------------------------------ screen

void build();

// Fills the value column of the System row (one line per caption).
void refresh_sys() {
    if (g_sys_vals == nullptr) {
        return;
    }
    SysInfo s;
    sysinfo_sample(&s);

    char buf[192];
    int n = 0;
    if (!g_cpu_ready) {
        n += snprintf(buf + n, sizeof(buf) - n, "--\n");
    } else if (s.cores > 1) {
        n += snprintf(buf + n, sizeof(buf) - n, "%u %%   (%u / %u)\n", s.cpu_pct, s.core_pct[0], s.core_pct[1]);
    } else {
        n += snprintf(buf + n, sizeof(buf) - n, "%u %%\n", s.cpu_pct);
    }
    n += snprintf(buf + n, sizeof(buf) - n, "%u / %u KB   (min %u)\n", (unsigned)(s.ram_free / 1024),
                  (unsigned)(s.ram_total / 1024), (unsigned)(s.ram_min_free / 1024));
    if (s.psram_total > 0) {
        n += snprintf(buf + n, sizeof(buf) - n, "%u / %u KB\n", (unsigned)(s.psram_free / 1024),
                      (unsigned)(s.psram_total / 1024));
    }
    const unsigned up = s.uptime_s;
    if (up >= 3600) {
        snprintf(buf + n, sizeof(buf) - n, "%u h %02u min", up / 3600, (up / 60) % 60);
    } else {
        snprintf(buf + n, sizeof(buf) - n, "%u min %02u s", up / 60, up % 60);
    }
    lv_label_set_text(g_sys_vals, buf);
}

void close_screen() {
    lv_obj_t* old = g_scr;
    if (g_sys_timer != nullptr) {
        lv_timer_delete(g_sys_timer);
        g_sys_timer = nullptr;
    }
    g_sys_vals = nullptr;
    if (g_idle != nullptr) {
        lv_timer_delete(g_idle);
        g_idle = nullptr;
    }
    if (g_toast_timer != nullptr) {
        lv_timer_delete(g_toast_timer);
        g_toast_timer = nullptr;
    }
    ui_show_dashboard(g_lang_changed);
    g_lang_changed = false;
    g_scr = g_list = g_toast = nullptr;
    if (old != nullptr) {
        lv_obj_delete_async(old);
    }
}

// Rebuilds this screen (new language) from outside the event that asked for it.
void rebuild_async(void*) {
    if (g_scr == nullptr) {
        return;
    }
    const int32_t scroll = lv_obj_get_scroll_y(g_list);
    lv_obj_t* old = g_scr;
    build();
    lv_obj_update_layout(g_list);
    lv_obj_scroll_to_y(g_list, scroll, LV_ANIM_OFF);
    lv_obj_delete_async(old);
}

// ------------------------------------------------------------------- rows

void on_back(lv_event_t*) {
    close_screen();
}

lv_obj_t* g_lang_value = nullptr;
lv_obj_t* g_units_value = nullptr;

void change_lang(int step) {
    const int n = settings::LANG_COUNT;
    settings::set_lang((uint8_t)((settings::get().lang + n + step) % n));
    g_lang_changed = true;
    lv_async_call(rebuild_async, nullptr);
}

void on_lang_prev(lv_event_t*) { change_lang(-1); }
void on_lang_next(lv_event_t*) { change_lang(+1); }

void change_units(lv_event_t*) {
    settings::set_units(settings::get().units == settings::UNITS_KMH ? settings::UNITS_MPH : settings::UNITS_KMH);
    lv_label_set_text(g_units_value, settings::get().units == settings::UNITS_KMH ? "km/h" : "mph");
}

void on_brightness(lv_event_t* e) {
    lv_obj_t* s = lv_event_get_target_obj(e);
    const int v = lv_slider_get_value(s);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        settings::set_brightness((uint8_t)v);
    } else {
        display_set_brightness(v);
    }
}

void on_demo(lv_event_t* e) {
    const bool on = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    settings::set_demo(on);
    demo_set_enabled(on);
}

void on_forget_bt(lv_event_t*) {
    peer_pins_forget_all();
    show_toast(i18n::tr(i18n::DONE_BT));
}

void on_reset_laps(lv_event_t*) {
    telemetry::reset_session();
    show_toast(i18n::tr(i18n::DONE_LAPS));
}

void on_restart(lv_event_t*) {
    esp_restart();
}

void add_cycler(const char* title, const char* value, lv_obj_t** value_label, lv_event_cb_t prev, lv_event_cb_t next) {
    lv_obj_t* row = make_row(kRowH);
    row_title(row, title);
    make_arrow(row, LV_SYMBOL_LEFT, 214, prev);
    lv_obj_t* v = make_label(row, fonts::text(), COL_TEXT, value);
    lv_obj_set_width(v, 100);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(v, LV_ALIGN_LEFT_MID, 266, 0);
    make_arrow(row, LV_SYMBOL_RIGHT, 366, next);
    if (value_label != nullptr) {
        *value_label = v;
    }
}

void add_action(const char* title, const char* symbol, lv_event_cb_t cb) {
    lv_obj_t* row = make_row(kRowH);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(row, COL_PRESSED, LV_STATE_PRESSED);
    lv_obj_t* l = make_label(row, fonts::text(), COL_TEXT, title);
    lv_obj_set_width(l, kRowW - 70);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_t* s = make_label(row, fonts::title(), COL_CAPTION, symbol);
    lv_obj_align(s, LV_ALIGN_RIGHT_MID, -14, 0);
    lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, nullptr);
}

void build() {
    using namespace i18n;
    const settings::Data& cfg = settings::get();

    g_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(g_scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(g_scr, LV_OBJ_FLAG_SCROLLABLE);

    // Header: back button and title.
    lv_obj_t* back = lv_button_create(g_scr);
    lv_obj_set_size(back, 104, 36);
    lv_obj_set_pos(back, kSide, 4);
    lv_obj_set_style_bg_color(back, COL_BUTTON, 0);
    lv_obj_set_style_bg_color(back, COL_PRESSED, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_set_style_radius(back, 6, 0);
    lv_obj_t* bl = make_label(back, fonts::text(), COL_TEXT, "");
    lv_label_set_text_fmt(bl, LV_SYMBOL_LEFT "  %s", tr(BACK));
    lv_obj_center(bl);
    lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* title = make_label(g_scr, fonts::title(), COL_TEXT, tr(SETTINGS));
    lv_obj_set_width(title, 240);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Scrollable list of settings.
    g_list = lv_obj_create(g_scr);
    lv_obj_remove_style_all(g_list);
    lv_obj_set_pos(g_list, kSide, kHeaderH + 2);
    lv_obj_set_size(g_list, kRowW, 320 - kHeaderH - 2);
    lv_obj_set_flex_flow(g_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_list, 6, 0);
    lv_obj_set_style_pad_bottom(g_list, 8, 0);
    lv_obj_add_flag(g_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(g_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_list, LV_SCROLLBAR_MODE_ACTIVE);

    add_cycler(tr(LANGUAGE), language_name(cfg.lang), &g_lang_value, on_lang_prev, on_lang_next);
    add_cycler(tr(SPEED_UNIT), cfg.units == settings::UNITS_KMH ? "km/h" : "mph", &g_units_value, change_units,
               change_units);

    lv_obj_t* row = make_row(kRowH);
    row_title(row, tr(BRIGHTNESS));
    lv_obj_t* slider = lv_slider_create(row);
    lv_obj_set_size(slider, 190, 14);
    lv_obj_align(slider, LV_ALIGN_LEFT_MID, 234, 0);
    lv_slider_set_range(slider, 10, 100);
    lv_slider_set_value(slider, cfg.brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, COL_BUTTON, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, COL_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, COL_BLUE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 6, LV_PART_KNOB);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_set_ext_click_area(slider, 14);
    lv_obj_add_event_cb(slider, on_brightness, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(slider, on_brightness, LV_EVENT_RELEASED, nullptr);

    row = make_row(kRowH);
    row_title(row, tr(DEMO_MODE));
    lv_obj_t* sw = lv_switch_create(row);
    lv_obj_set_size(sw, 64, 32);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -14, 0);
    lv_obj_set_style_bg_color(sw, COL_BLUE, (lv_style_selector_t)LV_PART_INDICATOR | (lv_style_selector_t)LV_STATE_CHECKED);
    if (cfg.demo) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, on_demo, LV_EVENT_VALUE_CHANGED, nullptr);

    add_action(tr(FORGET_BT), LV_SYMBOL_BLUETOOTH, on_forget_bt);
    add_action(tr(RESET_LAPS), LV_SYMBOL_REFRESH, on_reset_laps);
    add_action(tr(RESTART), LV_SYMBOL_POWER, on_restart);

    // System usage: captions on the left, live values on the right.
    {
        SysInfo probe;
        sysinfo_sample(&probe);
        const bool has_psram = probe.psram_total > 0;
        const int lines = has_psram ? 4 : 3;
        row = make_row(34 + lines * 18);
        lv_obj_t* st = make_label(row, fonts::text(), COL_TEXT, tr(SYSTEM));
        lv_obj_align(st, LV_ALIGN_TOP_LEFT, 12, 6);

        char caps[96];
        int n = snprintf(caps, sizeof(caps), "CPU\n%s\n", tr(SYS_RAM));
        if (has_psram) {
            n += snprintf(caps + n, sizeof(caps) - n, "%s\n", tr(SYS_PSRAM));
        }
        snprintf(caps + n, sizeof(caps) - n, "%s", tr(SYS_UPTIME));
        lv_obj_t* cap = make_label(row, fonts::small(), COL_CAPTION, caps);
        lv_obj_set_style_text_line_space(cap, 4, 0);
        lv_obj_align(cap, LV_ALIGN_TOP_LEFT, 12, 32);

        g_sys_vals = make_label(row, fonts::small(), COL_TEXT, "");
        lv_obj_set_style_text_line_space(g_sys_vals, 4, 0);
        lv_obj_align(g_sys_vals, LV_ALIGN_TOP_LEFT, 130, 32);
        refresh_sys();
    }

    // Firmware version.
    row = make_row(66);
    lv_obj_t* fw = make_label(row, fonts::text(), COL_TEXT, tr(FIRMWARE));
    lv_obj_align(fw, LV_ALIGN_TOP_LEFT, 12, 8);
    lv_obj_t* ver = make_label(row, fonts::title(), COL_TEXT, "");
    lv_label_set_text_fmt(ver, "%s  v%s", FIRMWARE_NAME, FIRMWARE_VERSION);
    lv_obj_align(ver, LV_ALIGN_TOP_RIGHT, -12, 5);
    lv_obj_t* det = make_label(row, fonts::small(), COL_CAPTION, "");
    lv_label_set_text_fmt(det, "%s  |  %s  |  ESP-IDF %s", __DATE__, CONFIG_IDF_TARGET, esp_get_idf_version());
    lv_obj_set_width(det, kRowW - 24);
    lv_label_set_long_mode(det, LV_LABEL_LONG_DOT);
    lv_obj_align(det, LV_ALIGN_BOTTOM_LEFT, 12, -8);

    // Short message after an action, above the list.
    g_toast = lv_obj_create(g_scr);
    lv_obj_remove_style_all(g_toast);
    lv_obj_set_size(g_toast, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(g_toast, COL_TEXT, 0);
    lv_obj_set_style_bg_opa(g_toast, LV_OPA_90, 0);
    lv_obj_set_style_radius(g_toast, 8, 0);
    lv_obj_set_style_pad_hor(g_toast, 14, 0);
    lv_obj_set_style_pad_ver(g_toast, 8, 0);
    lv_obj_align(g_toast, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_add_flag(g_toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_toast, LV_OBJ_FLAG_FLOATING);
    lv_obj_remove_flag(g_toast, LV_OBJ_FLAG_CLICKABLE);
    make_label(g_toast, fonts::text(), lv_color_white(), "");

    lv_screen_load(g_scr);
}

} // namespace

void config_ui_open(void) {
    if (g_scr != nullptr) {
        return;
    }
    g_lang_changed = false;
    g_cpu_ready = false;
    SysInfo baseline;
    sysinfo_sample(&baseline);  // start of the first CPU load interval
    build();
    g_sys_timer = lv_timer_create(
        [](lv_timer_t*) {
            g_cpu_ready = true;
            refresh_sys();
        },
        1000, nullptr);
    g_idle = lv_timer_create(
        [](lv_timer_t*) {
            if (g_scr != nullptr && lv_display_get_inactive_time(nullptr) > kIdleCloseMs) {
                close_screen();
            }
        },
        1000, nullptr);
}
