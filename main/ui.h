#pragma once

// Lap-timing / lean-angle dashboard (480x320 landscape). Builds the LVGL
// widgets and refreshes them from telemetry and the BLE modules. Call from
// the LVGL task only.
void ui_create(void);

// Set when simulated telemetry is driving the screen instead of a RaceBox.
extern bool ui_demo_active;
