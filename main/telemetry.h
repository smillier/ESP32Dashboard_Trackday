#pragma once

#include <cstdint>

// Lean angle estimation and GPS lap timing from the RaceBox data stream.
// Pure C++ (no ESP-IDF dependency) so it can be unit tested on a PC.
namespace telemetry {

// Decoded RaceBox Data Message (UBX 0xFF 0x01, 80 byte payload).
struct Sample {
    uint32_t itow_ms;     // ms from GPS week start
    int32_t lat_e7, lon_e7;
    int32_t speed_mm_s;
    int32_t heading_e5;   // degrees * 1e5, 0 = north, clockwise
    uint8_t fix;          // 3 = 3D fix
    uint8_t fix_flags;    // bit0 = valid fix
    uint8_t sats;
    uint8_t battery;      // bit7 charging, low 7 bits percent
    float gx, gy, gz;     // g
    float rx, ry, rz;     // deg/s (X roll, Y pitch, Z yaw)
};

// Start/finish line: a point and the direction of travel through it.
struct Line {
    int32_t lat_e7, lon_e7;
    float heading_deg;
};

enum class LapEvent : uint8_t { NONE, FIRST, BEST, FASTER, SLOWER };

struct Snapshot {
    bool fix_ok;
    uint8_t sats;
    int battery_pct;
    bool charging;
    float speed_kmh;

    float lean_deg;       // + right, - left
    float max_left_deg;   // positive numbers
    float max_right_deg;

    bool line_set;
    bool timing;          // a lap is in progress
    int lap_number;       // current lap, 1 based
    uint32_t current_ms;
    uint32_t best_ms;     // 0 = none yet
    int best_lap;
    uint32_t last_ms[3];  // newest first, 0 = none
    int last_lap[3];
    bool has_gap;
    int32_t gap_ms;       // last lap minus best lap before it
    uint32_t lap_count;   // completed laps; changes on every lap
    LapEvent last_event;
};

void feed(const Sample& s);
// Marks the start/finish line at the current position and heading and starts
// timing. Returns false (nothing changed) without a fix or while too slow.
bool mark_line(Line* out);
void set_line(const Line& line);
void reset_session();
void clear_line();   // forgets the start/finish line and resets the session
void snapshot(Snapshot* out);

} // namespace telemetry
