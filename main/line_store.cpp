#include "line_store.h"

#include "nvs.h"

namespace {
constexpr char kNamespace[] = "dash";
constexpr char kKey[] = "line";
} // namespace

bool line_store::load(telemetry::Line* line) {
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    size_t len = sizeof(*line);
    const bool ok = nvs_get_blob(h, kKey, line, &len) == ESP_OK && len == sizeof(*line);
    nvs_close(h);
    return ok;
}

void line_store::save(const telemetry::Line& line) {
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    nvs_set_blob(h, kKey, &line, sizeof(line));
    nvs_commit(h);
    nvs_close(h);
}
