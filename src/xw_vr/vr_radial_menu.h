#ifndef XW_VR_RADIAL_MENU_H
#define XW_VR_RADIAL_MENU_H

/* In-flight radial ("star") menus for the Quest build. Holding a grip shows
 * that hand's menu above its controller; tilting the same hand's stick picks
 * one of eight commands and releasing the grip runs it. Left hand: weapons,
 * energy and throttle. Right hand: targeting, wingmen, views and the flight
 * menu. Each entry is one of the game's input actions
 * (xw_runtime/input/actions.h). This file holds the commands, the direction
 * mapping and the drawing; xw_vr.c shows the panels and dispatches the
 * actions. Same layout logic as OpenXWA-VR's vr_radial_menu.c. */

#include <stdint.h>

enum {
	VR_RADIAL_SIZE_PX = 1024, /* square image */
	VR_RADIAL_ITEMS   = 8,
	VR_RADIAL_NONE    = -1,
};

typedef enum VrRadialSide { VR_RADIAL_LEFT = 0, VR_RADIAL_RIGHT = 1 } VrRadialSide;

typedef struct VrCommand {
	const char* label;
	int         action; /* XwInputAction */
} VrCommand;

/* Command `item` (0 = up, then clockwise) of a side's menu. */
const VrCommand* VrRadialMenu_Item(VrRadialSide side, int item);

/* Item for a stick direction (x right, y up, magnitude ignored). */
int VrRadialMenu_ItemForDirection(float x, float y);

/* Draws a side's menu with `selected` highlighted (or VR_RADIAL_NONE) into
 * `rgba` (VR_RADIAL_SIZE_PX squared, 4 bytes per pixel, premultiplied alpha).
 * `bgra` swaps red and blue. */
void VrRadialMenu_Render(VrRadialSide side, int selected, uint8_t* rgba, int bgra);

#endif
