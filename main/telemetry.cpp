#include "telemetry.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <mutex>

#include "config.h"

namespace telemetry {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr float kG = 9.80665f;
constexpr double kWeekMs = 604800000.0;
constexpr float kMaxLean = 70.0f;

std::mutex g_mutex;

// ---- latest sample / lean ---------------------------------------------------
Snapshot g_snap = {};
bool g_have_prev = false;
double g_time_ms = 0;       // unwrapped GPS time of the last sample
double g_week_offset = 0;
uint32_t g_last_itow = 0;
float g_lean = 0;
float g_max_left = 0, g_max_right = 0;
Sample g_latest = {};     // kept for mark_line()

// ---- lap timing -------------------------------------------------------------
bool g_line_set = false;
Line g_line = {};
double g_lat0_scale_e = 0, g_scale_n = 0;   // metres per 1e-7 degree
bool g_timing = false;
double g_lap_start_ms = 0;
double g_last_cross_ms = -1e12;
bool g_have_d = false;
double g_prev_d = 0, g_prev_c = 0, g_prev_time_ms = 0;
int g_lap_number = 0;       // completed laps
uint32_t g_best_ms = 0;
int g_best_lap = 0;
uint32_t g_last_ms[3] = {};
int g_last_lap[3] = {};

void to_local(int32_t lat_e7, int32_t lon_e7, double* e, double* n) {
    *e = (double)(lon_e7 - g_line.lon_e7) * g_lat0_scale_e;
    *n = (double)(lat_e7 - g_line.lat_e7) * g_scale_n;
}

void update_line_scale() {
    const double lat = g_line.lat_e7 * 1e-7 * kPi / 180.0;
    g_scale_n = 1e-7 * 110574.0;
    g_lat0_scale_e = 1e-7 * 111320.0 * std::cos(lat);
}

void along_cross(const Sample& s, double* d, double* c) {
    double e, n;
    to_local(s.lat_e7, s.lon_e7, &e, &n);
    const double h = g_line.heading_deg * kPi / 180.0;
    *d = e * std::sin(h) + n * std::cos(h);
    *c = e * std::cos(h) - n * std::sin(h);
}

void update_lean(const Sample& s, float dt, float speed_ms) {
    float roll_gyro = g_lean + kRollRateSign * s.rx * dt;

    // A rider keeps the resultant of gravity and centripetal force along the
    // bike's vertical axis, so the accelerometer alone reads ~0 in a corner.
    // Use v * yaw rate (lateral acceleration) as the reference instead, and
    // the accelerometer's tilt when nearly stopped.
    float ref, k;
    if (speed_ms > 4.0f) {
        const float yaw_rad = kYawRateSign * s.rz * (float)(kPi / 180.0);
        const float x = std::clamp(speed_ms * yaw_rad / kG, -0.99f, 0.99f);
        ref = std::asin(x) * (float)(180.0 / kPi);
        k = 0.03f;
    } else {
        ref = kAccelRollSign * std::atan2(s.gy, s.gz) * (float)(180.0 / kPi);
        k = 0.05f;
    }
    g_lean = std::clamp(roll_gyro + k * (ref - roll_gyro), -kMaxLean, kMaxLean);

    if (speed_ms > 4.0f) {
        g_max_left = std::max(g_max_left, -g_lean);
        g_max_right = std::max(g_max_right, g_lean);
    }
}

void complete_lap(double t_cross_ms) {
    const uint32_t lap_ms = (uint32_t)std::lround(t_cross_ms - g_lap_start_ms);
    const uint32_t prev_best = g_best_ms;
    const uint32_t prev_last = g_last_ms[0];
    ++g_lap_number;

    for (int i = 2; i > 0; --i) {
        g_last_ms[i] = g_last_ms[i - 1];
        g_last_lap[i] = g_last_lap[i - 1];
    }
    g_last_ms[0] = lap_ms;
    g_last_lap[0] = g_lap_number;

    LapEvent ev;
    if (prev_best == 0) {
        ev = LapEvent::FIRST;
    } else if (lap_ms < prev_best) {
        ev = LapEvent::BEST;
    } else {
        ev = lap_ms < prev_last ? LapEvent::FASTER : LapEvent::SLOWER;
    }
    g_snap.has_gap = prev_best != 0;
    g_snap.gap_ms = prev_best ? (int32_t)lap_ms - (int32_t)prev_best : 0;
    g_snap.last_event = ev;

    if (prev_best == 0 || lap_ms < prev_best) {
        g_best_ms = lap_ms;
        g_best_lap = g_lap_number;
    }
    g_lap_start_ms = t_cross_ms;
}

void update_laps(const Sample& s, float speed_ms) {
    if (!g_line_set) {
        return;
    }
    double d, c;
    along_cross(s, &d, &c);

    if (g_have_d && g_prev_d < 0 && d >= 0) {
        const double frac = -g_prev_d / (d - g_prev_d);
        const double t_cross = g_prev_time_ms + frac * (g_time_ms - g_prev_time_ms);
        const double c_cross = g_prev_c + frac * (c - g_prev_c);
        if (std::fabs(c_cross) < kLineHalfWidthM && speed_ms > kMinCrossSpeedMs &&
            t_cross - g_last_cross_ms > kMinLapMs) {
            if (g_timing) {
                complete_lap(t_cross);
            } else {
                g_timing = true;
                g_lap_start_ms = t_cross;
            }
            g_last_cross_ms = t_cross;
        }
    }
    g_prev_d = d;
    g_prev_c = c;
    g_prev_time_ms = g_time_ms;
    g_have_d = true;
}

} // namespace

void feed(const Sample& s) {
    std::lock_guard<std::mutex> lock(g_mutex);

    // Unwrap the GPS time of week.
    if (g_have_prev && s.itow_ms + 300000000u < g_last_itow) {
        g_week_offset += kWeekMs;
    }
    const double prev_time = g_time_ms;
    g_time_ms = g_week_offset + s.itow_ms;
    const float dt = g_have_prev ? (float)((g_time_ms - prev_time) / 1000.0) : 0.0f;
    g_last_itow = s.itow_ms;
    const bool dt_ok = g_have_prev && dt > 0.0f && dt < 0.5f;
    g_have_prev = true;

    const float speed_ms = std::fabs(s.speed_mm_s) * 1e-3f;
    const bool fix_ok = s.fix == 3 && (s.fix_flags & 1);

    if (dt_ok) {
        update_lean(s, dt, speed_ms);
    }
    if (fix_ok && dt_ok) {
        update_laps(s, speed_ms);
    }

    g_snap.fix_ok = fix_ok;
    g_snap.sats = s.sats;
    g_snap.charging = (s.battery & 0x80) != 0;
    g_snap.battery_pct = s.battery & 0x7F;
    g_snap.speed_kmh = speed_ms * 3.6f;
    g_snap.lean_deg = g_lean;
    g_snap.max_left_deg = g_max_left;
    g_snap.max_right_deg = g_max_right;
    g_snap.line_set = g_line_set;
    g_snap.timing = g_timing;
    g_snap.lap_number = g_lap_number + 1;
    g_snap.current_ms = g_timing ? (uint32_t)std::max(0.0, g_time_ms - g_lap_start_ms) : 0;
    g_snap.best_ms = g_best_ms;
    g_snap.best_lap = g_best_lap;
    for (int i = 0; i < 3; ++i) {
        g_snap.last_ms[i] = g_last_ms[i];
        g_snap.last_lap[i] = g_last_lap[i];
    }
    g_snap.lap_count = (uint32_t)g_lap_number;

    g_latest = s;
}

bool mark_line(Line* out) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const Sample& s = g_latest;
    const bool fix_ok = s.fix == 3 && (s.fix_flags & 1);
    if (!g_have_prev || !fix_ok || std::fabs(s.speed_mm_s) * 1e-3f < 2.0f) {
        return false;
    }
    g_line.lat_e7 = s.lat_e7;
    g_line.lon_e7 = s.lon_e7;
    g_line.heading_deg = s.heading_e5 * 1e-5f;
    g_line_set = true;
    update_line_scale();
    if (out) {
        *out = g_line;
    }
    // Rider is on the line right now: start the first lap.
    g_timing = true;
    g_lap_start_ms = g_time_ms;
    g_last_cross_ms = g_time_ms;
    g_have_d = false;
    g_lap_number = 0;
    g_best_ms = 0;
    g_best_lap = 0;
    std::fill(std::begin(g_last_ms), std::end(g_last_ms), 0u);
    std::fill(std::begin(g_last_lap), std::end(g_last_lap), 0);
    g_snap.has_gap = false;
    g_snap.last_event = LapEvent::NONE;
    return true;
}

void set_line(const Line& line) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_line = line;
    g_line_set = true;
    update_line_scale();
    g_timing = false;
    g_have_d = false;
}

void reset_session() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_timing = false;
    g_have_d = false;
    g_lap_number = 0;
    g_best_ms = 0;
    g_best_lap = 0;
    std::fill(std::begin(g_last_ms), std::end(g_last_ms), 0u);
    std::fill(std::begin(g_last_lap), std::end(g_last_lap), 0);
    g_max_left = g_max_right = 0;
    g_snap.has_gap = false;
    g_snap.last_event = LapEvent::NONE;
    g_snap.timing = false;
    g_snap.current_ms = 0;
    g_snap.best_ms = 0;
    g_snap.best_lap = 0;
    g_snap.max_left_deg = g_snap.max_right_deg = 0;
    g_snap.lap_count = 0;
    for (int i = 0; i < 3; ++i) {
        g_snap.last_ms[i] = 0;
        g_snap.last_lap[i] = 0;
    }
}

void clear_line() {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_line_set = false;
        g_snap.line_set = false;
    }
    reset_session();
}

void snapshot(Snapshot* out) {
    std::lock_guard<std::mutex> lock(g_mutex);
    *out = g_snap;
}

} // namespace telemetry
