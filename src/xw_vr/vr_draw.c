#include "xw_vr/vr_draw.h"

#include "xw_vr/vr_font.h"

#include <stddef.h>
#include <string.h>

static void blend_px(uint8_t* p, VrRgba c, int coverage) {
	/* c is straight alpha; coverage 0..255 */
	const int a   = c.a * coverage / 255;
	const int inv = 255 - a;
	p[0]          = (uint8_t)((c.r * a + p[0] * inv) / 255);
	p[1]          = (uint8_t)((c.g * a + p[1] * inv) / 255);
	p[2]          = (uint8_t)((c.b * a + p[2] * inv) / 255);
	p[3]          = (uint8_t)(a + p[3] * inv / 255);
}

void VrDraw_Clear(VrImage* img) { memset(img->px, 0, (size_t)img->w * img->h * 4); }

void VrDraw_RoundRect(VrImage* img, int x0, int y0, int x1, int y1, int r, VrRgba c) {
	for (int y = y0; y < y1; ++y) {
		if (y < 0 || y >= img->h) {
			continue;
		}
		for (int x = x0; x < x1; ++x) {
			int dx = 0, dy = 0;
			if (x < 0 || x >= img->w) {
				continue;
			}
			if (x < x0 + r) {
				dx = x0 + r - x;
			} else if (x >= x1 - r) {
				dx = x - (x1 - r - 1);
			}
			if (y < y0 + r) {
				dy = y0 + r - y;
			} else if (y >= y1 - r) {
				dy = y - (y1 - r - 1);
			}
			if (dx * dx + dy * dy > r * r) {
				continue;
			}
			blend_px(img->px + ((size_t)y * img->w + x) * 4, c, 255);
		}
	}
}

int VrDraw_TextWidth(const char* text) {
	int width = 0;
	for (; *text; ++text) {
		const unsigned char ch = (unsigned char)*text;
		if (ch >= VR_FONT_FIRST && ch <= VR_FONT_LAST) {
			width += vr_font_glyphs[ch - VR_FONT_FIRST].advance;
		}
	}
	return width;
}

void VrDraw_TextCentered(VrImage* img, const char* text, int cx, int cy, VrRgba c) {
	int pen_x    = cx - VrDraw_TextWidth(text) / 2;
	int baseline = cy + VR_FONT_ASCENT / 2 - 2;

	for (; *text; ++text) {
		const unsigned char ch = (unsigned char)*text;
		const VrGlyph*      g;
		if (ch < VR_FONT_FIRST || ch > VR_FONT_LAST) {
			continue;
		}
		g = &vr_font_glyphs[ch - VR_FONT_FIRST];
		for (int gy = 0; gy < g->h; ++gy) {
			const int y = baseline + g->yoff + gy;
			if (y < 0 || y >= img->h) {
				continue;
			}
			for (int gx = 0; gx < g->w; ++gx) {
				const int x        = pen_x + g->xoff + gx;
				const int coverage = vr_font_bits[g->offset + gy * g->w + gx];
				if (coverage && x >= 0 && x < img->w) {
					blend_px(img->px + ((size_t)y * img->w + x) * 4, c, coverage);
				}
			}
		}
		pen_x += g->advance;
	}
}

void VrDraw_SwapRedBlue(VrImage* img) {
	const size_t count = (size_t)img->w * img->h;
	for (size_t i = 0; i < count; ++i) {
		const uint8_t r    = img->px[i * 4];
		img->px[i * 4]     = img->px[i * 4 + 2];
		img->px[i * 4 + 2] = r;
	}
}
