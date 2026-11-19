#include "demo.h"

#include <cmath>

#include "esp_log.h"
#include "lvgl.h"

#include "line_store.h"
#include "telemetry.h"
#include "ui.h"

namespace {

constexpr char kTag[] = "demo";
constexpr double kPi = 3.14159265358979323846;
constexpr double kRadius = 64.0;
constexpr double kLat0 = 48.8566, kLon0 = 2.3522;
constexpr int kTickMs = 40;

double g_theta = -kPi / 2;   // clockwise from north, starts on the line heading north
uint32_t g_itow = 100000;
double g_t = 0;
uint32_t g_last_laps = 0;
lv_timer_t* g_timer = nullptr;

void tick(lv_timer_t*) {
    const int lap = (int)(g_t / 21.0);
    const double v = 20.0 + 1.2 * std::sin(lap * 1.7);     // m/s, varies per lap
    const double w = v / kRadius;
    g_theta += w * kTickMs / 1000.0;
    g_t += kTickMs / 1000.0;
    g_itow += kTickMs;

    const double e = kRadius * std::sin(g_theta), n = kRadius * std::cos(g_theta);
    telemetry::Sample s{};
    s.itow_ms = g_itow;
    s.lat_e7 = (int32_t)((kLat0 + n / 110574.0) * 1e7);
    s.lon_e7 = (int32_t)((kLon0 + e / (111320.0 * std::cos(kLat0 * kPi / 180.0))) * 1e7);
    s.speed_mm_s = (int32_t)(v * 1000);
    s.heading_e5 = (int32_t)(std::fmod((g_theta + kPi / 2) * 180.0 / kPi + 720.0, 360.0) * 1e5);
    s.fix = 3;
    s.fix_flags = 1;
    s.sats = 14;
    s.battery = 85;
    const double lat_g = v * v / (kRadius * 9.80665);
    const double lean = std::atan(lat_g);
    s.rz = (float)(-w * std::cos(lean) * 180.0 / kPi);      // body yaw rate = world rate * cos(lean), Z up
    s.gz = (float)std::sqrt(1.0 + lat_g * lat_g);           // resultant stays on the bike's Z axis
    telemetry::feed(s);
    ui_demo_bpm = (int)(118.0 + 34.0 * lat_g / 0.65 + 6.0 * std::sin(g_t * 0.7));

    if (g_t > 0.2 && g_t < 0.3) {
        telemetry::Line line;
        telemetry::mark_line(&line);
    }

    telemetry::Snapshot snap;
    telemetry::snapshot(&snap);
    if (snap.lap_count != g_last_laps) {
        g_last_laps = snap.lap_count;
        ESP_LOGI(kTag, "lap %u: %u ms best %u ms gap %d ms lean %.1f (expected ~%.1f)", (unsigned)snap.lap_count,
                 (unsigned)snap.last_ms[0], (unsigned)snap.best_ms, (int)snap.gap_ms, (double)snap.lean_deg,
                 std::atan(lat_g) * 180.0 / kPi);
    }
}

} // namespace

void demo_set_enabled(bool on) {
    if (on) {
        g_theta = -kPi / 2;
        g_t = 0;
        g_itow = 100000;
        g_last_laps = 0;
        telemetry::clear_line();   // the demo marks its own line
        ui_demo_active = true;
        if (g_timer == nullptr) {
            g_timer = lv_timer_create(tick, kTickMs, nullptr);
        } else {
            lv_timer_resume(g_timer);
        }
    } else {
        if (g_timer != nullptr) {
            lv_timer_pause(g_timer);
        }
        ui_demo_active = false;
        ui_demo_bpm = 0;
        telemetry::clear_line();   // drop the demo line, bring back the saved one
        telemetry::Line line;
        if (line_store::load(&line)) {
            telemetry::set_line(line);
        }
    }
}
