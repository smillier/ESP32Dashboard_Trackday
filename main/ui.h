#pragma once

// Lap-timing / lean-angle dashboard (480x320 landscape). Builds the LVGL
// widgets and refreshes them from telemetry and the BLE modules. Call from
// the LVGL task only.
void ui_create(void);

// Shows the dashboard again (after the settings screen). `rebuild` recreates
// every widget, needed after a language change.
void ui_show_dashboard(bool rebuild);

// Set when simulated telemetry is driving the screen instead of a RaceBox.
extern bool ui_demo_active;
extern int ui_demo_bpm;  // simulated heart rate shown in demo mode (0 = none)
