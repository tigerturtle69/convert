/* Celeste draws a 320x180 picture in rows 30..209 of the calculator's 320x240 screen (black bars above and below:
 * the cinematic format). The 3DS top screen is 400x240, so the picture is scaled by 5/4 to 400x225 and centred
 * there (nearest neighbour: every 4th source pixel is shown twice), and the bottom screen is free for the key map.
 *
 * Writes into a 3DS frame buffer (rotated: stored column by column, each column fw = 240 pixels, bottom first).
 * Kept in a header so a PC test can run the very same code. */
#ifndef NP_SCALE_TOP_H
#define NP_SCALE_TOP_H
#include <stddef.h>
#include <stdint.h>

#define NP_SRC_W 320
#define NP_VIEW_Y 30
#define NP_VIEW_H 180
#define NP_OUT_W 400
#define NP_OUT_H 225
#define NP_OUT_Y ((240 - NP_OUT_H) / 2)

/* fb: the 320x240 shadow screen; (x, y, w, h): the part of it that changed */
static inline void np_blit_scaled(uint16_t *fbuf, int fw, const uint16_t *fb, int x, int y, int w, int h) {
  int y0 = y < NP_VIEW_Y ? NP_VIEW_Y : y;
  int y1 = y + h > NP_VIEW_Y + NP_VIEW_H ? NP_VIEW_Y + NP_VIEW_H : y + h;
  if (y1 <= y0) return; /* only the black bars changed */
  int X0 = x * 5 / 4, X1 = ((x + w) * 5 + 3) / 4;
  if (X1 > NP_OUT_W) X1 = NP_OUT_W;
  int Y0 = NP_OUT_Y + (y0 - NP_VIEW_Y) * 5 / 4, Y1 = NP_OUT_Y + ((y1 - NP_VIEW_Y) * 5 + 3) / 4;
  if (Y1 > NP_OUT_Y + NP_OUT_H) Y1 = NP_OUT_Y + NP_OUT_H;
  for (int X = X0; X < X1; X++) {
    const uint16_t *src = fb + (X * 4 / 5);
    uint16_t *col = fbuf + (size_t)X * fw + (fw - 1);
    for (int Y = Y0; Y < Y1; Y++) col[-Y] = src[(NP_VIEW_Y + (Y - NP_OUT_Y) * 4 / 5) * NP_SRC_W];
  }
}
#endif
