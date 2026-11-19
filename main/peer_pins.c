#include "peer_pins.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "peer_pins";
static const char *NS = "dash";
static const char *KEYS[PEER_COUNT] = {"pin_rb", "pin_gp", "pin_hr"};
static const char *NAMES[PEER_COUNT] = {"RaceBox", "GoPro", "Heart-rate strap"};

typedef struct {
    bool loaded;
    bool pinned;
    ble_addr_t pin;
    bool has_candidate;
    ble_addr_t candidate;
} slot_t;

static slot_t s_slot[PEER_COUNT];

static void load_slot(peer_kind_t kind) {
    slot_t *s = &s_slot[kind];
    if (s->loaded) {
        return;
    }
    s->loaded = true;
    s->pinned = false;
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    size_t len = sizeof(s->pin);
    s->pinned = nvs_get_blob(h, KEYS[kind], &s->pin, &len) == ESP_OK && len == sizeof(s->pin);
    nvs_close(h);
}

bool peer_pins_allowed(peer_kind_t kind, const ble_addr_t *addr) {
    if (kind >= PEER_COUNT || addr == NULL) {
        return false;
    }
    load_slot(kind);
    slot_t *s = &s_slot[kind];
    if (s->pinned) {
        return s->pin.type == addr->type && memcmp(s->pin.val, addr->val, sizeof(addr->val)) == 0;
    }
    s->candidate = *addr;
    s->has_candidate = true;
    return true;
}

void peer_pins_confirm(peer_kind_t kind) {
    if (kind >= PEER_COUNT) {
        return;
    }
    load_slot(kind);
    slot_t *s = &s_slot[kind];
    if (s->pinned || !s->has_candidate) {
        return;
    }
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    if (nvs_set_blob(h, KEYS[kind], &s->candidate, sizeof(s->candidate)) == ESP_OK && nvs_commit(h) == ESP_OK) {
        s->pin = s->candidate;
        s->pinned = true;
        const uint8_t *v = s->pin.val;
        ESP_LOGI(TAG, "%s pinned to %02x:%02x:%02x:%02x:%02x:%02x", NAMES[kind], v[5], v[4], v[3], v[2], v[1], v[0]);
    }
    nvs_close(h);
}

void peer_pins_forget_all(void) {
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) {
        for (int i = 0; i < PEER_COUNT; i++) {
            nvs_erase_key(h, KEYS[i]);
        }
        nvs_commit(h);
        nvs_close(h);
    }
    for (int i = 0; i < PEER_COUNT; i++) {
        s_slot[i].loaded = true;
        s_slot[i].pinned = false;
        s_slot[i].has_candidate = false;
    }
    ESP_LOGW(TAG, "All pinned devices forgotten");
}
