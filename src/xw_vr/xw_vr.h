#ifndef XW_VR_H
#define XW_VR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Quest VR presentation: an OpenXR session that shows the game on a large
 * virtual screen and turns the Touch controllers into a virtual gamepad, a
 * laser pointer and keyboard shortcuts. Call before Aeron_Init(). Returns 1
 * when VR presentation is active. A no-op that returns 0 on other platforms. */
int XwVr_Install(void);

/* Controller text entry (X in menus): 1 while the on-screen keyboard should
 * be shown. Always 0 off Quest. */
int  XwVr_TextEntryActive(void);
void XwVr_SetTextEntryActive(int active);

#ifdef __cplusplus
}
#endif

#endif
