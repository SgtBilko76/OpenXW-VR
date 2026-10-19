#!/usr/bin/env python3
"""Swap in the Quest Touch controller defaults when staging the APK runtime.

OpenXW reads its shipped gamepad defaults from resources/config.yaml
(`input.gamepad_defaults`). The Quest build exposes both Touch controllers as
one virtual gamepad (src/xw_vr/xw_vr.c, vr_update_gamepad); the physical
sticks are swapped there, so on the controllers the LEFT stick flies
pitch/yaw and the RIGHT stick does roll (x) + throttle lever (y), the layout
used by the OpenTIE-VR and OpenXWA-VR ports:

  physical right stick -> pad leftx (roll) / lefty (throttle lever)
  physical left stick  -> pad rightx (yaw) / righty (pitch)
  triggers             -> lefttrigger / righttrigger
  A / B / X / Y        -> south / east / west / north
  right grip           -> rightshoulder (hold: rear view)
  stick clicks         -> swapped too: left click = rightstick, right = leftstick
  left Menu (tap)      -> start
  left grip held (shift layer): A/B/X/Y -> dpaddown/dpadright/dpadleft/dpadup,
                       right stick click -> guide, left stick click -> back,
                       left trigger -> leftshoulder

Usage: quest_config.py <staged resources/config.yaml>
Only the Quest APK is affected; desktop builds keep the upstream defaults.
"""
import re
import sys

QUEST_GAMEPAD_DEFAULTS = """\
  gamepad_defaults:
    axes:
      yaw: {source: rightx, invert: false, deadzone: 0.08}
      pitch: {source: righty, invert: false, deadzone: 0.08}
      roll: {source: leftx, invert: false, deadzone: 0.08}
      throttle: {source: lefty, invert: false}
    buttons:
      fire_weapon:
        - {axis: righttrigger, direction: positive, threshold: 0.5}
      target_roll_modifier:
        - {axis: lefttrigger, direction: positive, threshold: 0.5}
      target_next: south
      target_nearest_fighter: east
      target_my_attacker: west
      match_speed: north
      hold_view_rear: rightshoulder
      view_forward: rightstick
      hyperspace: leftstick
      pause: start
      throttle_full: dpaddown
      throttle_down: dpadright
      target_clear: dpadleft
      escape: dpadup
      cycle_weapon_group: back
      cycle_shield_mode: guide
      cycle_weapon_firing_mode: leftshoulder
"""


def main() -> int:
    path = sys.argv[1]
    text = open(path, encoding="utf-8").read()
    # The block runs from "  gamepad_defaults:" to the next key at the same
    # indentation (two spaces) or less.
    pattern = re.compile(r"^  gamepad_defaults:\n(?:^(?:    .*)?\n)*", re.M)
    if not pattern.search(text):
        print("quest_config: input.gamepad_defaults not found", file=sys.stderr)
        return 1
    text = pattern.sub(QUEST_GAMEPAD_DEFAULTS, text, count=1)
    open(path, "w", encoding="utf-8").write(text)
    print(f"quest_config: Touch controller defaults written to {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
