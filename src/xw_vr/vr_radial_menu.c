#include "xw_vr/vr_radial_menu.h"

#include "xw_runtime/input/actions.h"
#include "xw_vr/vr_draw.h"

#include <math.h>

/* Eight commands per hand, index 0 = up, then clockwise. Positions follow
 * OpenXWA-VR where the two games share a command (recharge rates up, shield
 * direction up-left, wingmen at the bottom); X-Wing's own commands (S-foils,
 * target in sights, rear view, previous target) take the places of TIE
 * Fighter's beam weapon, components, mission goals and target's attacker. */
static const VrCommand k_items[2][VR_RADIAL_ITEMS] = {
	/* Left: weapons, energy and throttle */
	{
		{ "Laser rate", XW_INPUT_ACTION_CYCLE_CANNON_RECHARGE_RATE },
		{ "Shield rate", XW_INPUT_ACTION_CYCLE_SHIELD_RECHARGE_RATE },
		{ "Cycle weapon", XW_INPUT_ACTION_CYCLE_WEAPON_GROUP },
		{ "Full throttle", XW_INPUT_ACTION_THROTTLE_FULL },
		{ "Throttle down", XW_INPUT_ACTION_THROTTLE_DOWN },
		{ "S-foils", XW_INPUT_ACTION_TOGGLE_S_FOILS },
		{ "Firing mode", XW_INPUT_ACTION_CYCLE_WEAPON_FIRING_MODE },
		{ "Shield balance", XW_INPUT_ACTION_CYCLE_SHIELD_MODE },
	},
	/* Right: targeting, wingmen, rear view and the flight menu. The rear view
	 * stays until the left stick click (forward view). */
	{
		{ "Clear target", XW_INPUT_ACTION_TARGET_CLEAR },
		{ "In sights", XW_INPUT_ACTION_AUTO_TARGET },
		{ "Map", XW_INPUT_ACTION_INFO_MAP },
		{ "Attack target", XW_INPUT_ACTION_COMM_ASSIGN_TARGET },
		{ "Cover me", XW_INPUT_ACTION_COMM_COVER_ME },
		{ "Rear view", XW_INPUT_ACTION_VIEW_REAR },
		{ "Flight menu", XW_INPUT_ACTION_ESCAPE },
		{ "Prev target", XW_INPUT_ACTION_TARGET_PREV },
	},
};

static const char* const k_titles[2][2] = { { "WEAPONS", "POWER" }, { "TARGETS", "ORDERS" } };

enum {
	CENTER_PX   = VR_RADIAL_SIZE_PX / 2,
	RING_PX     = 330, /* centre of the item boxes */
	BOX_W_PX    = 320,
	BOX_H_PX    = 84,
	BOX_R_PX    = 18,
	HUB_PX      = 128, /* radius of the centre disc */
	SPOKE_W_PX  = 6,
};

const VrCommand* VrRadialMenu_Item(VrRadialSide side, int item) {
	if ((side != VR_RADIAL_LEFT && side != VR_RADIAL_RIGHT) || item < 0 || item >= VR_RADIAL_ITEMS) {
		return 0;
	}
	return &k_items[side][item];
}

int VrRadialMenu_ItemForDirection(float x, float y) {
	/* Angle clockwise from up, in eighths, rounded to the nearest item. */
	const float angle = atan2f(x, y); /* 0 = up, +pi/2 = right */
	int         item  = (int)lroundf(angle / (2.0f * 3.14159265f / VR_RADIAL_ITEMS));

	return (item % VR_RADIAL_ITEMS + VR_RADIAL_ITEMS) % VR_RADIAL_ITEMS;
}

static void item_centre(int item, int* x, int* y) {
	const float angle = (float)item * 2.0f * 3.14159265f / VR_RADIAL_ITEMS;
	*x                = CENTER_PX + (int)lroundf(sinf(angle) * RING_PX);
	*y                = CENTER_PX - (int)lroundf(cosf(angle) * RING_PX);
}

static void fill_disc(VrImage* img, int cx, int cy, int r, VrRgba color) {
	VrDraw_RoundRect(img, cx - r, cy - r, cx + r, cy + r, r, color);
}

/* A spoke from the hub towards an item, drawn as a row of small discs. */
static void draw_spoke(VrImage* img, int item, VrRgba color) {
	const float angle = (float)item * 2.0f * 3.14159265f / VR_RADIAL_ITEMS;
	for (int d = HUB_PX; d < RING_PX - BOX_H_PX / 2; d += 2) {
		fill_disc(img, CENTER_PX + (int)lroundf(sinf(angle) * d), CENTER_PX - (int)lroundf(cosf(angle) * d),
				  SPOKE_W_PX / 2, color);
	}
}

void VrRadialMenu_Render(VrRadialSide side, int selected, uint8_t* rgba, int bgra) {
	static const VrRgba backdrop   = { 8, 12, 20, 150 };
	static const VrRgba hub        = { 24, 30, 42, 235 };
	static const VrRgba spoke      = { 70, 80, 100, 200 };
	static const VrRgba spoke_sel  = { 230, 160, 50, 255 };
	static const VrRgba box        = { 44, 50, 64, 240 };
	static const VrRgba box_sel    = { 230, 160, 50, 255 };
	static const VrRgba label      = { 240, 240, 245, 255 };
	static const VrRgba label_dark = { 20, 20, 25, 255 };
	static const VrRgba title      = { 230, 160, 50, 255 };
	VrImage             img        = { rgba, VR_RADIAL_SIZE_PX, VR_RADIAL_SIZE_PX };

	VrDraw_Clear(&img);
	fill_disc(&img, CENTER_PX, CENTER_PX, CENTER_PX - 4, backdrop);
	for (int item = 0; item < VR_RADIAL_ITEMS; ++item) {
		draw_spoke(&img, item, item == selected ? spoke_sel : spoke);
	}
	fill_disc(&img, CENTER_PX, CENTER_PX, HUB_PX, hub);
	VrDraw_TextCentered(&img, k_titles[side][0], CENTER_PX, CENTER_PX - 24, title);
	VrDraw_TextCentered(&img, k_titles[side][1], CENTER_PX, CENTER_PX + 24, title);
	for (int item = 0; item < VR_RADIAL_ITEMS; ++item) {
		const int sel = item == selected;
		int       x, y;
		item_centre(item, &x, &y);
		VrDraw_RoundRect(&img, x - BOX_W_PX / 2, y - BOX_H_PX / 2, x + BOX_W_PX / 2, y + BOX_H_PX / 2, BOX_R_PX,
						 sel ? box_sel : box);
		VrDraw_TextCentered(&img, k_items[side][item].label, x, y, sel ? label_dark : label);
	}
	if (bgra) {
		VrDraw_SwapRedBlue(&img);
	}
}
