#ifndef XW_APP_TEST_DRIVER_H
#define XW_APP_TEST_DRIVER_H

/* Desktop test driver for headless bring-up: with OPENXW_TEST_CMDS=<file>,
 * one command line is taken from the file per frame and injected as SDL
 * input. Commands: "text ABC", "key Return", "click X Y" / "move X Y"
 * (window pixels), "wait N" (frames). Does nothing without the env var. */
void XwTestDriver_Init(void);
void XwTestDriver_Frame(void);

#endif
