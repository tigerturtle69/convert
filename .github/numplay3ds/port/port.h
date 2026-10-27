/* Platform interface for the NumWorks EADK shim.
 *
 * eadk_port.c implements the whole EADK API (screen, keys, timing...) once, in plain C.
 * A backend implements these few functions for one platform:
 *   port_host.c  headless PC build, scripted keys, screenshots (for testing)
 *   port_3ds.c   Nintendo 3DS (libctru)
 */
#ifndef NP_PORT_H
#define NP_PORT_H
#include <stdbool.h>
#include <stdint.h>

#define NP_W 320
#define NP_H 240

/* NumWorks key bitmask (bit n = eadk_key_t n) of everything held right now. */
uint64_t port_keys(void);
/* Milliseconds since start. */
uint32_t port_millis(void);
void port_sleep_ms(uint32_t ms);
/* The shadow frame buffer (NP_W x NP_H, RGB565, row major) changed in this rectangle: show it. */
void port_present(const uint16_t *fb, int x, int y, int w, int h);
/* Wait for the next screen refresh. Backends also service system events here (3DS Home menu). */
void port_vblank(void);

/* Which screen the game is on: top (400x240, the picture centred) or bottom (the touch screen). Only the 3DS has two. */
void port_set_screen(bool top);

/* Stylus / finger on the 320x240 game screen: true while down, with its position in game pixels. */
bool port_touch(int *x, int *y);

/* Saved games. The games keep their saves in one RAM buffer (a list of records); the backend keeps a copy on
 * whatever persists (the SD card on the 3DS). Load fills buf and returns how many bytes it got (0 if none);
 * store writes len bytes and returns whether it worked. */
uint32_t port_storage_load(uint8_t *buf, uint32_t cap);
bool port_storage_store(const uint8_t *buf, uint32_t len);
#endif
