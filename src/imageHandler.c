#include "imageHandler.h"

#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

/*
 * Load a planar RAW image directly into the Screen's BitMap planes.
 *
 * IMPORTANT:
 * - This function does NOT assume any fixed width/height/depth.
 * - It uses the actual Screen/BitMap parameters:
 *     BytesPerRow, Rows (height), Depth (number of bitplanes)
 *
 * RAW file format expected:
 *   plane 0 data, then plane 1 data, ... up to (depth-1)
 * where each plane is BytesPerRow * Height bytes.
 */
BOOL LoadRawImageToScreen(const char *filename, struct Screen *screen)
{
    BPTR fh;
    struct BitMap *bm;
    LONG bytesPerRow;
    LONG height;
    LONG planeSize;
    UWORD depth;
    UWORD p;

    if (!screen)
        return FALSE;

    bm = screen->RastPort.BitMap;
    if (!bm)
        return FALSE;

    bytesPerRow = bm->BytesPerRow;
    height      = screen->Height;
    depth       = bm->Depth;

    if (bytesPerRow <= 0 || height <= 0 || depth == 0)
        return FALSE;

    planeSize = bytesPerRow * height;

    fh = Open((STRPTR)filename, MODE_OLDFILE);
    if (!fh)
        return FALSE;

    for (p = 0; p < depth; p++) {
        if (!bm->Planes[p]) {
            Close(fh);
            return FALSE;
        }

        if (Read(fh, bm->Planes[p], planeSize) != planeSize) {
            Close(fh);
            return FALSE;
        }
    }

    Close(fh);
    return TRUE;
}

/*
 * Legacy helper: waits for LMB or ESC.
 * (Your main loop now also supports joystick fire + debounce.)
 */
void WaitForExitEvent(struct Window *window)
{
    struct IntuiMessage *msg;
    BOOL done = FALSE;

    if (!window || !window->UserPort)
        return;

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
