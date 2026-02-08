#ifndef IMAGE_HANDLER_H
#define IMAGE_HANDLER_H

#include <intuition/screens.h>

/* Load RAW planar image directly into a custom screen bitmap */
BOOL LoadRawImageToScreen(const char *filename, struct Screen *screen);

/* Wait for LMB or ESC on given window */
void WaitForExitEvent(struct Window *window);

#endif /* IMAGE_HANDLER_H */
