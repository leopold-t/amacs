#include "imageHandler.h"

#include <exec/types.h>
#include <graphics/gfx.h> /* BitMap flags */
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>

BOOL LoadRawImageToScreen(const char *filename, struct Screen *screen) {
    BPTR fh;
    struct BitMap *bm;
    LONG bytesPerRow;
    LONG height;
    UWORD depth;
    BOOL interleaved;

    if (!screen) {
        return FALSE;
    }

    bm = screen->RastPort.BitMap;
    if (!bm) {
        return FALSE;
    }

    bytesPerRow = bm->BytesPerRow;
    height = screen->Height;
    depth = bm->Depth;

    if (bytesPerRow <= 0 || height <= 0 || depth == 0) {
        return FALSE;
    }

    interleaved = (bm->Flags & BMF_INTERLEAVED) ? TRUE : FALSE;

    fh = Open((STRPTR)filename, MODE_OLDFILE);
    if (!fh) {
        return FALSE;
    }

    UBYTE *rowBuf = (UBYTE *)AllocMem(bytesPerRow, MEMF_ANY);
    if (!rowBuf) {
        Close(fh);
        return FALSE;
    }

    for (UWORD p = 0; p < depth; p++) {
        if (!bm->Planes[p]) {
            FreeMem(rowBuf, bytesPerRow);
            Close(fh);
            return FALSE;
        }

        for (LONG y = 0; y < height; y++) {
            if (Read(fh, rowBuf, bytesPerRow) != bytesPerRow) {
                FreeMem(rowBuf, bytesPerRow);
                Close(fh);
                return FALSE;
            }

            UBYTE *dst = interleaved ? (bm->Planes[p] + (y * bytesPerRow * depth))
                                     : (bm->Planes[p] + (y * bytesPerRow));

            CopyMem(rowBuf, dst, bytesPerRow);
        }
    }

    FreeMem(rowBuf, bytesPerRow);
    Close(fh);
    return TRUE;
}