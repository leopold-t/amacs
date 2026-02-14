#include "imageHandler.h"

#include <exec/types.h>
#include <graphics/gfx.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>

/*
 * Robust RAW loader:
 * - Reads RAW as planar, plane-by-plane (contiguous planes).
 * - Uses a temporary planar BitMap built from AllocRaster() planes.
 * - Copies to the screen BitMap via BltBitMap(), which correctly handles
 *   the destination layout (interleaved/modulo/etc.).
 *
 * This avoids A600 artifacts caused by writing directly into screen->BitMap planes.
 */

static BOOL ReadRawIntoPlanarBitMap(const char *filename, struct BitMap *bm, UWORD width,
                                    UWORD height, UWORD depth) {
    BPTR fh = Open((STRPTR)filename, MODE_OLDFILE);
    if (!fh) {
        return FALSE;
    }

    /* Bytes per row for planar raster (must be word-aligned to 16 pixels) */
    const UWORD bytesPerRow = (UWORD)(((width + 15) >> 4) << 1);
    const ULONG planeSize = (ULONG)bytesPerRow * (ULONG)height;

    for (UWORD p = 0; p < depth; p++) {
        if (!bm->Planes[p]) {
            Close(fh);
            return FALSE;
        }

        if (Read(fh, bm->Planes[p], planeSize) != (LONG)planeSize) {
            Close(fh);
            return FALSE;
        }
    }

    Close(fh);
    return TRUE;
}

BOOL LoadRawImageToScreen(const char *filename, struct Screen *screen) {
    if (!screen || !screen->RastPort.BitMap) {
        return FALSE;
    }

    struct BitMap *dst = screen->RastPort.BitMap;

    const UWORD width = (UWORD)screen->Width;
    const UWORD height = (UWORD)screen->Height;
    const UWORD depth = (UWORD)dst->Depth;

    /* Build a guaranteed planar (non-interleaved) temporary bitmap */
    struct BitMap tmp;
    InitBitMap(&tmp, depth, width, height);

    /* Allocate chip rasters for each plane */
    for (UWORD p = 0; p < depth; p++) {
        tmp.Planes[p] = (PLANEPTR)AllocRaster(width, height);
        if (!tmp.Planes[p]) {
            /* Cleanup already allocated planes */
            for (UWORD q = 0; q < p; q++) {
                if (tmp.Planes[q]) {
                    FreeRaster(tmp.Planes[q], width, height);
                    tmp.Planes[q] = NULL;
                }
            }
            return FALSE;
        }
    }

    /* Read RAW data into the temporary planar bitmap */
    if (!ReadRawIntoPlanarBitMap(filename, &tmp, width, height, depth)) {
        for (UWORD p = 0; p < depth; p++) {
            if (tmp.Planes[p]) {
                FreeRaster(tmp.Planes[p], width, height);
                tmp.Planes[p] = NULL;
            }
        }
        return FALSE;
    }

    /* Blit from temp bitmap to the screen bitmap */
    WaitBlit();
    BltBitMap(&tmp, 0, 0, dst, 0, 0, width, height, 0xC0, 0xFF, NULL);
    WaitBlit();

    /* Free temporary planes */
    for (UWORD p = 0; p < depth; p++) {
        if (tmp.Planes[p]) {
            FreeRaster(tmp.Planes[p], width, height);
            tmp.Planes[p] = NULL;
        }
    }

    return TRUE;
}