#ifndef XW_APP_VR_KEYBOARD_H
#define XW_APP_VR_KEYBOARD_H

#include "aeron/aeron.h"
#include "xw_app/ui.h"

#include <stdbool.h>

/* On-screen keyboard for the Quest build. Horizon OS does not show its
 * system keyboard over an immersive app, so while a text field (pilot name)
 * is on screen a key grid is drawn with the app UI and clicked with the
 * controller laser. It opens when the field is clicked with the laser, and
 * closes with Enter/Close or when the field goes away. Keys reach the game
 * as ordinary SDL text/key events.
 *
 * `enabled` is true in VR; the env var OPENXW_VR_KEYBOARD=1 also enables it
 * on the desktop for testing. */
void XwVrKeyboard_Init(XwAppUi* ui, bool enabled);
/* Before the game reads input: open/close and keep keyboard clicks away
 * from the game. */
void XwVrKeyboard_BeginFrame(const AeronInputSnapshot* input);
/* After the game frame: draw the keyboard and handle its keys. */
void XwVrKeyboard_Frame(const AeronInputSnapshot* input, float dt_seconds);

#endif
