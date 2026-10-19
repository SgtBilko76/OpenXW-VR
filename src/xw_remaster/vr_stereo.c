/* VR stereo hand-off between the Quest host (src/xw_vr) and the remaster.
 *
 * The host publishes the two eye views before each remaster frame; the
 * remaster renders the 3D flight scene into them (remaster.c,
 * XwRemaster_RenderFlightVr) and reports the angular extent of the flat
 * frame, which then only carries the cockpit/HUD overlay. */

#include "xw_remaster/vr_stereo.h"

#include <stddef.h>

static XwVrStereoFrame g_vr_frame;
static int              g_vr_frame_valid;
static int              g_vr_rendered;
static float            g_vr_tangents[4];
static int              g_vr_tangents_valid;
static int              g_vr_cockpit3d;
static float            g_vr_classic_map[4];
static float            g_vr_frame_size[2];
static char             g_vr_cockpit_view[16];

void XwRemasterVr_SetStereoFrame(const XwVrStereoFrame* frame) {
	g_vr_frame_valid = frame != NULL && frame->eyes[0].target && frame->eyes[1].target;
	if (frame)
		g_vr_frame = *frame;
}

const XwVrStereoFrame* XwRemasterVr_PublishedFrame(void) { return g_vr_frame_valid ? &g_vr_frame : NULL; }

void XwRemasterVr_SetRendered(int rendered, const float frame_tangents[4]) {
	g_vr_rendered = rendered;
	if (!rendered)
		g_vr_cockpit3d = 0;
	if (rendered && frame_tangents) {
		for (int i = 0; i < 4; ++i)
			g_vr_tangents[i] = frame_tangents[i];
		g_vr_tangents_valid = 1;
	}
}

int XwRemasterVr_StereoRendered(void) { return g_vr_rendered; }

int XwRemasterVr_HudTangents(float* tan_left, float* tan_right, float* tan_up, float* tan_down) {
	if (!g_vr_tangents_valid)
		return 0;
	*tan_left = g_vr_tangents[0];
	*tan_right = g_vr_tangents[1];
	*tan_up = g_vr_tangents[2];
	*tan_down = g_vr_tangents[3];
	return 1;
}

void XwRemasterVr_SetCockpit3d(int on, const float classic_map[4], const float frame_size[2], const char* view) {
	g_vr_cockpit3d = on && classic_map && frame_size && view;
	if (g_vr_cockpit3d) {
		size_t n = 0;
		for (; view[n] && n + 1 < sizeof g_vr_cockpit_view; ++n)
			g_vr_cockpit_view[n] = view[n];
		g_vr_cockpit_view[n] = '\0';
		for (int i = 0; i < 4; ++i)
			g_vr_classic_map[i] = classic_map[i];
		g_vr_frame_size[0] = frame_size[0];
		g_vr_frame_size[1] = frame_size[1];
	}
}

int XwRemasterVr_Cockpit3d(float classic_map[4], float frame_size[2]) {
	if (!g_vr_rendered || !g_vr_cockpit3d)
		return 0;
	for (int i = 0; i < 4; ++i)
		classic_map[i] = g_vr_classic_map[i];
	frame_size[0] = g_vr_frame_size[0];
	frame_size[1] = g_vr_frame_size[1];
	return 1;
}

const char* XwRemasterVr_Cockpit3dView(void) { return g_vr_rendered && g_vr_cockpit3d ? g_vr_cockpit_view : NULL; }
