#ifndef PEER_PINS_H
#define PEER_PINS_H

// Pins each Bluetooth device (RaceBox, GoPro, heart-rate strap) to one
// address so the dash never connects to a neighbour's device.
//
// Trust on first use: while a device has no pin, the first match is
// accepted and its address is saved in flash once the connection is up.
// From then on only that address is accepted. peer_pins_forget_all() clears
// the pins so the next matching device is paired again.
#include <stdbool.h>

#include "host/ble_hs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PEER_RACEBOX = 0,
    PEER_GOPRO,
    PEER_HEART,
    PEER_COUNT
} peer_kind_t;

// True if `addr` may be connected for this device kind: no pin yet, or the
// pin matches. When there is no pin the address is remembered as the
// candidate to save by peer_pins_confirm().
bool peer_pins_allowed(peer_kind_t kind, const ble_addr_t *addr);

// Call once the connection to the candidate succeeded: saves it as the pin
// if the device was not pinned yet.
void peer_pins_confirm(peer_kind_t kind);

// Clears every pin (flash and RAM).
void peer_pins_forget_all(void);

#ifdef __cplusplus
}
#endif

#endif // PEER_PINS_H
