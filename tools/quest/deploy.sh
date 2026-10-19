#!/usr/bin/env bash
# Install OpenXW VR on the connected Quest, copy the original game data once,
# grant all-files access and launch. Usage:
#   tools/quest/deploy.sh              # install + launch
#   PUSH_DATA=1 tools/quest/deploy.sh  # also (re)copy the game data
#
# Device layout (matches OpenXWActivity):
#   /sdcard/OpenXW/XW94  <- Steam "collectors_cd" (1994 Collector's CD-ROM)
#   /sdcard/OpenXW/XW98  <- Steam "remastered"    (1998 Windows release)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ADB="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
PKG=com.bilko.openxwvr
APK="${APK:-$ROOT/android/app/build/outputs/apk/debug/app-debug.apk}"
GAME_DIR="${XW_GAME_DIR:-$HOME/.steam/debian-installation/steamapps/common/STAR WARS X-Wing}"
DEVICE_DATA=/sdcard/OpenXW

"$ADB" install -r "$APK"

# RESET_CONFIG=1 deletes the saved OpenXW settings on the headset, e.g. after
# changing the shipped Touch controller defaults (a controller already saved
# in the user config keeps its old bindings).
if [[ "${RESET_CONFIG:-0}" == 1 ]]; then
    echo "== resetting saved OpenXW settings"
    "$ADB" shell am force-stop "$PKG"
    "$ADB" shell run-as "$PKG" rm -f files/config.yaml
fi

push_edition() { # push_edition <steam subdir> <device subdir> <marker file>
    local src="$GAME_DIR/$1" dst="$DEVICE_DATA/$2"
    if [[ "${PUSH_DATA:-0}" == 1 ]] || ! "$ADB" shell "test -f $dst/$3"; then
        echo "== copying $1 to $dst (one time)"
        "$ADB" shell mkdir -p "$dst"
        # DOSBOX is the Steam DOS launcher, not game data.
        for entry in "$src"/*; do
            [[ "$(basename "$entry")" == DOSBOX ]] && continue
            "$ADB" push "$entry" "$dst/" >/dev/null
        done
        "$ADB" shell "touch $dst/.openxw-copied"
    fi
}
push_edition collectors_cd XW94 .openxw-copied
push_edition remastered XW98 .openxw-copied

"$ADB" shell appops set --uid "$PKG" MANAGE_EXTERNAL_STORAGE allow
"$ADB" shell am force-stop "$PKG"
"$ADB" logcat -c
"$ADB" shell am start -n "$PKG/.OpenXWActivity"
