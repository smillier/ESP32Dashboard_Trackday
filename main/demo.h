#pragma once

// Feeds telemetry with a simulated bike lapping a circular track so the UI
// can be exercised without a RaceBox. The initial state is kDemoTelemetry in
// config.h; the settings screen can change it.
// Turns the simulation on or off at runtime (settings screen). Call from the
// LVGL task.
void demo_set_enabled(bool on);
