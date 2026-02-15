#ifndef BOB_H
#define BOB_H

#include <exec/types.h>
#include <graphics/gfx.h>
#include <intuition/screens.h>

typedef struct AmacsBob {
    UWORD width;
    UWORD height;
    UWORD depth;      /* must match screen depth (e.g., 5 for 32 colors) */
    struct BitMap bm; /* planar bitmap (owned planes) */
    PLANEPTR mask;    /* 1-bit mask plane (AllocRaster) */
} AmacsBob;

/* Loads planar RAW into bm planes (AllocRaster) and 1-bit RAW mask into mask plane. */
BOOL Bob_LoadRawAndMask(AmacsBob *b, const char *rawFile, const char *maskFile, UWORD width,
                        UWORD height, UWORD depth);

/* Frees planes and mask. Safe to call multiple times. */
void Bob_Free(AmacsBob *b);

/* Draws the Bob using its mask onto a screen RastPort at (x,y). */
void Bob_DrawMaskedToScreen(const AmacsBob *b, struct Screen *screen, WORD x, WORD y);

#endif