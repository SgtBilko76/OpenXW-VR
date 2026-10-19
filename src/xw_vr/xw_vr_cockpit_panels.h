/* VR cockpit instrument panels per craft view (none yet for OpenXW; the
 * shared VR host only places panels for views listed here). */
#ifndef XW_VR_COCKPIT_PANELS_H
#define XW_VR_COCKPIT_PANELS_H

#include <stddef.h>
#include <string.h>

typedef struct XwVrCockpitPanel {
	const char* name;
	float src[4];         /* x0, y0, x1, y1 in classic 640x480 pixels */
	float position[3];    /* metres, x right, y up, -z forward */
	float orientation[4]; /* x, y, z, w */
	float size[2];        /* width, height in metres */
} XwVrCockpitPanel;

typedef struct XwVrCockpitView {
	const char*             view;
	const char*             xwa_model;
	int                     has_builtin;
	float                   hud_rect[4];
	const XwVrCockpitPanel* panels;
	size_t                  panel_count;
} XwVrCockpitView;

static inline const XwVrCockpitView* XwVrCockpit_FindView(const char* view_name) {
	(void)view_name;
	return NULL;
}

#endif
