/* VR 3D cockpits (see vr_cockpit.h). */
#include "xw_remaster/vr_cockpit.h"

#include <aeron/asset/flight_model.h>
#include <aeron/asset/opt_model.h>
#include <aeron/log.h>
#include <aeron/scene/mesh.h>
#include <aeron/vfs.h>
#include <stdlib.h>
#include <string.h>

enum { COCKPIT_COUNT = 4 };

typedef struct Cockpit {
	AeronFlightModel* model;
	AeronSceneMesh* mesh;
	bool failed;
} Cockpit;

static const char* const paths[COCKPIT_COUNT] = { "vr_cockpits/xwing.glb", "vr_cockpits/ywing.glb",
												  "vr_cockpits/awing.glb", "vr_cockpits/bwing.glb" };
static Cockpit cockpits[COCKPIT_COUNT];

int XwVrCockpit_Select(const XwRenderSnapshot* s) {
	if (!s || s->flight_version != 98 || s->camera.external || s->camera.replay_mode)
		return -1;
	for (unsigned i = 0; i < s->object_count; ++i) {
		const XwSnapObject* object = &s->objects[i];
		if (!XwRenderMath_SameObject(object->id, s->camera.player))
			continue;
		/* Object types 1-4: X-Wing, Y-Wing, A-Wing, B-Wing. */
		int cockpit = object->type >= 1 && object->type <= COCKPIT_COUNT ? object->type - 1 : -1;
		return cockpit >= 0 && !cockpits[cockpit].failed ? cockpit : -1;
	}
	return -1;
}

static bool Ensure(AeronCommandBuffer* cmd, int index) {
	Cockpit* c = &cockpits[index];
	if (c->mesh)
		return true;
	if (c->failed)
		return false;
	c->failed = true;
	uint8_t* data = NULL;
	size_t size = 0;
	if (!AeronVfs_ReadAll(Aeron_GetVfs(), AERON_VFS_ROOT_RESOURCE, paths[index], 0, &data, &size)) {
		Aeron_LogWarn("xw.vr", "3D cockpit %s not found", paths[index]);
		return false;
	}
	c->model = calloc(1, sizeof *c->model);
	bool built = c->model && Aeron_FlightModelBuildMemory(data, size, paths[index], c->model);
	free(data);
	if (!built) {
		Aeron_LogWarn("xw.vr", "3D cockpit %s failed to load", paths[index]);
		free(c->model);
		c->model = NULL;
		return false;
	}
	AeronSceneMeshCreateStatus status = AERON_SCENE_MESH_CREATE_SUCCESS;
	c->mesh = AeronScene_MeshCreate(cmd, c->model, paths[index], &status);
	if (!c->mesh) {
		Aeron_LogWarn("xw.vr", "3D cockpit %s mesh creation failed (%d)", paths[index], (int)status);
		return false;
	}
	c->failed = false;
	Aeron_LogInfo("xw.vr", "3D cockpit %s ready: %u triangles", paths[index], c->mesh->index_count / 3u);
	return true;
}

static const XwSnapObject* Player(const XwRenderSnapshot* s) {
	for (unsigned i = 0; s && i < s->object_count; ++i)
		if (XwRenderMath_SameObject(s->objects[i].id, s->camera.player))
			return &s->objects[i];
	return NULL;
}

bool XwVrCockpit_Add(AeronCommandBuffer* cmd, AeronScene3D* scene, int index, const XwRenderSnapshot* s,
					 const XwRenderView* body) {
	if (index < 0 || index >= COCKPIT_COUNT || !Ensure(cmd, index))
		return false;
	/* Model axes are the body eye axes (x right, y down, z forward) in metres
	 * with the pilot's eye at the origin. The cockpit is fixed to the ship,
	 * not to the game camera: the look views (12:00 HIGH, rear, shoulder)
	 * turn the camera inside the cockpit. Looking more than 60 degrees away
	 * from the nose (rear/shoulder/wing views, right grip on Quest) the seat
	 * and canopy frame would fill the view, so the cockpit is left out, as
	 * the game leaves out its 2D cockpit there. */
	float rows[9];
	XwRenderMath_ViewRows(&body->camera, rows);
	float axes[9]; /* column c = eye axis c in scene space */
	for (int r = 0; r < 3; ++r)
		for (int c = 0; c < 3; ++c)
			axes[r * 3 + c] = rows[c * 3 + r];
	const XwSnapObject* player = Player(s);
	if (player) {
		/* Ship model axes (OPT: x right, y back, z up) -> eye axes:
		 * right = x, down = -z, forward = -y. */
		float ship[16];
		XwRenderMath_ObjectMatrix(player, s->flight_version, body->origin_world, ship);
		const float scale = s->flight_version == 98 ? AERON_OPT_UNITS_PER_METER : 1.0f;
		for (int r = 0; r < 3; ++r) {
			axes[r * 3 + 0] = ship[r * 4 + 0] / scale;
			axes[r * 3 + 1] = -ship[r * 4 + 2] / scale;
			axes[r * 3 + 2] = -ship[r * 4 + 1] / scale;
		}
		const float facing = rows[6] * axes[2] + rows[7] * axes[5] + rows[8] * axes[8];
		if (facing < 0.5f)
			return false;
	}
	AeronSceneMeshInstance in;
	memset(&in, 0, sizeof in);
	in.mesh = cockpits[index].mesh;
	const float m = AERON_OPT_UNITS_PER_METER;
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c)
			in.transform[r * 4 + c] = axes[r * 3 + c] * m;
		in.transform[r * 4 + 3] = body->camera.pos[r];
	}
	in.transform[15] = 1.0f;
	memcpy(in.prev_transform, in.transform, sizeof in.prev_transform);
	in.zero_velocity = 1;
	in.cull_mode = AERON_CULL_NONE;
	in.shadow_flags = AERON_SCENE_INSTANCE_NO_CAST_SHADOW | AERON_SCENE_INSTANCE_NO_RECEIVE_SHADOW;
	AeronScene_AddMeshInstance(scene, &in);
	return true;
}

void XwVrCockpit_Shutdown(void) {
	for (int i = 0; i < COCKPIT_COUNT; ++i) {
		if (cockpits[i].mesh)
			AeronScene_MeshDestroy(cockpits[i].mesh);
		if (cockpits[i].model) {
			Aeron_FlightModelFree(cockpits[i].model);
			free(cockpits[i].model);
		}
	}
	memset(cockpits, 0, sizeof cockpits);
}
