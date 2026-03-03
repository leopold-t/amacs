#include "targetsHandler.h"

#include <dos/dos.h>
#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "bob.h"
#include "gfx.h"

/* ---- Assets ---- */
#define TARGET050_RAW "gfx/Target050.raw"
#define TARGET050_MASK "gfx/Target050.mask"

/* ---- Target050 dimensions ---- */
#define T050_W 48
#define T050_H 23

/* Screen */
#define SCR_W 320
#define SCR_H 256

/* Slots (left-bottom corner positions) */
#define SLOT_COUNT 5
static const WORD gSlotX[SLOT_COUNT] = {17, 77, 136, 195, 255};
static const WORD gSlotY[SLOT_COUNT] = {215, 215, 215, 215, 215};

/*
 * Timing:
 * We use DateStamp(), which gives:
 * - ds_Days
 * - ds_Minute (minutes since midnight)
 * - ds_Tick   (ticks since last minute; tick rate is 50 Hz in DOS timebase)
 *
 * So:
 * 5 seconds  = 250 ticks
 * 0.30 sec   = 15 ticks
 */
#define TICKS_PER_SEC 50
#define SLOT_VISIBLE_TICKS (5 * TICKS_PER_SEC) /* 5s */
#define RISE_TICKS 15                          /* 0.30s at 50Hz */

/* ------------------------------------------------------------------ */

static BOOL gInited = FALSE;
static BOOL gReady = FALSE;

static AmacsBob gTarget050;

static WORD gActiveSlot = 0;

/* Start time of current slot */
static struct DateStamp gStartStamp;

/* ------------------------------------------------------------------ */

static void StartSlot(WORD slot) {
    if (slot < 0)
        slot = 0;
    if (slot >= SLOT_COUNT)
        slot = 0;

    gActiveSlot = slot;
    DateStamp(&gStartStamp);
}

/* returns elapsed ticks since gStartStamp (clamped to >= 0) */
static ULONG ElapsedTicks(void) {
    struct DateStamp now;
    DateStamp(&now);

    LONG dd = (LONG)now.ds_Days - (LONG)gStartStamp.ds_Days;
    LONG dm = (LONG)now.ds_Minute - (LONG)gStartStamp.ds_Minute;
    LONG dt = (LONG)now.ds_Tick - (LONG)gStartStamp.ds_Tick;

    /* total ticks = days + minutes + ticks */
    LONG total =
        dd * (24L * 60L * 60L * (LONG)TICKS_PER_SEC) + dm * (60L * (LONG)TICKS_PER_SEC) + dt;

    if (total < 0)
        total = 0;

    return (ULONG)total;
}

/*
 * Masked blit with:
 * - source offsets (srcX, srcY)
 * - destination coords (dstX, dstY)
 * - width/height (w,h)
 * - manual clipping against screen to avoid wraparound artifacts
 */
static void BltMaskClippedSrc(const struct BitMap *srcBm, PLANEPTR maskPlane,
                              struct RastPort *dstRP, WORD srcX, WORD srcY, WORD dstX, WORD dstY,
                              WORD w, WORD h) {
    WORD sx = srcX;
    WORD sy = srcY;
    WORD dx = dstX;
    WORD dy = dstY;
    WORD cw = w;
    WORD ch = h;

    if (!srcBm || !maskPlane || !dstRP || !dstRP->BitMap)
        return;

    /* Clip left */
    if (dx < 0) {
        WORD cut = (WORD)(-dx);
        sx = (WORD)(sx + cut);
        cw = (WORD)(cw - cut);
        dx = 0;
    }
    /* Clip top */
    if (dy < 0) {
        WORD cut = (WORD)(-dy);
        sy = (WORD)(sy + cut);
        ch = (WORD)(ch - cut);
        dy = 0;
    }
    /* Clip right */
    if ((dx + cw) > SCR_W) {
        cw = (WORD)(SCR_W - dx);
    }
    /* Clip bottom */
    if ((dy + ch) > SCR_H) {
        ch = (WORD)(SCR_H - dy);
    }

    if (cw <= 0 || ch <= 0)
        return;

    BltMaskBitMapRastPort((struct BitMap *)srcBm, sx, sy, dstRP, dx, dy, cw, ch, 0xE0, maskPlane);
    WaitBlit();
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

BOOL TargetsHandler_Init(void) {
    if (gInited)
        return TRUE;

    gInited = TRUE;
    gReady = FALSE;

    /* Load the target bob */
    if (!Bob_LoadRawAndMask(&gTarget050, TARGET050_RAW, TARGET050_MASK, T050_W, T050_H, 5)) {
        /* Non-fatal: keep range running, just disable targets */
        return TRUE;
    }

    gReady = TRUE;
    StartSlot(0);
    return TRUE;
}

void TargetsHandler_Shutdown(void) {
    if (!gInited)
        return;

    if (gReady) {
        Bob_Free(&gTarget050);
    }

    gReady = FALSE;
    gInited = FALSE;
}

void TargetsHandler_Tick(void) {
    if (!gInited || !gReady)
        return;

    ULONG ticks = ElapsedTicks();

    /* After 5 seconds, move to next slot */
    if (ticks >= (ULONG)SLOT_VISIBLE_TICKS) {
        WORD next = (WORD)(gActiveSlot + 1);
        if (next >= SLOT_COUNT)
            next = 0;
        StartSlot(next);
    }
}

/*
 * Draw current target with "rise from ground" animation.
 * Slot coordinates are LEFT-BOTTOM corner.
 */
void TargetsHandler_Draw(struct RastPort *rp) {
    if (!gInited || !gReady || !rp)
        return;

    ULONG ticks = ElapsedTicks();

    /* Rise progress 0..T050_H */
    WORD risePx;
    if (ticks >= (ULONG)RISE_TICKS) {
        risePx = T050_H;
    } else {
        /* linear rise over RISE_TICKS */
        risePx = (WORD)((ticks * (ULONG)T050_H) / (ULONG)RISE_TICKS);
        if (risePx < 0)
            risePx = 0;
        if (risePx > T050_H)
            risePx = T050_H;
    }

    if (risePx <= 0)
        return;

    WORD left = gSlotX[gActiveSlot];
    WORD bottom = gSlotY[gActiveSlot];

    /* We draw only the bottom 'risePx' lines */
    WORD drawH = risePx;
    WORD srcY = (WORD)(T050_H - drawH);

    /* Convert LEFT-BOTTOM to TOP-LEFT destination */
    WORD dstY = (WORD)(bottom - drawH + 1);

    /* Blit subset: src (0, srcY, T050_W, drawH) -> dst (left, dstY) */
    BltMaskClippedSrc(&gTarget050.bm, gTarget050.mask, rp, 0, srcY, left, dstY, T050_W, drawH);
}