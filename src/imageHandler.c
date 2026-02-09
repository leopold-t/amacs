#include "imageHandler.h"

#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

/* Screen format assumptions */
#define WIDTH  640
#define HEIGHT 256
#define DEPTH  4

BOOL LoadRawImageToScreen(const char *filename, struct Screen *screen)
{
    BPTR fh;
    LONG bytesPerRow = WIDTH / 8;
    LONG planeSize = bytesPerRow * HEIGHT;
    int i;

    if (!screen)
        return FALSE;

    fh = Open((STRPTR)filename, MODE_OLDFILE);
    if (!fh)
        return FALSE;

    for (i = 0; i < DEPTH; i++) {
        if (!screen->BitMap.Planes[i]) {
            Close(fh);
            return FALSE;
        }

        if (Read(fh, screen->BitMap.Planes[i], planeSize) != planeSize) {
            Close(fh);
            return FALSE;
        }
    }

    Close(fh);
    return TRUE;
}

void WaitForExitEvent(struct Window *window)
{
    struct IntuiMessage *msg;
    BOOL done = FALSE;

    while (!done) {
        Wait(1L << window->UserPort->mp_SigBit);

        while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort))) {
            if (msg->Class == IDCMP_MOUSEBUTTONS &&
                msg->Code == SELECTDOWN) {
                done = TRUE;
            }

            if (msg->Class == IDCMP_RAWKEY &&
                msg->Code == 0x45) { /* ESC */
                done = TRUE;
            }

            ReplyMsg((struct Message *)msg);
        }

        WaitTOF();
    }
}
