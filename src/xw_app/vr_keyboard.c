/* Quest on-screen keyboard (see vr_keyboard.h). */

#include "xw_app/vr_keyboard.h"

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "aeron/log.h"
#include "aeron/render.h"
#include "aeron/scene/ui.h"
#include "xw_app/settings/settings.h"
#include "xw_runtime/input/capture.h"
#include "xw_runtime/input/input_bridge.h"
#include "xw_vr/xw_vr.h"

#define VR_KEYBOARD_LOG "xw.vr"

enum {
	/* Frames the game keeps ignoring mouse buttons after the keyboard
	 * closes, so the click on "Close" does not land in the game. */
	VR_KEYBOARD_RELEASE_FRAMES = 15,
	/* Extra margin (game pixels) so the laser need not hit the thin field. */
	VR_KEYBOARD_FIELD_MARGIN = 4,
};

static AeronUiContext* g_ui;
static bool            g_enabled;
static bool            g_open;
static bool            g_dismissed;  /* closed by the player; stays closed */
static bool            g_field_seen; /* a text field was on screen last frame */
static bool            g_field_top;  /* the field sits in the upper half */
static int             g_block_frames;
/* SDL text events reference their string until they are read. */
static char g_chars[128][2];

static void XwVrKeyboard_SetOpen(bool open, const char* why) {
	if (open != g_open)
		Aeron_LogInfo(VR_KEYBOARD_LOG, "keyboard %s (%s)", open ? "opened" : "closed", why);
	g_open = open;
	XwVr_SetTextEntryActive(open ? 1 : 0);
}

void XwVrKeyboard_Init(XwAppUi* ui, bool enabled) {
	const char* env = getenv("OPENXW_VR_KEYBOARD");
	g_ui            = ui ? XwAppUi_Context(ui) : NULL;
	g_enabled       = enabled || (env && env[0] == '1');
	for (int c = 0; c < 128; ++c) {
		g_chars[c][0] = (char)c;
		g_chars[c][1] = '\0';
	}
}

static void XwVrKeyboard_PushText(char c) {
	SDL_Event event;

	SDL_SetEventEnabled(SDL_EVENT_TEXT_INPUT, true);
	SDL_zero(event);
	event.type           = SDL_EVENT_TEXT_INPUT;
	event.text.timestamp = SDL_GetTicksNS();
	event.text.text      = g_chars[(unsigned char)c & 0x7f];
	SDL_PushEvent(&event);
}

static void XwVrKeyboard_TapKey(SDL_Scancode scancode) {
	for (int down = 1; down >= 0; --down) {
		SDL_Event event;
		SDL_zero(event);
		event.type          = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
		event.key.timestamp = SDL_GetTicksNS();
		event.key.scancode  = scancode;
		event.key.key       = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);
		event.key.down      = down ? true : false;
		SDL_PushEvent(&event);
	}
}

void XwVrKeyboard_BeginFrame(const AeronInputSnapshot* input) {
	int rect[4], size[2];

	if (!g_enabled || !g_ui || !input)
		return;
	const bool field = XwInput_TextFieldRect(rect, size);
	if (!field) {
		/* Pilot screen left: forget the player's Close for next time. */
		if (g_open)
			XwVrKeyboard_SetOpen(false, "no text field on screen");
		g_dismissed  = false;
		g_field_seen = false;
	} else {
		g_field_top = size[1] > 0 && (rect[1] + rect[3]) < size[1];
		/* Enter typed with the A button (xw_vr.c leaves text mode). */
		if (g_open && !XwVr_TextEntryActive() && !getenv("OPENXW_VR_KEYBOARD")) {
			g_open      = false;
			g_dismissed = true;
		}
		if (!g_field_seen)
			Aeron_LogInfo(VR_KEYBOARD_LOG, "text field %d,%d-%d,%d on %dx%d", rect[0], rect[1], rect[2], rect[3],
						  size[0], size[1]);
		if (!g_field_seen && !g_dismissed)
			XwVrKeyboard_SetOpen(true, "text field on screen");
		g_field_seen = true;
		/* Laser click into the field reopens it; this click stays the game's. */
		if (!g_open && !XwSettingsMenu_Open() && (input->mouse.pressed_buttons & AERON_MOUSE_BUTTON_LEFT)) {
			int x, y;
			XwInput_CursorPosition(&x, &y);
			if (x >= rect[0] - VR_KEYBOARD_FIELD_MARGIN && x <= rect[2] + VR_KEYBOARD_FIELD_MARGIN &&
				y >= rect[1] - VR_KEYBOARD_FIELD_MARGIN && y <= rect[3] + VR_KEYBOARD_FIELD_MARGIN) {
				g_dismissed = false;
				XwVrKeyboard_SetOpen(true, "text field clicked");
				return;
			}
		}
	}
	/* The keyboard owns the pointer: game screens must not see its clicks. */
	if (g_open && !XwSettingsMenu_Open())
		g_block_frames = VR_KEYBOARD_RELEASE_FRAMES;
	else if (g_block_frames > 0)
		--g_block_frames;
	if (g_block_frames > 0)
		XwInput_BlockMouseButtons(input->mouse.buttons | input->mouse.pressed_buttons);
}

static void XwVrKeyboard_Row(const char* keys) {
	const int count = (int)SDL_strlen(keys);

	AeronUi_BeginColumns(g_ui, count, NULL);
	for (int i = 0; i < count; ++i) {
		if (i > 0)
			AeronUi_NextColumn(g_ui);
		if (AeronUi_Button(g_ui, g_chars[(unsigned char)keys[i]]))
			XwVrKeyboard_PushText(keys[i]);
	}
	AeronUi_EndColumns(g_ui);
}

void XwVrKeyboard_Frame(const AeronInputSnapshot* input, float dt_seconds) {
	if (!g_enabled || !g_ui || !input || !g_open || XwSettingsMenu_Open())
		return;

	/* Laser only: in menus the VR pad keeps reporting the throttle lever on
	 * its stick, which the UI would read as endless focus navigation. */
	static AeronInputSnapshot keyboard_input;
	keyboard_input = *input;
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i)
		keyboard_input.controllers[i].connected = 0;

	AeronUi_BeginFrame(g_ui, &(AeronUiFrameDesc) { .input = &keyboard_input, .dt_seconds = dt_seconds });
	/* Horizontally centred, in the half of the screen without the field. */
	AeronUiWindowDesc window = { .width_ref = 960.0f, .y_ref = 12.0f, .flags = AERON_UI_WINDOW_NO_TITLE };
	AeronRectI        content;
	if (Aeron_GetContentPixelRect(&content) && content.height > 0)
		window.x_ref = ((float)content.width * 1080.0f / (float)content.height - window.width_ref) * 0.5f;
	if (g_field_top)
		window.y_ref = 1080.0f - 12.0f - 470.0f;
	if (AeronUi_BeginWindow(g_ui, "Keyboard", &window)) {
		/* Pilot names: the game takes letters (upper case), digits, - and _. */
		XwVrKeyboard_Row("1234567890");
		XwVrKeyboard_Row("QWERTYUIOP");
		XwVrKeyboard_Row("ASDFGHJKL-");
		XwVrKeyboard_Row("ZXCVBNM_");
		AeronUi_BeginColumns(g_ui, 3, NULL);
		if (AeronUi_Button(g_ui, "Delete"))
			XwVrKeyboard_TapKey(SDL_SCANCODE_BACKSPACE);
		AeronUi_NextColumn(g_ui);
		if (AeronUi_Button(g_ui, "Enter")) {
			XwVrKeyboard_TapKey(SDL_SCANCODE_RETURN);
			g_dismissed = true;
			XwVrKeyboard_SetOpen(false, "Enter");
		}
		AeronUi_NextColumn(g_ui);
		if (AeronUi_Button(g_ui, "Close")) {
			g_dismissed = true;
			XwVrKeyboard_SetOpen(false, "Close");
		}
		AeronUi_EndColumns(g_ui);
		AeronUi_EndWindow(g_ui);
	}
	AeronUi_EndFrame(g_ui);
	AeronUi_Submit(g_ui);
}
