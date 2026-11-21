#include "sysinfo.h"

#include <cstdlib>
#include <cstring>

#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

namespace {

constexpr int kMaxCores = 2;

// Idle-task run time and total run time at the previous call, per core.
uint32_t g_prev_idle[kMaxCores] = {};
uint32_t g_prev_total = 0;
uint8_t g_last_core_pct[kMaxCores] = {};

// Updates out->core_pct from the idle tasks' share of the run time since the
// previous call. Keeps the old values if the task list can't be read.
void sample_cpu(SysInfo* out) {
    const UBaseType_t cap = uxTaskGetNumberOfTasks() + 4;
    TaskStatus_t* tasks = static_cast<TaskStatus_t*>(malloc(cap * sizeof(TaskStatus_t)));
    if (tasks == nullptr) {
        return;
    }
    uint32_t total = 0;
    const UBaseType_t n = uxTaskGetSystemState(tasks, cap, &total);
    uint32_t idle[kMaxCores] = {};
    for (UBaseType_t i = 0; i < n; ++i) {
        // The idle tasks are called IDLE0 and IDLE1.
        if (strncmp(tasks[i].pcTaskName, "IDLE", 4) == 0) {
            const int core = tasks[i].pcTaskName[4] == '1' ? 1 : 0;
            idle[core] = tasks[i].ulRunTimeCounter;
        }
    }
    free(tasks);

    const uint32_t d_total = total - g_prev_total;  // unsigned: survives a wrap
    // The run time counter ticks in microseconds. A very short interval gives
    // noisy percentages, so keep the previous result and wait for more time.
    if (d_total >= 200000) {
        for (int c = 0; c < out->cores; ++c) {
            uint32_t d_idle = idle[c] - g_prev_idle[c];
            if (d_idle > d_total) {
                d_idle = d_total;
            }
            g_last_core_pct[c] = (uint8_t)(100 - (uint64_t)d_idle * 100 / d_total);
        }
        for (int c = 0; c < kMaxCores; ++c) {
            g_prev_idle[c] = idle[c];
        }
        g_prev_total = total;
    }
}

} // namespace

void sysinfo_sample(SysInfo* out) {
    memset(out, 0, sizeof(*out));
    out->cores = portNUM_PROCESSORS > kMaxCores ? kMaxCores : portNUM_PROCESSORS;

    sample_cpu(out);
    unsigned sum = 0;
    for (int c = 0; c < out->cores; ++c) {
        out->core_pct[c] = g_last_core_pct[c];
        sum += g_last_core_pct[c];
    }
    out->cpu_pct = (uint8_t)(sum / out->cores);

    out->ram_free = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    out->ram_total = (uint32_t)heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    out->ram_min_free = (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    out->psram_total = (uint32_t)heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    out->psram_free = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    out->uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
}
