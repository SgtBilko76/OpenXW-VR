#ifndef XW_VR_DRAW_H
#define XW_VR_DRAW_H

/* Tiny CPU rasterizer for the VR panels (virtual keyboard, command menu):
 * rounded rectangles and the baked UI font into a premultiplied RGBA8 image. */

#include <stdint.h>

typedef struct VrImage {
	uint8_t* px; /* w * h * 4 bytes, premultiplied alpha */
	int      w, h;
} VrImage;

typedef struct VrRgba {
	uint8_t r, g, b, a; /* straight alpha */
} VrRgba;

void VrDraw_Clear(VrImage* img);
void VrDraw_RoundRect(VrImage* img, int x0, int y0, int x1, int y1, int radius, VrRgba color);
int  VrDraw_TextWidth(const char* text);
/* Draws one line of text centred on (cx, cy). */
void VrDraw_TextCentered(VrImage* img, const char* text, int cx, int cy, VrRgba color);
/* RGBA -> BGRA for swapchains that want blue first. */
void VrDraw_SwapRedBlue(VrImage* img);

#endif
