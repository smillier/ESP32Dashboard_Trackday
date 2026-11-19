#include "splash.h"

#include <cmath>

#include "lvgl.h"

#include "fonts.h"
#include "version.h"

namespace {

#define COL_BG      lv_color_hex(0x0E0F12)
#define COL_RED     lv_color_hex(0xE8202A)
#define COL_RED_DIM lv_color_hex(0x3A1014)
#define COL_WHITE   lv_color_hex(0xF4F5F7)
#define COL_GREY    lv_color_hex(0x8A909C)
#define COL_GREEN   lv_color_hex(0x22C55E)
#define COL_CHECK   lv_color_hex(0xD8DADF)
#define COL_CHECK_D lv_color_hex(0x1B1D22)

constexpr float kShear = 0.26f;     // italic lean: sideways shift per pixel of height
constexpr int kCell = 16;           // checkered square
constexpr int kLights = 5;
constexpr int kLightD = 30;

constexpr const char* kLine1 = "ESP32 DASHBOARD";
constexpr const char* kLine2 = "TRACKDAY";

lv_obj_t* g_prev = nullptr;         // the dashboard screen to fade into
lv_obj_t* g_scr = nullptr;
lv_obj_t* g_lights[kLights];
lv_obj_t* g_caption = nullptr;
lv_timer_t* g_timer = nullptr;
int g_step = 0;

void anim_translate_x(void* obj, int32_t v) {
    lv_obj_set_style_translate_x((lv_obj_t*)obj, v, 0);
}

void anim_opa(void* obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
}

void start_anim(lv_obj_t* obj, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms, uint32_t delay,
                lv_anim_path_cb_t path, bool infinite = false) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_path_cb(&a, path);
    if (infinite) {
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    }
    lv_anim_start(&a);
}

lv_obj_t* make_rect(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t color) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, color, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

void free_mask(lv_event_t* e) {
    lv_draw_buf_destroy((lv_draw_buf_t*)lv_event_get_user_data(e));
}

// Shows an 8-bit alpha mask tinted with `color`. The canvas owns the buffer.
lv_obj_t* show_mask(lv_obj_t* parent, lv_draw_buf_t* mask, lv_color_t color) {
    lv_obj_t* c = lv_canvas_create(parent);
    lv_canvas_set_draw_buf(c, mask);
    lv_obj_set_size(c, mask->header.w, mask->header.h);
    lv_obj_update_layout(c);
    lv_obj_set_style_image_recolor(c, color, 0);
    lv_obj_set_style_image_recolor_opa(c, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(c, free_mask, LV_EVENT_DELETE, mask);
    return c;
}

// Slants a w x h alpha source to the right (top shifted most), with sub-pixel
// accuracy, into a new A8 buffer. LVGL has no italic fonts and no skew for
// widgets, so the racing slant is done on the pixels.
template <typename F>
lv_draw_buf_t* shear(int w, int h, F alpha) {
    const int ow = w + (int)std::ceil(h * kShear) + 1;
    lv_draw_buf_t* out = lv_draw_buf_create(ow, h, LV_COLOR_FORMAT_A8, 0);
    if (out == nullptr) {
        return nullptr;
    }
    const uint32_t stride = out->header.stride;
    uint8_t* d = (uint8_t*)out->data;
    for (int y = 0; y < h; ++y) {
        const float shift = (h - 1 - y) * kShear;
        const int s = (int)shift;
        const float f = shift - s;
        for (int x = 0; x < ow; ++x) {
            const int sx = x - s;
            const float v = (1.0f - f) * alpha(sx, y) + f * alpha(sx - 1, y);
            d[y * stride + x] = (uint8_t)(v + 0.5f);
        }
    }
    return out;
}

lv_obj_t* make_slanted_rect(lv_obj_t* parent, int w, int h, lv_color_t color) {
    lv_draw_buf_t* m = shear(w, h, [&](int x, int) { return (x >= 0 && x < w) ? 255 : 0; });
    return m ? show_mask(parent, m, color) : nullptr;
}

lv_obj_t* make_slanted_text(lv_obj_t* parent, const lv_font_t* font, const char* text, int letter_space,
                            lv_color_t color) {
    lv_obj_t* tmp = lv_label_create(parent);
    lv_obj_set_style_text_font(tmp, font, 0);
    lv_obj_set_style_text_color(tmp, lv_color_white(), 0);
    lv_obj_set_style_text_letter_space(tmp, letter_space, 0);
    lv_label_set_text(tmp, text);
    lv_obj_update_layout(tmp);
    // The label box clips the last glyph advance and the descender; give it room.
    const int w = lv_obj_get_width(tmp) + letter_space + 12, h = lv_obj_get_height(tmp) + 8;
    lv_obj_set_size(tmp, w, h);
    lv_obj_update_layout(tmp);
    lv_draw_buf_t* snap = lv_snapshot_take(tmp, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_delete(tmp);
    if (snap == nullptr) {
        return nullptr;
    }
    const uint8_t* data = (const uint8_t*)snap->data;
    const uint32_t stride = snap->header.stride;
    lv_draw_buf_t* m = shear(w, h, [&](int x, int y) { return (x >= 0 && x < w) ? data[y * stride + x * 4 + 3] : 0; });
    lv_draw_buf_destroy(snap);
    return m ? show_mask(parent, m, color) : nullptr;
}

// Two rows of alternating squares that scroll sideways like a flag.
void make_checker_strip(lv_obj_t* scr, int y, bool to_right) {
    lv_obj_t* clip = lv_obj_create(scr);
    lv_obj_remove_style_all(clip);
    lv_obj_set_pos(clip, 0, y);
    lv_obj_set_size(clip, 480, 2 * kCell);
    lv_obj_set_style_clip_corner(clip, true, 0);

    lv_obj_t* strip = lv_obj_create(clip);
    lv_obj_remove_style_all(strip);
    lv_obj_set_pos(strip, -2 * kCell, 0);
    lv_obj_set_size(strip, 480 + 4 * kCell, 2 * kCell);
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 480 / kCell + 4; ++c) {
            make_rect(strip, c * kCell, r * kCell, kCell, kCell, ((r + c) & 1) ? COL_CHECK : COL_CHECK_D);
        }
    }
    start_anim(strip, anim_translate_x, to_right ? 0 : 2 * kCell, to_right ? 2 * kCell : 0, 700, 0,
               lv_anim_path_linear, true);
}

// Thin slanted streaks that rush past behind the title.
void make_streak(lv_obj_t* scr, int y, int w, int h, lv_color_t color, lv_opa_t opa, uint32_t ms, uint32_t delay) {
    lv_obj_t* s = make_slanted_rect(scr, w, h, color);
    if (s == nullptr) {
        return;
    }
    lv_obj_set_y(s, y);
    lv_obj_set_style_opa(s, opa, 0);
    start_anim(s, anim_translate_x, 480, -w - 20, ms, delay, lv_anim_path_linear, true);
}

void set_light(int i, bool on) {
    lv_obj_t* l = g_lights[i];
    lv_obj_set_style_bg_color(l, on ? COL_RED : COL_RED_DIM, 0);
    lv_obj_set_style_shadow_width(l, on ? 22 : 0, 0);
    lv_obj_set_style_shadow_color(l, COL_RED, 0);
    lv_obj_set_style_shadow_spread(l, on ? 2 : 0, 0);
}

// 100 ms steps: lights come on one by one, hold, go out together (GO), fade.
void tick(lv_timer_t* t) {
    ++g_step;
    constexpr int kFirstLight = 8, kLightGap = 3;
    const int lit = (g_step - kFirstLight) / kLightGap + 1;
    if (g_step >= kFirstLight && (g_step - kFirstLight) % kLightGap == 0 && lit <= kLights) {
        set_light(lit - 1, true);
    }
    const int kLightsOut = kFirstLight + kLightGap * kLights + 4;
    if (g_step == kLightsOut) {
        for (int i = 0; i < kLights; ++i) {
            set_light(i, false);
        }
        lv_label_set_text(g_caption, "GO");
    }
    if (g_step == kLightsOut + 4) {
        lv_timer_delete(t);
        g_timer = nullptr;
        lv_screen_load_anim(g_prev, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, true);   // deletes the splash
        g_scr = nullptr;
    }
}

// Centres a canvas horizontally on the screen, at row y.
void place_centred(lv_obj_t* o, int y) {
    if (o != nullptr) {
        lv_obj_set_pos(o, (480 - lv_obj_get_width(o)) / 2, y);
    }
}

} // namespace

void splash_run(void) {
    g_prev = lv_screen_active();
    g_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(g_scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(g_scr, LV_OBJ_FLAG_SCROLLABLE);

    // Streaks first so everything else sits on top.
    make_streak(g_scr, 62, 150, 3, COL_RED, LV_OPA_60, 620, 0);
    make_streak(g_scr, 84, 90, 2, COL_GREY, LV_OPA_40, 480, 150);
    make_streak(g_scr, 196, 170, 3, COL_RED, LV_OPA_50, 700, 300);
    make_streak(g_scr, 214, 100, 2, COL_GREY, LV_OPA_30, 540, 80);
    make_streak(g_scr, 252, 130, 2, COL_RED, LV_OPA_40, 600, 400);

    make_checker_strip(g_scr, 0, true);
    make_checker_strip(g_scr, 320 - 2 * kCell, false);

    // Product name, sliding in from the sides.
    lv_obj_t* l1 = make_slanted_text(g_scr, fonts::title(), kLine1, 6, COL_GREY);
    lv_obj_t* l2 = make_slanted_text(g_scr, &lv_font_montserrat_48, kLine2, 8, COL_WHITE);
    place_centred(l1, 66);
    place_centred(l2, 86);
    if (l1) start_anim(l1, anim_translate_x, 520, 0, 520, 0, lv_anim_path_ease_out);
    if (l2) start_anim(l2, anim_translate_x, -520, 0, 520, 120, lv_anim_path_ease_out);

    // Red slash under the title.
    lv_obj_t* bar = make_slanted_rect(g_scr, 330, 8, COL_RED);
    place_centred(bar, 160);
    if (bar) start_anim(bar, anim_translate_x, -560, 0, 480, 450, lv_anim_path_ease_out);

    // Version badge.
    lv_obj_t* badge = make_slanted_rect(g_scr, 104, 30, COL_RED);
    lv_obj_t* ver = nullptr;
    char vtxt[24];
    lv_snprintf(vtxt, sizeof(vtxt), "V%s", FIRMWARE_VERSION);
    ver = make_slanted_text(g_scr, fonts::title(), vtxt, 2, COL_WHITE);
    if (badge && ver) {
        const int bx = 240 + 165 - lv_obj_get_width(badge);
        lv_obj_set_pos(badge, bx, 180);
        lv_obj_set_pos(ver, bx + (lv_obj_get_width(badge) - lv_obj_get_width(ver)) / 2, 180 + (30 - lv_obj_get_height(ver)) / 2);
        lv_obj_set_style_opa(badge, LV_OPA_TRANSP, 0);
        lv_obj_set_style_opa(ver, LV_OPA_TRANSP, 0);
        start_anim(badge, anim_opa, 0, 255, 250, 800, lv_anim_path_linear);
        start_anim(ver, anim_opa, 0, 255, 250, 800, lv_anim_path_linear);
    }

    // Five start lights.
    constexpr int gap = 14;
    const int total = kLights * kLightD + (kLights - 1) * gap;
    for (int i = 0; i < kLights; ++i) {
        lv_obj_t* d = make_rect(g_scr, (480 - total) / 2 + i * (kLightD + gap), 232, kLightD, kLightD, COL_RED_DIM);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(d, 2, 0);
        lv_obj_set_style_border_color(d, lv_color_hex(0x4A4D55), 0);
        g_lights[i] = d;
    }
    g_caption = lv_label_create(g_scr);
    lv_label_set_text(g_caption, "");
    lv_obj_set_style_text_font(g_caption, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(g_caption, COL_GREEN, 0);
    lv_obj_set_style_text_letter_space(g_caption, 6, 0);
    lv_obj_set_width(g_caption, 200);
    lv_obj_set_style_text_align(g_caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(g_caption, 140, 270);

    lv_screen_load(g_scr);
    g_step = 0;
    g_timer = lv_timer_create(tick, 100, nullptr);
}
