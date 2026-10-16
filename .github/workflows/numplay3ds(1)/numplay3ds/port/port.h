/* Platform interface for the NumWorks EADK shim.
 *
 * eadk_port.c implements the whole EADK API (screen, keys, timing...) once, in plain C.
 * A backend implements these few functions for one platform:
 *   port_host.c  headless PC build, scripted keys, screenshots (for testing)
 *   port_3ds.c   Nintendo 3DS (libctru)
 */
#ifndef NP_PORT_H
#define NP_PORT_H
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
#endif
