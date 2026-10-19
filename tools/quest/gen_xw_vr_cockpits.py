#!/usr/bin/env python3
"""Convert X-Wing Revival's 3D cockpits into Aeron flight models for OpenXW VR.

X-Wing Revival (free, https://www.raptor007.com/xwing/) ships textured OBJ
cockpits for the four Rebel starfighters, made by the XWA Upgrade team. This
script turns each one into a glTF, cooks it with aeron_gltf_cook (RGBA8
atlases, the Quest has no BC textures) and writes

  <out_dir>/xwing.glb  ywing.glb  awing.glb  bwing.glb

which build-native.sh stages as APK assets (resources/vr_cockpits/). The
models are not committed to this repository.

Axes: Revival's OBJ space is x right, y up, z forward, 0.022 m per unit
(model_scale in Data/Ships/*.def); the pilot's eye sits at the def file's
`cockpit <obj> <right> <up> <fwd>` offset in metres. Output vertices are in
metres with the eye at the origin, converted so that Aeron's glTF import
((-gx, -gz, gy)) yields the game's eye axes (x right, y down, z forward).

Usage: gen_xw_vr_cockpits.py <aeron_gltf_cook> <revival gamedata dir> <out dir> [craft ...]

A craft is a name from CRAFTS, or `name=<obj file>:<right>,<up>,<fwd>` for
any other Revival cockpit (eye offset in metres from its .def file; use the
`cockpit_vr` line when there is one). This file is shared by the OpenXW,
OpenTIE and OpenXWA Quest ports.
"""
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

from PIL import Image, ImageChops

MODEL_SCALE = 0.022
# craft id -> (obj file, eye right/up/fwd in metres from Data/Ships/*.def)
CRAFTS = {
    "xwing": ("x-wing_cockpit_hq.obj", (0.0, 0.814, -0.704)),
    "ywing": ("y-wing_cockpit_hq.obj", (0.0, 0.836, 4.18)),
    "awing": ("a-wing_cockpit.obj", (0.0, 0.56, -0.4)),
    "bwing": ("b-wing_cockpit.obj", (0.0, 4.55, 1.15)),
    "tiefighter": ("tie-fighter_cockpit_vr.obj", (0.0, 0.1, -0.1)),
    "tieinterceptor": ("tie-interceptor_cockpit_vr.obj", (0.0, 0.1, -0.1), "tie-interceptor_cockpit"),
    "tiebomber": ("tie-bomber_cockpit_vr.obj", (1.474, 0.1, 1.7152)),
    "gunboat": ("gunboat_cockpit.obj", (0.0, 0.36, 0.98)),
    "falcon": ("yt-1300_cockpit.obj", (10.0, 1.25, 7.64)),
}
DEFAULT_CRAFTS = ["xwing", "ywing", "awing", "bwing"]
# Lit like the game's ships: Revival's own diffuse (Kd, mostly 0.4) and rough
# surfaces; brighter albedo made the white side consoles glow.
BASE_COLOR_MAX = 0.45
# Self-lit parts (lamps, screens, labels) at a moderate glow.
EMISSIVE = 0.35
ROUGHNESS = 0.8


def parse_mtl(path):
    mats, cur = {}, None
    for line in open(path, encoding="latin-1"):
        parts = line.split("#", 1)[0].split()
        if not parts:
            continue
        key, args = parts[0], parts[1:]
        if key == "newmtl":
            cur = mats.setdefault(" ".join(args), {"d": 1.0, "Ke": (0.0, 0.0, 0.0), "Kd": (0.4, 0.4, 0.4)})
        elif cur is None:
            continue
        elif key == "map_Kd":
            cur["map_Kd"] = args[-1]
        elif key == "map_glow":
            cur["map_glow"] = args[-1]
        elif key == "Kd":
            cur["Kd"] = tuple(float(a) for a in args[:3])
        elif key == "d":
            cur["d"] = float(args[0])
        elif key == "Ke":
            cur["Ke"] = tuple(float(a) for a in args[:3])
    return mats


def parse_obj(path):
    verts, uvs, faces, mtllibs = [], [], [], []
    mtl, smooth = None, 0
    for line in open(path, encoding="latin-1"):
        parts = line.split("#", 1)[0].split()
        if not parts:
            continue
        key, args = parts[0], parts[1:]
        if key == "v":
            verts.append(tuple(float(a) for a in args[:3]))
        elif key == "vt":
            uvs.append((float(args[0]), float(args[1]) if len(args) > 1 else 0.0))
        elif key == "f":
            corners = []
            for a in args:
                bits = a.split("/")
                vi = int(bits[0])
                vi = vi - 1 if vi > 0 else len(verts) + vi
                ti = None
                if len(bits) > 1 and bits[1]:
                    ti = int(bits[1])
                    ti = ti - 1 if ti > 0 else len(uvs) + ti
                corners.append((vi, ti))
            for i in range(1, len(corners) - 1):
                faces.append((mtl, smooth, (corners[0], corners[i], corners[i + 1])))
        elif key == "usemtl":
            mtl = " ".join(args)
        elif key == "s":
            smooth = 0 if not args or args[0] == "off" else int(args[0])
        elif key == "mtllib":
            mtllibs.append(" ".join(args))
    return verts, uvs, faces, mtllibs


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def norm(a):
    length = math.sqrt(dot(a, a))
    return (a[0] / length, a[1] / length, a[2] / length) if length > 1e-12 else (0.0, 0.0, 1.0)


def to_gltf(p, eye):
    """OBJ (right, up, fwd) units -> game eye metres -> glTF."""
    right = p[0] * MODEL_SCALE - eye[0]
    up = p[1] * MODEL_SCALE - eye[1]
    fwd = p[2] * MODEL_SCALE - eye[2]
    gx, gy, gz = right, -up, fwd  # game axes: x right, y down, z forward
    return (-gx, gz, -gy)


def convert(name, gamedata, work):
    obj_name, eye = CRAFTS[name][:2]
    tex_dir = CRAFTS[name][2] if len(CRAFTS[name]) > 2 else None
    obj_path = os.path.join(gamedata, "Models", obj_name)
    verts, uvs, faces, mtllibs = parse_obj(obj_path)
    mats = {}
    for lib in mtllibs:
        mats.update(parse_mtl(os.path.join(gamedata, "Models", lib)))
    gverts = [to_gltf(v, eye) for v in verts]

    # Face normals, oriented toward the pilot (eye = origin): every cockpit
    # surface is seen from the seat. Smooth within OBJ smoothing groups.
    tris = []
    smooth_acc = {}
    for mtl, smooth, corners in faces:
        p = [gverts[c[0]] for c in corners]
        n = cross(sub(p[1], p[0]), sub(p[2], p[0]))
        if dot(n, n) < 1e-18:
            continue
        centroid = ((p[0][0] + p[1][0] + p[2][0]) / 3, (p[0][1] + p[1][1] + p[2][1]) / 3,
                    (p[0][2] + p[1][2] + p[2][2]) / 3)
        if dot(n, centroid) > 0:  # pointing away from the eye: flip
            corners = (corners[0], corners[2], corners[1])
            n = (-n[0], -n[1], -n[2])
        n = norm(n)
        tris.append((mtl, smooth, corners, n))
        if smooth:
            for c in corners:
                acc = smooth_acc.setdefault((smooth, c[0]), [0.0, 0.0, 0.0])
                acc[0] += n[0]
                acc[1] += n[1]
                acc[2] += n[2]

    # Per material: unique (position, uv, normal) vertices.
    prims = {}
    for mtl, smooth, corners, n in tris:
        prim = prims.setdefault(mtl, {"map": {}, "pos": [], "nrm": [], "uv": [], "idx": []})
        for c in corners:
            vn = n
            if smooth:
                acc = smooth_acc[(smooth, c[0])]
                vn = norm(acc) if dot(acc, n) > 0.5 * math.sqrt(dot(acc, acc)) else n
            uv = uvs[c[1]] if c[1] is not None and c[1] < len(uvs) else (0.0, 0.0)
            key = (c[0], round(uv[0], 6), round(uv[1], 6), round(vn[0], 4), round(vn[1], 4), round(vn[2], 4))
            index = prim["map"].get(key)
            if index is None:
                index = len(prim["pos"])
                prim["map"][key] = index
                prim["pos"].append(gverts[c[0]])
                prim["nrm"].append(vn)
                prim["uv"].append((uv[0], 1.0 - uv[1]))  # glTF v runs down the image
            prim["idx"].append(index)

    images, image_index = [], {}

    def emissive(kd_rel, glow_rel):
        """Revival's map_glow is a self-lit mask: emission = diffuse * mask."""
        paths = []
        for rel in (kd_rel, glow_rel):
            src = os.path.join(gamedata, "Models", rel)
            if not os.path.exists(src):
                src = os.path.join(gamedata, "Textures", rel)
            if not os.path.exists(src):
                return None
            paths.append(src)
        key = ("emissive", kd_rel, glow_rel)
        if key not in image_index:
            kd = Image.open(paths[0]).convert("RGB")
            mask = Image.open(paths[1]).convert("RGB").resize(kd.size)
            png = f"{name}_{len(images):03d}_e.png"
            ImageChops.multiply(kd, mask).save(os.path.join(work, png))
            image_index[key] = len(images)
            images.append({"uri": png})
        return image_index[key]

    def image(rel):
        src = os.path.join(gamedata, "Models", rel)
        if not os.path.exists(src):
            src = os.path.join(gamedata, "Textures", rel)
        if not os.path.exists(src):
            print(f"{name}: missing texture {rel}", file=sys.stderr)
            return None
        if rel not in image_index:
            png = f"{name}_{len(images):03d}.png"
            Image.open(src).convert("RGB").save(os.path.join(work, png))
            image_index[rel] = len(images)
            images.append({"uri": png})
        return image_index[rel]

    materials, mat_index = [], {}
    bin_data = bytearray()
    buffer_views, accessors, primitives = [], [], []

    def add_view(data, target):
        off = len(bin_data)
        bin_data.extend(data)
        while len(bin_data) % 4:
            bin_data.append(0)
        buffer_views.append({"buffer": 0, "byteOffset": off, "byteLength": len(data), "target": target})
        return len(buffer_views) - 1

    for mtl, prim in prims.items():
        if len(prim["pos"]) > 65535:
            raise SystemExit(f"{name}: material {mtl} has too many vertices")
        if mtl not in mat_index:
            m = mats.get(mtl)
            named = re.search(r"_(TEX\d+)\.bmp(_Dark)?$", mtl or "", re.IGNORECASE)
            if m is None and named and tex_dir:
                # tie-interceptor_cockpit_vr names most materials after
                # their texture ("Material014_TEX00013.bmp") without
                # defining them in its .mtl; Revival draws them untextured.
                dark = 0.2 if named.group(2) else 0.4
                m = {"d": 1.0, "Ke": (0.0, 0.0, 0.0), "Kd": (dark, dark, dark),
                     "map_Kd": f"{tex_dir}/{named.group(1)}.BMP"}
            if m is None:
                m = {"d": 1.0, "Ke": (0.0, 0.0, 0.0), "Kd": (0.4, 0.4, 0.4)}
            alpha = m["d"]
            mat = {
                "name": mtl or "default",
                "pbrMetallicRoughness": {"baseColorFactor": [min(k, BASE_COLOR_MAX) for k in m["Kd"]] + [alpha],
                                         "metallicFactor": 0.0, "roughnessFactor": ROUGHNESS},
                "doubleSided": True,
            }
            if alpha < 1.0:
                mat["alphaMode"] = "BLEND"
            screen = m.get("map_Kd", "").lower().endswith(".ani")
            if screen:
                # Revival's live display (health/target/throttle): a dark
                # screen here; OpenXW's instruments are separate panels.
                mat["pbrMetallicRoughness"]["baseColorFactor"] = [0.03, 0.03, 0.035, alpha]
                mat["pbrMetallicRoughness"]["roughnessFactor"] = 0.3
            tex = image(m["map_Kd"]) if "map_Kd" in m and not screen else None
            if tex is not None:
                mat["pbrMetallicRoughness"]["baseColorTexture"] = {"index": tex}
            glow = emissive(m["map_Kd"], m["map_glow"]) if "map_glow" in m and "map_Kd" in m and not screen else None
            if glow is not None:
                mat["emissiveTexture"] = {"index": glow}
                mat["emissiveFactor"] = [EMISSIVE, EMISSIVE, EMISSIVE]
            elif any(k > 0 for k in m["Ke"]):
                mat["emissiveFactor"] = list(m["Ke"])
            mat_index[mtl] = len(materials)
            materials.append(mat)
        pos = b"".join(struct.pack("<3f", *p) for p in prim["pos"])
        nrm = b"".join(struct.pack("<3f", *n) for n in prim["nrm"])
        uv = b"".join(struct.pack("<2f", *t) for t in prim["uv"])
        idx = b"".join(struct.pack("<H", i) for i in prim["idx"])
        acc0 = len(accessors)
        accessors.append({"bufferView": add_view(pos, 34962), "componentType": 5126, "count": len(prim["pos"]),
                          "type": "VEC3", "min": [min(p[i] for p in prim["pos"]) for i in range(3)],
                          "max": [max(p[i] for p in prim["pos"]) for i in range(3)]})
        accessors.append({"bufferView": add_view(nrm, 34962), "componentType": 5126, "count": len(prim["nrm"]),
                          "type": "VEC3"})
        accessors.append({"bufferView": add_view(uv, 34962), "componentType": 5126, "count": len(prim["uv"]),
                          "type": "VEC2"})
        accessors.append({"bufferView": add_view(idx, 34963), "componentType": 5123, "count": len(prim["idx"]),
                          "type": "SCALAR"})
        primitives.append({"attributes": {"POSITION": acc0, "NORMAL": acc0 + 1, "TEXCOORD_0": acc0 + 2},
                           "indices": acc0 + 3, "material": mat_index[mtl], "mode": 4})

    textures = [{"source": i} for i in range(len(images))]
    gltf = {
        "asset": {"version": "2.0", "generator": "OpenXW gen_xw_vr_cockpits.py"},
        "extensionsUsed": ["AERON_flight_model"],
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [
            {"name": f"xw_vr_cockpit_{name}", "children": [1],
             "extensions": {"AERON_flight_model": {"role": "model"}}},
            {"name": "cockpit", "mesh": 0,
             "extensions": {"AERON_flight_model": {"role": "component", "meshType": 0}}},
        ],
        "meshes": [{"name": "cockpit", "primitives": primitives}],
        "materials": materials,
        "images": images,
        "textures": textures,
        "buffers": [{"uri": f"{name}.bin", "byteLength": len(bin_data)}],
        "bufferViews": buffer_views,
        "accessors": accessors,
    }
    gltf_path = os.path.join(work, f"{name}.gltf")
    with open(gltf_path, "w") as f:
        json.dump(gltf, f)
    with open(os.path.join(work, f"{name}.bin"), "wb") as f:
        f.write(bin_data)
    ntri = sum(len(p["idx"]) for p in prims.values()) // 3
    nvert = sum(len(p["pos"]) for p in prims.values())
    return gltf_path, nvert, ntri, len(images)


def main():
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    cook, gamedata, out_dir = sys.argv[1:4]
    names = []
    for spec in sys.argv[4:] or DEFAULT_CRAFTS:
        if "=" in spec:
            name, rest = spec.split("=", 1)
            obj, offs = rest.split(":")
            CRAFTS[name] = (obj, tuple(float(v) for v in offs.split(",")))
        elif spec not in CRAFTS:
            raise SystemExit(f"unknown craft {spec}")
        names.append(spec.split("=", 1)[0])
    os.makedirs(out_dir, exist_ok=True)
    work = tempfile.mkdtemp(prefix="xw_vr_cockpits_")
    try:
        for name in names:
            gltf, nvert, ntri, nimg = convert(name, gamedata, work)
            glb = os.path.join(out_dir, f"{name}.glb")
            subprocess.run([cook, gltf, "--out", glb, "--no-zstd", "--rgba8", "--max-atlas", "2048"], check=True,
                           stdout=subprocess.DEVNULL)
            print(f"{name}: {nvert} vertices, {ntri} triangles, {nimg} textures -> {os.path.getsize(glb)} bytes")
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
