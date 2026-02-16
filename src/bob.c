#include "bob.h"

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>

static UWORD BytesPerRowWordAligned(UWORD width) {
    /* word-aligned to 16 pixels */
    return (UWORD)(((width + 15) >> 4) << 1);
}

BOOL Bob_LoadRawAndMask(AmacsBob *b, const char *rawFile, const char *maskFile, UWORD width,
                        UWORD height, UWORD depth) {
    if (!b || !rawFile || !maskFile || width == 0 || height == 0 || depth == 0) {
        return FALSE;
    }

    b->width = width;
    b->height = height;
    b->depth = depth;
    b->mask = NULL;

    /* init planar bitmap */
    InitBitMap(&b->bm, depth, width, height);

    /* allocate planes */
    for (UWORD p = 0; p < depth; p++) {
        b->bm.Planes[p] = (PLANEPTR)AllocRaster(width, height);
        if (!b->bm.Planes[p]) {
            Bob_Free(b);
            return FALSE;
        }
    }

    /* allocate mask plane (1-bit) */
    b->mask = (PLANEPTR)AllocRaster(width, height);
    if (!b->mask) {
        Bob_Free(b);
        return FALSE;
    }

    const UWORD bytesPerRow = BytesPerRowWordAligned(width);
    const ULONG planeSize = (ULONG)bytesPerRow * (ULONG)height;

    /* read RAW planes */
    {
        BPTR fh = Open((STRPTR)rawFile, MODE_OLDFILE);
        if (!fh) {
            Bob_Free(b);
            return FALSE;
        }

        for (UWORD p = 0; p < depth; p++) {
            if (Read(fh, b->bm.Planes[p], planeSize) != (LONG)planeSize) {
                Close(fh);
                Bob_Free(b);
                return FALSE;
            }
        }
        Close(fh);
    }

    /* read MASK (1-bit plane) */
    {
        BPTR fh = Open((STRPTR)maskFile, MODE_OLDFILE);
        if (!fh) {
            Bob_Free(b);
            return FALSE;
        }

        if (Read(fh, b->mask, planeSize) != (LONG)planeSize) {
            Close(fh);
            Bob_Free(b);
            return FALSE;
        }
        Close(fh);
    }

    return TRUE;
}

void Bob_Free(AmacsBob *b) {
    if (!b) {
        return;
    }

    /* free mask */
    if (b->mask) {
        FreeRaster(b->mask, b->width, b->height);
        b->mask = NULL;
    }

    /* free planes */
    for (UWORD p = 0; p < b->depth; p++) {
        if (b->bm.Planes[p]) {
            FreeRaster(b->bm.Planes[p], b->width, b->height);
            b->bm.Planes[p] = NULL;
        }
    }

    b->width = 0;
    b->height = 0;
    b->depth = 0;
}

void Bob_DrawMaskedToRastPort(const AmacsBob *b, struct RastPort *rp, WORD x, WORD y) {
    if (!b || !rp || !rp->BitMap || !b->mask) {
        return;
    }

    BltMaskBitMapRastPort((struct BitMap *)&b->bm, 0, 0, rp, x, y, b->width, b->height,
                          0xE0, /* masked copy */
                          b->mask);
    WaitBlit();
}

void Bob_DrawMaskedToScreen(const AmacsBob *b, struct Screen *screen, WORD x, WORD y) {
    if (!b || !screen) {
        return;
    }

    Bob_DrawMaskedToRastPort(b, &screen->RastPort, x, y);
}