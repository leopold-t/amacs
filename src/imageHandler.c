#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <string.h>

#include "imageHandler.h"

BOOL LoadRawImageToScreen(const char *filename, struct Screen *screen) {
    if (!screen || !screen->RastPort.BitMap)
        return FALSE;

    struct BitMap *dst = screen->RastPort.BitMap;
    UWORD width = (UWORD)screen->Width;
    UWORD height = (UWORD)screen->Height;
    UWORD depth = (UWORD)dst->Depth;

    SetRast(&screen->RastPort, 0);
    WaitBlit();
    WaitTOF();

    struct BitMap tmp;
    InitBitMap(&tmp, depth, width, height);

    ULONG bytesPerRow = (ULONG)(((width + 15) >> 4) << 1);
    ULONG planeSize = bytesPerRow * (ULONG)height;

    for (UWORD p = 0; p < depth; p++) {
        tmp.Planes[p] = AllocRaster(width, height);

        if (!tmp.Planes[p]) {
            for (UWORD q = 0; q < p; q++)
                FreeRaster(tmp.Planes[q], width, height);
            return FALSE;
        }

        memset(tmp.Planes[p], 0, planeSize);
    }

    BPTR fh = Open((STRPTR)filename, MODE_OLDFILE);

    if (!fh) {
        for (UWORD p = 0; p < depth; p++)
            FreeRaster(tmp.Planes[p], width, height);
        return FALSE;
    }

    for (UWORD p = 0; p < depth; p++) {
        if (Read(fh, tmp.Planes[p], planeSize) != (LONG)planeSize) {
            Close(fh);
            for (UWORD p2 = 0; p2 < depth; p2++)
                FreeRaster(tmp.Planes[p2], width, height);
            return FALSE;
        }
    }

    Close(fh);

    WaitBlit();
    BltBitMap(&tmp, 0, 0, dst, 0, 0, width, height, 0xC0, 0xFF, NULL);
    WaitBlit();

    for (UWORD p = 0; p < depth; p++)
        FreeRaster(tmp.Planes[p], width, height);

    return TRUE;
}