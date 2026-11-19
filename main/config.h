#pragma once

#include "sdkconfig.h"

#include <cstdint>

// ---------------------------------------------------------------------
// RaceBox (standalone recording bridge)
// ---------------------------------------------------------------------

// BLE advertised name prefix for the target device. RaceBox devices always
// advertise as "RaceBox Mini <serial>", "RaceBox Mini S <serial>" or
// "RaceBox Micro <serial>". Only Mini S and Micro support standalone
// recording, so this project targets Micro by default.
inline constexpr const char* kRaceBoxNamePrefix = "RaceBox Micro";

// Nordic-UART-style service used by RaceBox for its binary UBX protocol.
// (Kept here for reference; racebox_ble.cpp encodes these as NimBLE
// ble_uuid128_t constants since that's the form the NimBLE C API needs.)
inline constexpr const char* kUartServiceUuid = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
inline constexpr const char* kUartRxCharUuid = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"; // write commands here
inline constexpr const char* kUartTxCharUuid = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"; // subscribe for notifications

// Memory security code (see "Unlock Memory", message 0xFF 0x30, in the RaceBox
// BLE protocol doc). Only relevant if you enabled memory security on the
// device from the official RaceBox app. Leave at 0 if you never set one -
// unlocking is a harmless no-op on devices without security enabled.
inline constexpr uint32_t kMemoryUnlockCode = 0;

// Standalone recording configuration sent when the button starts a recording
// (message 0xFF 0x25). Values follow the "Recommended setup" in the protocol
// doc: all filters enabled, 5 kph / 30 s stationary cutoff, 30 s no-fix
// cutoff, 5 minute auto-shutdown.
inline constexpr uint8_t kRecordingDataRate = 0; // 0 = 25 Hz
inline constexpr uint8_t kRecordingFlags = 0x1F; // wait-for-fix, stationary, no-fix, auto-shutdown, wait-for-data
inline constexpr uint16_t kStationarySpeedThresholdMmS = 1389; // ~5 kph
inline constexpr uint16_t kStationaryIntervalSec = 30;
inline constexpr uint16_t kNoFixIntervalSec = 30;
inline constexpr uint16_t kAutoShutdownIntervalSec = 300;

// ---------------------------------------------------------------------
// External buttons (each between the GPIO and GND). GPIO34-39 have no
// internal pull-up: add a 10k to 3V3 if you use one of them (e.g. 35).
// ---------------------------------------------------------------------
#if CONFIG_IDF_TARGET_ESP32S3
// Waveshare ESP32-S3-Touch-LCD-3.5: free pins on the 2x16 header.
inline constexpr int kBtnRaceBoxRec = 38; // RaceBox standalone recording toggle
inline constexpr int kBtnGoProShutter = 39;
inline constexpr int kBtnGoProMode = 40;
inline constexpr int kBtnGoProPower = 41;
inline constexpr int kBtnMarkLine = 42;   // short: mark start/finish here, long: reset laps
#else
inline constexpr int kBtnRaceBoxRec = 16; // RaceBox standalone recording toggle
inline constexpr int kBtnGoProShutter = 22;
inline constexpr int kBtnGoProMode = 21;
inline constexpr int kBtnGoProPower = 35; // needs an external pull-up
inline constexpr int kBtnMarkLine = 17;   // short: mark start/finish here, long: reset laps
#endif

inline constexpr int kLongPressMs = 1500;

// Holding the GoPro mode button this long forgets every pinned Bluetooth
// device (RaceBox, GoPro, heart-rate strap) so the next ones are pinned.
inline constexpr int kForgetPinsMs = 3000;

// ---------------------------------------------------------------------
// NeoPixel status bar: 0 = RaceBox, 1 = GoPro, 2 = heart rate monitor,
// 3.. = lap result flash (green faster, red slower, purple new best).
// ---------------------------------------------------------------------
#if CONFIG_IDF_TARGET_ESP32S3
inline constexpr int kRgbLedPin = 21;
#else
inline constexpr int kRgbLedPin = 4;
#endif
inline constexpr int kRgbLedCount = 10;
inline constexpr int kRaceBoxLedIndex = 0;
inline constexpr int kGoProLedIndex = 1;
inline constexpr int kHeartRateLedIndex = 2;

// ---------------------------------------------------------------------
// Telemetry
// ---------------------------------------------------------------------

// Replace the RaceBox with a simulated bike lapping a circuit (UI test).
inline constexpr bool kDemoTelemetry = true;

// Lean angle sign conventions: RaceBox reports gyro X = roll, Z = yaw with
// Z pointing up when mounted flat. Flip a sign if left/right is swapped.
inline constexpr float kRollRateSign = 1.0f;
inline constexpr float kYawRateSign = -1.0f;
inline constexpr float kAccelRollSign = 1.0f;

inline constexpr float kLineHalfWidthM = 30.0f;      // start/finish line half length
inline constexpr uint32_t kMinLapMs = 15000;         // ignore crossings closer than this
inline constexpr float kMinCrossSpeedMs = 5.0f;      // ~18 km/h
