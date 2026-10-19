#ifndef XW_REMASTER_VR_COCKPIT_H
#define XW_REMASTER_VR_COCKPIT_H

#include "xw_remaster/render_math.h"
#include "xw_runtime/snapshot/render_snapshot.h"
#include <aeron/aeron.h>
#include <aeron/scene/scene3d.h>
#include <stdbool.h>

/* VR 3D cockpits (X-Wing Revival's models, converted by
 * tools/quest/gen_xw_vr_cockpits.py into resources/vr_cockpits/<craft>.glb).
 *
 * Returns the cockpit for the player's craft in this snapshot (0 X-Wing,
 * 1 Y-Wing, 2 A-Wing, 3 B-Wing), or -1 for external/replay views, other
 * craft, or when the model is missing. */
int XwVrCockpit_Select(const XwRenderSnapshot* s);
/* Adds the cockpit, fixed to the player's ship with the eye at `body`'s
 * camera (the game camera before any VR eye offset), to the scene. Loads and
 * uploads the model on first use. */
bool XwVrCockpit_Add(AeronCommandBuffer* cmd, AeronScene3D* scene, int cockpit, const XwRenderSnapshot* s,
					 const XwRenderView* body);
void XwVrCockpit_Shutdown(void);

#endif
