#ifndef XW_REMASTER_VR_STEREO_H
#define XW_REMASTER_VR_STEREO_H

/* Stereo flight rendering for VR presentation.
 *
 * The VR host publishes the two eye views of the current frame before the
 * remaster renders. While the HD flight scene is active the remaster renders
 * the 3D world once per eye (head-tracked, real parallax) into the host's eye
 * textures, and renders the HUD alone on a transparent background into the
 * normal presentation frame. Every non-flight scene stays on the flat path. */

#include <SDL3/SDL_gpu.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwVrStereoEye {
	/* Eye pose relative to the seated reference (OpenXR LOCAL space at
	 * recenter): x right, y up, -z forward. Metres; quaternion x,y,z,w. */
	float           position_m[3];
	float           orientation[4];
	/* Positive tangents of the four frustum half-angles. */
	float           tan_left, tan_right, tan_up, tan_down;
	SDL_GPUTexture* target;
	int             target_width;
	int             target_height;
} XwVrStereoEye;

typedef struct XwVrStereoFrame {
	XwVrStereoEye eyes[2];
} XwVrStereoFrame;

/* Publishes this frame's eye views (NULL disables stereo). The struct is
 * copied; targets must stay valid until the frame's GPU work is submitted. */
void XwRemasterVr_SetStereoFrame(const XwVrStereoFrame* frame);

/* 1 when the last remaster frame wrote both eye targets; the flat
 * presentation frame then holds only the transparent HUD overlay. */
int XwRemasterVr_StereoRendered(void);

/* Positive tangents of the HUD frame's edges as seen from the seat, so a
 * host can place the HUD overlay to line up with the 3D world. Returns 0
 * while unknown. */
int XwRemasterVr_HudTangents(float* tan_left, float* tan_right, float* tan_up, float* tan_down);

/* 3D cockpit this frame: the flat frame then holds only instruments/HUD on a
 * transparent background. `classic_map` maps classic 640x480 cockpit pixels
 * to flat-frame pixels: frame = (map[0] + x * map[2], map[1] + y * map[3]);
 * frame_size is the flat frame's width/height. Returns 0 when off. */
int XwRemasterVr_Cockpit3d(float classic_map[4], float frame_size[2]);
/* Cockpit view name (CP640 LFD basename) of the 3D cockpit, or NULL. */
const char* XwRemasterVr_Cockpit3dView(void);

/* Remaster-internal: the frame published for this host frame (NULL when
 * none), and the result of the stereo render (tangents of the flat frame's
 * edges as seen from the seat; NULL when not rendered). */
const XwVrStereoFrame* XwRemasterVr_PublishedFrame(void);
void                    XwRemasterVr_SetRendered(int rendered, const float frame_tangents[4]);
void XwRemasterVr_SetCockpit3d(int on, const float classic_map[4], const float frame_size[2], const char* view);

#ifdef __cplusplus
}
#endif

#endif
