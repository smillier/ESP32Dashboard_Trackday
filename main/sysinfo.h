#pragma once

#include <stdint.h>

// Snapshot of the chip's resource usage, for the settings screen.
// CPU load is measured between two calls (so call it about once a second);
// the first call after a long pause averages over that whole pause.
struct SysInfo {
    uint8_t cores;            // 1 or 2
    uint8_t cpu_pct;          // load averaged over the cores, 0..100
    uint8_t core_pct[2];      // load per core
    uint32_t ram_free;        // internal RAM, bytes
    uint32_t ram_total;
    uint32_t ram_min_free;    // lowest free value since boot
    uint32_t psram_free;      // external RAM, bytes (total is 0 when there is none)
    uint32_t psram_total;
    uint32_t uptime_s;
};

void sysinfo_sample(SysInfo* out);
