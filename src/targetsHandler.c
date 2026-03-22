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

#define TARGET100_RAW "gfx/Target100.raw"
#define TARGET100_MASK "gfx/Target100.mask"

#define TARGET150_RAW "gfx/Target150.raw"
#define TARGET150_MASK "gfx/Target150.mask"

#define TARGET200_RAW "gfx/Target200.raw"
#define TARGET200_MASK "gfx/Target200.mask"

#define TARGET250_RAW "gfx/Target250.raw"
#define TARGET250_MASK "gfx/Target250.mask"

#define TARGET300_RAW "gfx/Target300.raw"
#define TARGET300_MASK "gfx/Target300.mask"

/* ---- Dimensions ---- */
#define T050_W 48
#define T050_H 23

#define T100_W 24
#define T100_H 11

#define T150_W 9
#define T150_H 17

#define T200_W 8
#define T200_H 15

#define T250_W 7
#define T250_H 14

#define T300_W 5
#define T300_H 10

/* Screen */
#define SCR_W 320
#define SCR_H 256

#define TICKS_PER_SEC 50

#define RISE_TICKS 15
#define HOLD_TICKS (5 * TICKS_PER_SEC)
#define SLOT_TOTAL_TICKS (RISE_TICKS + HOLD_TICKS)

/* Delays */
#define SERIES100_DELAY (3 * TICKS_PER_SEC)
#define SERIES150_DELAY (6 * TICKS_PER_SEC)
#define SERIES200_DELAY (9 * TICKS_PER_SEC)
#define SERIES250_DELAY (12 * TICKS_PER_SEC)
#define SERIES300_DELAY (15 * TICKS_PER_SEC)

/* ---- Slots ---- */
#define SLOT050_COUNT 5
static const WORD gSlot050X[SLOT050_COUNT] = {17, 77, 136, 195, 255};
static const WORD gSlot050Y[SLOT050_COUNT] = {215, 215, 215, 215, 215};

#define SLOT100_COUNT 9
static const WORD gSlot100X[SLOT100_COUNT] = {0, 36, 73, 110, 147, 184, 221, 258, 295};
static const WORD gSlot100Y[SLOT100_COUNT] = {167, 167, 167, 167, 167, 167, 167, 167, 167};

#define SLOT150_COUNT 9
static const WORD gSlot150X[SLOT150_COUNT] = {49, 75, 101, 128, 155, 181, 208, 234, 260};
static const WORD gSlot150Y[SLOT150_COUNT] = {145, 145, 145, 145, 145, 145, 145, 145, 145};

#define SLOT200_COUNT 9
static const WORD gSlot200X[SLOT200_COUNT] = {71, 92, 113, 134, 155, 176, 197, 218, 239};
static const WORD gSlot200Y[SLOT200_COUNT] = {133, 133, 133, 133, 133, 133, 133, 133, 133};

#define SLOT250_COUNT 9
static const WORD gSlot250X[SLOT250_COUNT] = {87, 104, 121, 138, 155, 173, 190, 207, 224};
static const WORD gSlot250Y[SLOT250_COUNT] = {125, 125, 125, 125, 125, 125, 125, 125, 125};

#define SLOT300_COUNT 9
static const WORD gSlot300X[SLOT300_COUNT] = {99, 114, 128, 142, 156, 171, 185, 199, 214};
static const WORD gSlot300Y[SLOT300_COUNT] = {119, 119, 119, 119, 119, 119, 119, 119, 119};

/* ------------------------------------------------------------------ */

typedef struct TargetSeries {
    AmacsBob bob;
    BOOL loaded;

    const WORD *slotX;
    const WORD *slotY;
    WORD slotCount;

    WORD width;
    WORD height;

    WORD activeSlot;

    struct DateStamp startStamp;
    ULONG startDelayTicks;
} TargetSeries;

static BOOL gInited = FALSE;
static BOOL gReady = FALSE;

static TargetSeries gSeries050;
static TargetSeries gSeries100;
static TargetSeries gSeries150;
static TargetSeries gSeries200;
static TargetSeries gSeries250;
static TargetSeries gSeries300;

/* ------------------------------------------------------------------ */

static ULONG ElapsedTicks(const struct DateStamp *start) {
    struct DateStamp now;
    DateStamp(&now);

    LONG dd = now.ds_Days - start->ds_Days;
    LONG dm = now.ds_Minute - start->ds_Minute;
    LONG dt = now.ds_Tick - start->ds_Tick;

    LONG total = dd * (24L * 60L * 60L * TICKS_PER_SEC) + dm * (60L * TICKS_PER_SEC) + dt;

    if (total < 0)
        total = 0;
    return (ULONG)total;
}

static void StartSlot(TargetSeries *s, WORD slot) {
    if (slot >= s->slotCount)
        slot = 0;
    s->activeSlot = slot;
    DateStamp(&s->startStamp);
}

static void InitSeries(TargetSeries *s, const WORD *x, const WORD *y, WORD count, WORD w, WORD h,
                       ULONG delay) {
    s->loaded = FALSE;
    s->slotX = x;
    s->slotY = y;
    s->slotCount = count;
    s->width = w;
    s->height = h;
    s->startDelayTicks = delay;
    s->activeSlot = 0;
    DateStamp(&s->startStamp);
}

static void BltMaskClipped(const struct BitMap *bm, PLANEPTR mask, struct RastPort *rp, WORD sx,
                           WORD sy, WORD dx, WORD dy, WORD w, WORD h) {
    if (!bm || !mask || !rp || !rp->BitMap)
        return;

    if (dx < 0) {
        sx -= dx;
        w += dx;
        dx = 0;
    }
    if (dy < 0) {
        sy -= dy;
        h += dy;
        dy = 0;
    }
    if ((dx + w) > SCR_W)
        w = SCR_W - dx;
    if ((dy + h) > SCR_H)
        h = SCR_H - dy;

    if (w <= 0 || h <= 0)
        return;

    BltMaskBitMapRastPort((struct BitMap *)bm, sx, sy, rp, dx, dy, w, h, 0xE0, mask);
    WaitBlit();
}

static void TickSeries(TargetSeries *s) {
    if (!s->loaded)
        return;

    ULONG t = ElapsedTicks(&s->startStamp);

    if (t < s->startDelayTicks)
        return;

    t -= s->startDelayTicks;

    if (t >= SLOT_TOTAL_TICKS) {
        StartSlot(s, s->activeSlot + 1);
        s->startDelayTicks = 0;
    }
}

static void DrawSeries(TargetSeries *s, struct RastPort *rp) {
    if (!s->loaded)
        return;

    ULONG t = ElapsedTicks(&s->startStamp);

    if (t < s->startDelayTicks)
        return;

    t -= s->startDelayTicks;

    if (t >= SLOT_TOTAL_TICKS)
        return;

    WORD left = s->slotX[s->activeSlot];
    WORD bottom = s->slotY[s->activeSlot];

    WORD risePx = (t >= RISE_TICKS) ? s->height : (WORD)((t * s->height) / RISE_TICKS);

    if (risePx <= 0)
        return;

    WORD dstY = (bottom + 1) - risePx;

    WORD visibleH = bottom - dstY + 1;
    if (visibleH > s->height)
        visibleH = s->height;

    BltMaskClipped(&s->bob.bm, s->bob.mask, rp, 0, 0, left, dstY, s->width, visibleH);
}

/* ------------------------------------------------------------------ */

BOOL TargetsHandler_Init(void) {
    if (gInited)
        return TRUE;
    gInited = TRUE;

    InitSeries(&gSeries050, gSlot050X, gSlot050Y, SLOT050_COUNT, T050_W, T050_H, 0);
    InitSeries(&gSeries100, gSlot100X, gSlot100Y, SLOT100_COUNT, T100_W, T100_H, SERIES100_DELAY);
    InitSeries(&gSeries150, gSlot150X, gSlot150Y, SLOT150_COUNT, T150_W, T150_H, SERIES150_DELAY);
    InitSeries(&gSeries200, gSlot200X, gSlot200Y, SLOT200_COUNT, T200_W, T200_H, SERIES200_DELAY);
    InitSeries(&gSeries250, gSlot250X, gSlot250Y, SLOT250_COUNT, T250_W, T250_H, SERIES250_DELAY);
    InitSeries(&gSeries300, gSlot300X, gSlot300Y, SLOT300_COUNT, T300_W, T300_H, SERIES300_DELAY);

    if (Bob_LoadRawAndMask(&gSeries050.bob, TARGET050_RAW, TARGET050_MASK, T050_W, T050_H, 5)) {
        gSeries050.loaded = TRUE;
        StartSlot(&gSeries050, 0);
    }
    if (Bob_LoadRawAndMask(&gSeries100.bob, TARGET100_RAW, TARGET100_MASK, T100_W, T100_H, 5)) {
        gSeries100.loaded = TRUE;
        StartSlot(&gSeries100, 0);
    }
    if (Bob_LoadRawAndMask(&gSeries150.bob, TARGET150_RAW, TARGET150_MASK, T150_W, T150_H, 5)) {
        gSeries150.loaded = TRUE;
        StartSlot(&gSeries150, 0);
    }
    if (Bob_LoadRawAndMask(&gSeries200.bob, TARGET200_RAW, TARGET200_MASK, T200_W, T200_H, 5)) {
        gSeries200.loaded = TRUE;
        StartSlot(&gSeries200, 0);
    }
    if (Bob_LoadRawAndMask(&gSeries250.bob, TARGET250_RAW, TARGET250_MASK, T250_W, T250_H, 5)) {
        gSeries250.loaded = TRUE;
        StartSlot(&gSeries250, 0);
    }
    if (Bob_LoadRawAndMask(&gSeries300.bob, TARGET300_RAW, TARGET300_MASK, T300_W, T300_H, 5)) {
        gSeries300.loaded = TRUE;
        StartSlot(&gSeries300, 0);
    }

    gReady = TRUE;
    return TRUE;
}

void TargetsHandler_Shutdown(void) {
    if (!gInited)
        return;

    if (gSeries050.loaded)
        Bob_Free(&gSeries050.bob);
    if (gSeries100.loaded)
        Bob_Free(&gSeries100.bob);
    if (gSeries150.loaded)
        Bob_Free(&gSeries150.bob);
    if (gSeries200.loaded)
        Bob_Free(&gSeries200.bob);
    if (gSeries250.loaded)
        Bob_Free(&gSeries250.bob);
    if (gSeries300.loaded)
        Bob_Free(&gSeries300.bob);

    gInited = FALSE;
    gReady = FALSE;
}

void TargetsHandler_ToggleSlot(UWORD slot) {
    (void)slot;
}

void TargetsHandler_Tick(void) {
    if (!gReady)
        return;

    TickSeries(&gSeries050);
    TickSeries(&gSeries100);
    TickSeries(&gSeries150);
    TickSeries(&gSeries200);
    TickSeries(&gSeries250);
    TickSeries(&gSeries300);
}

void TargetsHandler_Draw(struct RastPort *rp) {
    if (!gReady || !rp)
        return;

    /* far -> near */
    DrawSeries(&gSeries300, rp);
    DrawSeries(&gSeries250, rp);
    DrawSeries(&gSeries200, rp);
    DrawSeries(&gSeries150, rp);
    DrawSeries(&gSeries100, rp);
    DrawSeries(&gSeries050, rp);
}