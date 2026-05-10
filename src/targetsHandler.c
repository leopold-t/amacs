#include "targetsHandler.h"

#include <dos/dos.h>
#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "bob.h"
#include "gfx.h"
#include "targetScoring.h"

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

#define SCR_W 320
#define SCR_H 256

#define TICKS_PER_SEC 50

#define RISE_TICKS 15
#define HOLD_TICKS (5 * TICKS_PER_SEC)
#define SLOT_TOTAL_TICKS (RISE_TICKS + HOLD_TICKS)

#define SERIES050_DELAY 0
#define SERIES100_DELAY (3 * TICKS_PER_SEC)
#define SERIES150_DELAY (6 * TICKS_PER_SEC)
#define SERIES200_DELAY (9 * TICKS_PER_SEC)
#define SERIES250_DELAY (12 * TICKS_PER_SEC)
#define SERIES300_DELAY (15 * TICKS_PER_SEC)

#define HIT_DELAY_050 10
#define HIT_DELAY_100 20
#define HIT_DELAY_150 30
#define HIT_DELAY_200 40
#define HIT_DELAY_250 50
#define HIT_DELAY_300 60

#define DEMO_TARGET_LIMIT 10
#define TARGET_RESPAWN_AFTER_KILLS 4

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

typedef struct TargetSeries {
    AmacsBob bob;
    BOOL loaded;
    BOOL visible;
    BOOL hit;
    const WORD *slotX;
    const WORD *slotY;
    WORD slotCount;
    WORD width;
    WORD height;
    WORD activeSlot;
    struct DateStamp startStamp;
    ULONG startDelayTicks;
    UWORD hitDelayTicks;
    BOOL respawnAllowed;
} TargetSeries;

static BOOL gInited = FALSE;
static BOOL gReady = FALSE;
static BOOL gPaused = FALSE;
static struct DateStamp gPauseStamp;
static ULONG gRandomState = 1;
static UWORD gSpawnedTargets = 0;
static UWORD gKilledTargets = 0;

static TargetSeries gSeries050;
static TargetSeries gSeries100;
static TargetSeries gSeries150;
static TargetSeries gSeries200;
static TargetSeries gSeries250;
static TargetSeries gSeries300;

static TargetSeries *GetSeriesByDistance(TargetDistance distance) {
    switch (distance) {
        case TARGET_DISTANCE_050:
            return &gSeries050;
        case TARGET_DISTANCE_100:
            return &gSeries100;
        case TARGET_DISTANCE_150:
            return &gSeries150;
        case TARGET_DISTANCE_200:
            return &gSeries200;
        case TARGET_DISTANCE_250:
            return &gSeries250;
        case TARGET_DISTANCE_300:
            return &gSeries300;
        default:
            return NULL;
    }
}

static void AddDateStampDelta(struct DateStamp *stamp, const struct DateStamp *delta) {
    LONG days;
    LONG minutes;
    LONG ticks;

    if (!stamp || !delta) {
        return;
    }

    days = (LONG)stamp->ds_Days + (LONG)delta->ds_Days;
    minutes = (LONG)stamp->ds_Minute + (LONG)delta->ds_Minute;
    ticks = (LONG)stamp->ds_Tick + (LONG)delta->ds_Tick;

    minutes += ticks / 3000;
    ticks %= 3000;

    days += minutes / (24L * 60L);
    minutes %= (24L * 60L);

    stamp->ds_Days = days;
    stamp->ds_Minute = minutes;
    stamp->ds_Tick = ticks;
}

static void ApplyPauseDeltaToSeries(TargetSeries *s, const struct DateStamp *delta) {
    if (!s || !s->loaded) {
        return;
    }

    AddDateStampDelta(&s->startStamp, delta);
}

static ULONG ElapsedTicks(const struct DateStamp *start) {
    struct DateStamp now;
    LONG dd;
    LONG dm;
    LONG dt;
    LONG total;

    if (gPaused) {
        now = gPauseStamp;
    } else {
        DateStamp(&now);
    }

    dd = now.ds_Days - start->ds_Days;
    dm = now.ds_Minute - start->ds_Minute;
    dt = now.ds_Tick - start->ds_Tick;

    total = dd * (24L * 60L * 60L * TICKS_PER_SEC) + dm * (60L * TICKS_PER_SEC) + dt;

    if (total < 0) {
        total = 0;
    }

    return (ULONG)total;
}

static ULONG NextRandom(void) {
    struct DateStamp now;

    DateStamp(&now);
    gRandomState ^= (ULONG)now.ds_Tick;
    gRandomState ^= ((ULONG)now.ds_Minute << 8);
    gRandomState = (gRandomState * 1103515245UL) + 12345UL;
    return gRandomState;
}

static BOOL ConsumeTargetBudget(void) {
    if (gSpawnedTargets >= DEMO_TARGET_LIMIT) {
        return FALSE;
    }

    gSpawnedTargets++;
    return TRUE;
}

static BOOL GetSlotRect(const TargetSeries *s, WORD slot, WORD *outLeft, WORD *outTop,
                        WORD *outRight, WORD *outBottom) {
    WORD left;
    WORD top;

    if (!s || slot < 0 || slot >= s->slotCount) {
        return FALSE;
    }

    left = s->slotX[slot];
    top = (WORD)(s->slotY[slot] - s->height + 1);

    if (outLeft) {
        *outLeft = left;
    }

    if (outTop) {
        *outTop = top;
    }

    if (outRight) {
        *outRight = (WORD)(left + s->width - 1);
    }

    if (outBottom) {
        *outBottom = s->slotY[slot];
    }

    return TRUE;
}

static BOOL RectsOverlap(WORD l1, WORD t1, WORD r1, WORD b1, WORD l2, WORD t2, WORD r2, WORD b2) {
    if (r1 < l2 || r2 < l1) {
        return FALSE;
    }

    if (b1 < t2 || b2 < t1) {
        return FALSE;
    }

    return TRUE;
}

static BOOL IsSlotOccupied(const TargetSeries *self, WORD slot) {
    TargetSeries *series[6] = {&gSeries050, &gSeries100, &gSeries150,
                               &gSeries200, &gSeries250, &gSeries300};
    WORD l1, t1, r1, b1;
    int i;

    if (!GetSlotRect(self, slot, &l1, &t1, &r1, &b1)) {
        return TRUE;
    }

    for (i = 0; i < 6; i++) {
        TargetSeries *other = series[i];
        WORD l2, t2, r2, b2;

        if (other == self || !other->loaded || !other->visible || other->hit) {
            continue;
        }

        if (!GetSlotRect(other, other->activeSlot, &l2, &t2, &r2, &b2)) {
            continue;
        }

        if (RectsOverlap(l1, t1, r1, b1, l2, t2, r2, b2)) {
            return TRUE;
        }
    }

    return FALSE;
}

static WORD PickRandomFreeSlot(TargetSeries *s) {
    WORD candidates[16];
    WORD count = 0;
    WORD i;

    if (!s || s->slotCount <= 0) {
        return 0;
    }

    for (i = 0; i < s->slotCount && i < 16; i++) {
        if (!IsSlotOccupied(s, i)) {
            candidates[count++] = i;
        }
    }

    if (count == 0) {
        return 0;
    }

    return candidates[NextRandom() % count];
}

static BOOL SpawnSeries(TargetSeries *s, ULONG delayTicks) {
    WORD slot;

    if (!s || !s->loaded) {
        return FALSE;
    }

    if (!ConsumeTargetBudget()) {
        s->visible = FALSE;
        s->hit = FALSE;
        s->respawnAllowed = FALSE;
        s->startDelayTicks = 0;
        return FALSE;
    }

    slot = PickRandomFreeSlot(s);
    s->activeSlot = slot;
    s->visible = TRUE;
    s->hit = FALSE;
    s->respawnAllowed = FALSE;
    s->startDelayTicks = delayTicks;
    DateStamp(&s->startStamp);
    return TRUE;
}

static TargetSeries *PickRandomFreeSeries(void) {
    TargetSeries *series[6] = {&gSeries050, &gSeries100, &gSeries150,
                               &gSeries200, &gSeries250, &gSeries300};
    TargetSeries *candidates[6];
    WORD count = 0;
    WORD i;

    for (i = 0; i < 6; i++) {
        TargetSeries *s = series[i];

        if (!s->loaded || s->visible || s->hit) {
            continue;
        }

        candidates[count++] = s;
    }

    if (count == 0) {
        return NULL;
    }

    return candidates[NextRandom() % count];
}

static BOOL SpawnRandomFreeSeries(ULONG delayTicks) {
    TargetSeries *s = PickRandomFreeSeries();

    if (!s) {
        return FALSE;
    }

    return SpawnSeries(s, delayTicks);
}

static void StartSlot(TargetSeries *s, WORD slot) {
    if (!s) {
        return;
    }

    if (slot >= s->slotCount) {
        slot = 0;
    }

    s->activeSlot = slot;
    s->visible = TRUE;
    s->hit = FALSE;
    s->respawnAllowed = FALSE;
    s->startDelayTicks = 0;
    DateStamp(&s->startStamp);
}

static void InitSeries(TargetSeries *s, const WORD *x, const WORD *y, WORD count, WORD w, WORD h,
                       ULONG delay, UWORD hitDelayTicks) {
    s->loaded = FALSE;
    s->visible = FALSE;
    s->hit = FALSE;
    s->slotX = x;
    s->slotY = y;
    s->slotCount = count;
    s->width = w;
    s->height = h;
    s->startDelayTicks = delay;
    s->hitDelayTicks = hitDelayTicks;
    s->respawnAllowed = FALSE;
    s->activeSlot = 0;
    DateStamp(&s->startStamp);
}

static void BltMaskClipped(const struct BitMap *bm, PLANEPTR mask, struct RastPort *rp, WORD sx,
                           WORD sy, WORD dx, WORD dy, WORD w, WORD h) {
    if (!bm || !mask || !rp || !rp->BitMap) {
        return;
    }

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

    if ((dx + w) > SCR_W) {
        w = SCR_W - dx;
    }

    if ((dy + h) > SCR_H) {
        h = SCR_H - dy;
    }

    if (w <= 0 || h <= 0) {
        return;
    }

    BltMaskBitMapRastPort((struct BitMap *)bm, sx, sy, rp, dx, dy, w, h, 0xE0, mask);
    WaitBlit();
}

static BOOL GetSeriesVisibleRect(TargetSeries *s, WORD *outX, WORD *outY, WORD *outW, WORD *outH) {
    ULONG t;
    WORD left;
    WORD bottom;
    WORD risePx;
    WORD dstY;
    WORD visibleH;

    if (!s->loaded || !s->visible || s->hit) {
        return FALSE;
    }

    t = ElapsedTicks(&s->startStamp);

    if (t < s->startDelayTicks) {
        return FALSE;
    }

    t -= s->startDelayTicks;

    if (t >= SLOT_TOTAL_TICKS) {
        t = SLOT_TOTAL_TICKS - 1;
    }

    left = s->slotX[s->activeSlot];
    bottom = s->slotY[s->activeSlot];
    risePx = (t >= RISE_TICKS) ? s->height : (WORD)((t * s->height) / RISE_TICKS);

    if (risePx <= 0) {
        return FALSE;
    }

    dstY = (bottom + 1) - risePx;
    visibleH = bottom - dstY + 1;

    if (visibleH > s->height) {
        visibleH = s->height;
    }

    *outX = left;
    *outY = dstY;
    *outW = s->width;
    *outH = visibleH;
    return TRUE;
}

static BOOL IsMaskBitSet(PLANEPTR mask, WORD width, WORD x, WORD y) {
    UWORD wordsPerRow;
    UWORD maskWord;
    UWORD bitMask;

    if (!mask || x < 0 || y < 0 || x >= width) {
        return FALSE;
    }

    wordsPerRow = (UWORD)((width + 15) >> 4);
    maskWord = ((UWORD *)mask)[(y * wordsPerRow) + ((UWORD)x >> 4)];
    bitMask = (UWORD)(0x8000 >> (x & 15));

    return (maskWord & bitMask) ? TRUE : FALSE;
}

static UWORD GetDistanceForSeries(const TargetSeries *s) {
    if (s == &gSeries050) {
        return 50;
    }

    if (s == &gSeries100) {
        return 100;
    }

    if (s == &gSeries150) {
        return 150;
    }

    if (s == &gSeries200) {
        return 200;
    }

    if (s == &gSeries250) {
        return 250;
    }

    if (s == &gSeries300) {
        return 300;
    }

    return 0;
}

static BOOL CheckSeriesHit(TargetSeries *s, WORD x, WORD y, UBYTE *hitScore) {
    WORD left;
    WORD top;
    WORD width;
    WORD visibleH;
    WORD localX;
    WORD localY;
    UWORD distance;
    BYTE zeroOffsetY;
    WORD bulletY;

    if (!GetSeriesVisibleRect(s, &left, &top, &width, &visibleH)) {
        return FALSE;
    }

    distance = (UWORD)GetDistanceForSeries(s);
    zeroOffsetY = TargetScoring_GetZeroOffset(distance);

    /*
     * Apply zeroing before hit detection, not during scoring.
     * Positive offset means the bullet goes up on screen, so Y decreases.
     * If this moves the bullet outside the target/mask, the shot is a miss.
     */
    bulletY = (WORD)(y - zeroOffsetY);

    if (x < left || x >= (left + width) || bulletY < top || bulletY >= (top + visibleH)) {
        return FALSE;
    }

    localX = (WORD)(x - left);
    localY = (WORD)(bulletY - top);

    if (!IsMaskBitSet(s->bob.mask, s->width, localX, localY)) {
        return FALSE;
    }

    if (hitScore) {
        *hitScore = TargetScoring_GetScore(distance, localX, localY);
    }

    gKilledTargets++;

    s->hit = TRUE;
    s->visible = FALSE;
    s->respawnAllowed = (gKilledTargets >= TARGET_RESPAWN_AFTER_KILLS) ? TRUE : FALSE;
    DateStamp(&s->startStamp);
    return TRUE;
}

static void TickSeries(TargetSeries *s) {
    ULONG t;

    if (!s || !s->loaded) {
        return;
    }

    t = ElapsedTicks(&s->startStamp);

    if (s->hit) {
        if (t >= s->hitDelayTicks) {
            BOOL shouldRespawn = s->respawnAllowed;

            s->hit = FALSE;
            s->visible = FALSE;
            s->respawnAllowed = FALSE;

            if (shouldRespawn) {
                SpawnRandomFreeSeries(0);
            }
        }

        return;
    }

    if (!s->visible) {
        return;
    }

    if (t < s->startDelayTicks) {
        return;
    }

    /* Once spawned, the target stays in its slot until hit. */
}

static void DrawSeries(TargetSeries *s, struct RastPort *rp) {
    WORD left;
    WORD dstY;
    WORD visibleW;
    WORD visibleH;

    if (!GetSeriesVisibleRect(s, &left, &dstY, &visibleW, &visibleH)) {
        return;
    }

    BltMaskClipped(&s->bob.bm, s->bob.mask, rp, 0, 0, left, dstY, visibleW, visibleH);
}

BOOL TargetsHandler_Init(void) {
    if (gInited) {
        gPaused = FALSE;
        gPauseStamp.ds_Days = 0;
        gPauseStamp.ds_Minute = 0;
        gPauseStamp.ds_Tick = 0;
        TargetsHandler_Reset();
        return TRUE;
    }
    gInited = TRUE;

    InitSeries(&gSeries050, gSlot050X, gSlot050Y, SLOT050_COUNT, T050_W, T050_H, SERIES050_DELAY,
               HIT_DELAY_050);
    InitSeries(&gSeries100, gSlot100X, gSlot100Y, SLOT100_COUNT, T100_W, T100_H, SERIES100_DELAY,
               HIT_DELAY_100);
    InitSeries(&gSeries150, gSlot150X, gSlot150Y, SLOT150_COUNT, T150_W, T150_H, SERIES150_DELAY,
               HIT_DELAY_150);
    InitSeries(&gSeries200, gSlot200X, gSlot200Y, SLOT200_COUNT, T200_W, T200_H, SERIES200_DELAY,
               HIT_DELAY_200);
    InitSeries(&gSeries250, gSlot250X, gSlot250Y, SLOT250_COUNT, T250_W, T250_H, SERIES250_DELAY,
               HIT_DELAY_250);
    InitSeries(&gSeries300, gSlot300X, gSlot300Y, SLOT300_COUNT, T300_W, T300_H, SERIES300_DELAY,
               HIT_DELAY_300);

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

    gPaused = FALSE;
    gPauseStamp.ds_Days = 0;
    gPauseStamp.ds_Minute = 0;
    gPauseStamp.ds_Tick = 0;
    gSpawnedTargets = 0;
    gKilledTargets = 0;
    NextRandom();
    SpawnSeries(&gSeries050, SERIES050_DELAY);
    SpawnSeries(&gSeries100, SERIES100_DELAY);
    SpawnSeries(&gSeries150, SERIES150_DELAY);
    SpawnSeries(&gSeries200, SERIES200_DELAY);
    SpawnSeries(&gSeries250, SERIES250_DELAY);
    SpawnSeries(&gSeries300, SERIES300_DELAY);
    gReady = TRUE;
    return TRUE;
}

void TargetsHandler_Shutdown(void) {
    if (!gInited) {
        return;
    }

    if (gSeries050.loaded) {
        Bob_Free(&gSeries050.bob);
    }

    if (gSeries100.loaded) {
        Bob_Free(&gSeries100.bob);
    }

    if (gSeries150.loaded) {
        Bob_Free(&gSeries150.bob);
    }

    if (gSeries200.loaded) {
        Bob_Free(&gSeries200.bob);
    }

    if (gSeries250.loaded) {
        Bob_Free(&gSeries250.bob);
    }

    if (gSeries300.loaded) {
        Bob_Free(&gSeries300.bob);
    }

    gInited = FALSE;
    gReady = FALSE;
    gPaused = FALSE;
    gKilledTargets = 0;
}

void TargetsHandler_Reset(void) {
    if (!gInited) {
        return;
    }

    gPaused = FALSE;
    gPauseStamp.ds_Days = 0;
    gPauseStamp.ds_Minute = 0;
    gPauseStamp.ds_Tick = 0;

    gSpawnedTargets = 0;
    gKilledTargets = 0;
    NextRandom();

    SpawnSeries(&gSeries050, SERIES050_DELAY);
    SpawnSeries(&gSeries100, SERIES100_DELAY);
    SpawnSeries(&gSeries150, SERIES150_DELAY);
    SpawnSeries(&gSeries200, SERIES200_DELAY);
    SpawnSeries(&gSeries250, SERIES250_DELAY);
    SpawnSeries(&gSeries300, SERIES300_DELAY);
}

void TargetsHandler_ToggleSlot(UWORD slot) {
    (void)slot;
}

UWORD TargetsHandler_GetSlotCount(TargetDistance distance) {
    TargetSeries *series = GetSeriesByDistance(distance);

    if (!series) {
        return 0;
    }

    return (UWORD)series->slotCount;
}

BOOL TargetsHandler_SelectSlot(TargetDistance distance, UWORD slotIndex) {
    TargetSeries *series = GetSeriesByDistance(distance);

    if (!gReady || !series || !series->loaded) {
        return FALSE;
    }

    StartSlot(series, (WORD)slotIndex);
    series->startDelayTicks = 0;
    return TRUE;
}

BOOL TargetsHandler_GetTargetInfo(TargetDistance distance, TargetInfo *outInfo) {
    TargetSeries *series = GetSeriesByDistance(distance);
    WORD x = 0;
    WORD y = 0;
    WORD w = 0;
    WORD h = 0;

    if (!outInfo) {
        return FALSE;
    }

    outInfo->loaded = FALSE;
    outInfo->visible = FALSE;
    outInfo->hit = FALSE;
    outInfo->distance = (UWORD)distance;
    outInfo->slotIndex = 0;
    outInfo->slotCount = 0;
    outInfo->x = 0;
    outInfo->y = 0;
    outInfo->width = 0;
    outInfo->height = 0;
    outInfo->hitDelayTicks = 0;

    if (!series) {
        return FALSE;
    }

    outInfo->loaded = series->loaded;
    outInfo->visible = series->visible;
    outInfo->hit = series->hit;
    outInfo->slotIndex = (UWORD)series->activeSlot;
    outInfo->slotCount = (UWORD)series->slotCount;
    outInfo->width = series->width;
    outInfo->height = series->height;
    outInfo->hitDelayTicks = series->hitDelayTicks;

    if (GetSeriesVisibleRect(series, &x, &y, &w, &h)) {
        outInfo->x = x;
        outInfo->y = y;
        outInfo->width = w;
        outInfo->height = h;
    }

    return TRUE;
}

void TargetsHandler_Tick(void) {
    if (!gReady || gPaused) {
        return;
    }

    TickSeries(&gSeries050);
    TickSeries(&gSeries100);
    TickSeries(&gSeries150);
    TickSeries(&gSeries200);
    TickSeries(&gSeries250);
    TickSeries(&gSeries300);
}

void TargetsHandler_Draw(struct RastPort *rp) {
    if (!gReady || !rp) {
        return;
    }

    DrawSeries(&gSeries300, rp);
    DrawSeries(&gSeries250, rp);
    DrawSeries(&gSeries200, rp);
    DrawSeries(&gSeries150, rp);
    DrawSeries(&gSeries100, rp);
    DrawSeries(&gSeries050, rp);
}

void TargetsHandler_SetPaused(BOOL paused) {
    struct DateStamp now;
    struct DateStamp delta;

    if (!gReady || paused == gPaused) {
        return;
    }

    if (paused) {
        DateStamp(&gPauseStamp);
        gPaused = TRUE;
        return;
    }

    DateStamp(&now);
    delta.ds_Days = now.ds_Days - gPauseStamp.ds_Days;
    delta.ds_Minute = now.ds_Minute - gPauseStamp.ds_Minute;
    delta.ds_Tick = now.ds_Tick - gPauseStamp.ds_Tick;

    while (delta.ds_Tick < 0) {
        delta.ds_Tick += 3000;
        delta.ds_Minute--;
    }

    while (delta.ds_Minute < 0) {
        delta.ds_Minute += 24L * 60L;
        delta.ds_Days--;
    }

    ApplyPauseDeltaToSeries(&gSeries050, &delta);
    ApplyPauseDeltaToSeries(&gSeries100, &delta);
    ApplyPauseDeltaToSeries(&gSeries150, &delta);
    ApplyPauseDeltaToSeries(&gSeries200, &delta);
    ApplyPauseDeltaToSeries(&gSeries250, &delta);
    ApplyPauseDeltaToSeries(&gSeries300, &delta);

    gPaused = FALSE;
}

BOOL TargetsHandler_CheckHit(WORD x, WORD y, UWORD *hitDelayTicks, UBYTE *hitScore) {
    if (!gReady) {
        return FALSE;
    }

    if (CheckSeriesHit(&gSeries050, x, y, hitScore)) {
        if (hitDelayTicks) {
            *hitDelayTicks = gSeries050.hitDelayTicks;
        }

        return TRUE;
    }

    if (CheckSeriesHit(&gSeries100, x, y, hitScore)) {
        if (hitDelayTicks) {
            *hitDelayTicks = gSeries100.hitDelayTicks;
        }

        return TRUE;
    }

    if (CheckSeriesHit(&gSeries150, x, y, hitScore)) {
        if (hitDelayTicks) {
            *hitDelayTicks = gSeries150.hitDelayTicks;
        }

        return TRUE;
    }

    if (CheckSeriesHit(&gSeries200, x, y, hitScore)) {
        if (hitDelayTicks) {
            *hitDelayTicks = gSeries200.hitDelayTicks;
        }

        return TRUE;
    }

    if (CheckSeriesHit(&gSeries250, x, y, hitScore)) {
        if (hitDelayTicks) {
            *hitDelayTicks = gSeries250.hitDelayTicks;
        }

        return TRUE;
    }

    if (CheckSeriesHit(&gSeries300, x, y, hitScore)) {
        if (hitDelayTicks) {
            *hitDelayTicks = gSeries300.hitDelayTicks;
        }

        return TRUE;
    }

    return FALSE;
}