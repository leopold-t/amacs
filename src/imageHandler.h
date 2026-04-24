#ifndef IMAGE_HANDLER_H
#define IMAGE_HANDLER_H

#include <intuition/screens.h>
#include <graphics/rastport.h>

/* Load RAW planar image directly into a custom screen bitmap */
BOOL LoadRawImageToScreen(const char *filename, struct Screen *screen);

/* Load RAW planar image into an arbitrary RastPort bitmap. */
BOOL LoadRawImageToRastPort(const char *filename, struct RastPort *rp, UWORD width, UWORD height);

/* Wait for LMB or ESC on given window */
void WaitForExitEvent(struct Window *window);

#endif /* IMAGE_HANDLER_H */
