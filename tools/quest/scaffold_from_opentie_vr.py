#!/usr/bin/env python3
"""One-time import of the Quest scaffolding from the OpenTIE-VR port.

Copies the Android/Gradle app, the OpenXR headers and the Quest build scripts
from ~/Projekte/OpenTIE and renames package/activity/paths for OpenXW.
Re-running overwrites the copied files; src/xw_vr is generated separately by
tools/quest/gen_xw_vr.py.
"""
import pathlib
import shutil

SRC = pathlib.Path.home() / "Projekte/OpenTIE"
DST = pathlib.Path(__file__).resolve().parents[2]

RENAMES = [
    ("com.bilko.opentievr", "com.bilko.openxwvr"),
    ("com/bilko/opentievr", "com/bilko/openxwvr"),
    ("OpenTIEActivity", "OpenXWActivity"),
    ("OpenTIE-VR", "OpenXW-VR"),
    ("OpenTIE VR", "OpenXW VR"),
    (".opentie-copied", ".openxw-copied"),
    ("OpenTIE", "OpenXW"),
    ("TIE Fighter", "X-Wing"),
    ("build-tie", "build-xw"),
    ("--target tie ", "--target xw "),
    ("-DTIE_BUILD_TOOLS", "-DXW_BUILD_TOOLS"),
    ("-DTIE_ENABLE_DEBUG_UI", "-DXW_ENABLE_DEBUG_UI"),
    ("-DENABLE_FLUIDSYNTH=OFF", "-DIMUSE_ENABLE_FLUIDSYNTH=OFF"),
]

# Game-specific edits applied after the generic renames (file -> pairs).
EDITS = {
    "android/app/src/main/java/com/bilko/openxwvr/OpenXWActivity.java": [
        ("    /** Original X-Wing installations copied from the PC: the 1995\n"
         "     *  Collector's CD-ROM and the 1998 Windows release. */\n",
         "    /** Original X-Wing installations copied from the PC: the 1994\n"
         "     *  Collector's CD-ROM and the 1998 Windows release. */\n"),
        ('TIE95_DATA_DIR = GAME_DATA_DIR + "/TIE95"', 'XW94_DATA_DIR = GAME_DATA_DIR + "/XW94"'),
        ('TIE98_DATA_DIR = GAME_DATA_DIR + "/TIE98"', 'XW98_DATA_DIR = GAME_DATA_DIR + "/XW98"'),
        ('"--tie95-data", TIE95_DATA_DIR, "--tie98-data", TIE98_DATA_DIR',
         '"--xw94-data", XW94_DATA_DIR, "--xw98-data", XW98_DATA_DIR'),
    ],
    "android/app/src/main/AndroidManifest.xml": [
        ("/sdcard/OpenXW/{TIE95,TIE98}", "/sdcard/OpenXW/{XW94,XW98}"),
    ],
    "android/app/build.gradle": [
        ("versionCode 5", "versionCode 1"),
        ("versionName '0.1.4-beta'", "versionName '0.1.0-beta'"),
    ],
    "tools/quest/deploy.sh": [
        ("#   /sdcard/OpenXW/TIE95  <- Steam \"collectors_cd\" (1995 Collector's CD-ROM)\n"
         "#   /sdcard/OpenXW/TIE98  <- Steam \"remastered\"    (1998 Windows release)\n",
         "#   /sdcard/OpenXW/XW94  <- Steam \"collectors_cd\" (1994 Collector's CD-ROM)\n"
         "#   /sdcard/OpenXW/XW98  <- Steam \"remastered\"    (1998 Windows release)\n"),
        ('GAME_DIR="${TIE_GAME_DIR:-$HOME/.steam/debian-installation/steamapps/common/STAR WARS Tie Fighter}"',
         'GAME_DIR="${XW_GAME_DIR:-$HOME/.steam/debian-installation/steamapps/common/STAR WARS X-Wing}"'),
        ("push_edition collectors_cd TIE95", "push_edition collectors_cd XW94"),
        ("push_edition remastered TIE98", "push_edition remastered XW98"),
    ],
    "tools/quest/build-deps.sh": [
        ('BORROW="$HOME/Projekte/OpenXWA-VR/build/quest/src"', 'BORROW="$HOME/Projekte/OpenTIE/build/quest/src"'),
        ("from OpenXWA-VR", "from OpenTIE-VR"),
        ("~/Projekte/OpenXWA-VR/build/quest/src", "~/Projekte/OpenTIE/build/quest/src"),
    ],
    "tools/quest/build-native.sh": [
        # X-Wing has no Quest-specific controller config transform yet.
        ('# Quest Touch controller defaults (desktop keeps the upstream gamepad layout).\n'
         'python3 "$ROOT/tools/quest/quest_config.py" "$RUNTIME/resources/config.yaml"\n', ""),
    ],
}

TEXT_SUFFIXES = {".gradle", ".xml", ".java", ".properties", ".h", ".sh", ""}


def rename(text: str) -> str:
    for old, new in RENAMES:
        text = text.replace(old, new)
    return text


def copy_file(src: pathlib.Path, rel_out: str) -> None:
    out = DST / rel_out
    out.parent.mkdir(parents=True, exist_ok=True)
    if src.suffix in TEXT_SUFFIXES and not src.name.endswith(".jar"):
        try:
            out.write_text(rename(src.read_text()))
        except UnicodeDecodeError:
            shutil.copy2(src, out)
    else:
        shutil.copy2(src, out)
    shutil.copymode(src, out)


def copy_tree(rel: str) -> int:
    count = 0
    for src in (SRC / rel).rglob("*"):
        s = str(src)
        if src.is_dir() or "/build/" in s or "/.gradle/" in s or src.name == "local.properties":
            continue
        copy_file(src, rename(str(src.relative_to(SRC))))
        count += 1
    return count


def main() -> None:
    n = copy_tree("android")
    n += copy_tree("third_party/openxr")
    for script in ("build-deps.sh", "build-native.sh", "deploy.sh"):
        copy_file(SRC / "tools/quest" / script, f"tools/quest/{script}")
        n += 1
    for rel, pairs in EDITS.items():
        path = DST / rel
        text = path.read_text()
        for old, new in pairs:
            if old not in text:
                raise SystemExit(f"{rel}: pattern not found: {old[:60]!r}")
            text = text.replace(old, new)
        path.write_text(text)
    print(f"copied {n} files")


if __name__ == "__main__":
    main()
