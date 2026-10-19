/* Desktop test driver (see test_driver.h). */
#include "xw_app/test_driver.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* g_path;
static char g_chars[128][2];

void XwTestDriver_Init(void) {
	g_path = getenv("OPENXW_TEST_CMDS");
	for (int c = 0; c < 128; ++c) {
		g_chars[c][0] = (char)c;
		g_chars[c][1] = '\0';
	}
	if (g_path)
		fprintf(stderr, "test driver: polling %s\n", g_path);
}

static SDL_Window* Window(void) {
	int count = 0;
	SDL_Window** windows = SDL_GetWindows(&count);
	SDL_Window* window = windows && count > 0 ? windows[0] : NULL;
	SDL_free(windows);
	return window;
}

static void PushKey(SDL_Scancode scancode, bool down) {
	SDL_Event e;
	SDL_zero(e);
	e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
	e.key.timestamp = SDL_GetTicksNS();
	e.key.scancode = scancode;
	e.key.key = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);
	e.key.down = down;
	e.key.windowID = SDL_GetWindowID(Window());
	SDL_PushEvent(&e);
}

static void PushMouse(float x, float y, int button, bool down) {
	SDL_Window* window = Window();
	if (window)
		SDL_WarpMouseInWindow(window, x, y);
	SDL_Event e;
	SDL_zero(e);
	e.type = SDL_EVENT_MOUSE_MOTION;
	e.motion.timestamp = SDL_GetTicksNS();
	e.motion.windowID = window ? SDL_GetWindowID(window) : 0;
	e.motion.x = x;
	e.motion.y = y;
	e.motion.state = down ? SDL_BUTTON_MASK(button) : 0;
	SDL_PushEvent(&e);
	SDL_zero(e);
	e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
	e.button.timestamp = SDL_GetTicksNS();
	e.button.windowID = window ? SDL_GetWindowID(window) : 0;
	e.button.button = (Uint8)button;
	e.button.down = down;
	e.button.clicks = 1;
	e.button.x = x;
	e.button.y = y;
	SDL_PushEvent(&e);
}

/* One command per poll; "click"/"key" release on the following frame. */
void XwTestDriver_Frame(void) {
	static int release_key = -1, release_button = 0, wait_frames = 0;
	static float release_x, release_y;
	if (!g_path)
		return;
	if (release_key >= 0) {
		PushKey((SDL_Scancode)release_key, false);
		release_key = -1;
		return;
	}
	if (release_button) {
		PushMouse(release_x, release_y, release_button, false);
		release_button = 0;
		return;
	}
	if (wait_frames > 0) {
		--wait_frames;
		return;
	}
	FILE* f = fopen(g_path, "r");
	if (!f)
		return;
	char line[256] = "", rest[4096] = "";
	if (!fgets(line, sizeof line, f)) {
		fclose(f);
		return;
	}
	size_t used = fread(rest, 1, sizeof rest - 1, f);
	rest[used] = '\0';
	fclose(f);
	f = fopen(g_path, "w");
	if (f) {
		fputs(rest, f);
		fclose(f);
	}
	line[strcspn(line, "\r\n")] = '\0';
	char word[32] = "", arg[200] = "";
	float x = 0, y = 0;
	sscanf(line, "%31s %199[^\n]", word, arg);
	fprintf(stderr, "test driver: %s\n", line);
	if (!strcmp(word, "text")) {
		SDL_SetEventEnabled(SDL_EVENT_TEXT_INPUT, true);
		for (const char* c = arg; *c; ++c) {
			SDL_Event e;
			SDL_zero(e);
			e.type = SDL_EVENT_TEXT_INPUT;
			e.text.timestamp = SDL_GetTicksNS();
			e.text.text = g_chars[(unsigned char)*c & 0x7f];
			SDL_PushEvent(&e);
		}
	} else if (!strcmp(word, "key")) {
		SDL_Scancode sc = SDL_GetScancodeFromName(arg);
		if (sc != SDL_SCANCODE_UNKNOWN) {
			PushKey(sc, true);
			release_key = sc;
		}
	} else if (!strcmp(word, "keydown") || !strcmp(word, "keyup")) {
		SDL_Scancode sc = SDL_GetScancodeFromName(arg);
		if (sc != SDL_SCANCODE_UNKNOWN)
			PushKey(sc, word[3] == 'd');
	} else if (!strcmp(word, "click") && sscanf(arg, "%f %f", &x, &y) == 2) {
		PushMouse(x, y, SDL_BUTTON_LEFT, true);
		release_button = SDL_BUTTON_LEFT;
		release_x = x;
		release_y = y;
	} else if (!strcmp(word, "move") && sscanf(arg, "%f %f", &x, &y) == 2) {
		PushMouse(x, y, SDL_BUTTON_LEFT, false);
	} else if (!strcmp(word, "wait")) {
		wait_frames = atoi(arg);
	}
}
