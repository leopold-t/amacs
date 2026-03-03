#include "sightHandler.h"

#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/graphics.h>

#include "bob.h"
#include "gfx.h"
#include "input.h"
#include "targetsHandler.h"

/* External input directions (joystick) */
extern BOOL Input_Left(void);
extern BOOL Input_Right(void);
extern BOOL Input_Up(void);
extern BOOL Input_Down(void);

/* ---- Assets ---- */
#define FRONTSIGHT_RAW "gfx/FrontSight.raw"
#define FRONTSIGHT_MASK "gfx/FrontSight.mask"

#define REARSIGHT_RAW "gfx/RearSight.raw"
#define REARSIGHT_MASK "gfx/RearSight.mask"

/* ---- Dimensions ---- */
#define FRONTSIGHT_W 83
#define FRONTSIGHT_H 79

#define REARSIGHT_W 115
#define REARSIGHT_H 115

#define SCR_W 320
#define SCR_H 256

/* Allow moving slightly off-screen (horizontal) */
#define OVERSCAN_X 41

/* Extra horizontal overscan requested for the whole optic (ring + front sight) */
#define OVERSCAN_X_EXTRA 16
#define OVERSCAN_X_TOTAL (OVERSCAN_X + OVERSCAN_X_EXTRA)

/* Extra movement down */
#define OVERSCAN_Y 16

/* Ring offset relative to front sight at REST:
   ringX = frontX + RING_OFFSET_X
   ringY = frontY + RING_OFFSET_Y
*/
#define RING_OFFSET_X (-16)
#define RING_OFFSET_Y (-51)

/* ---- "Occlusion" rectangle under the ring ----
   Coordinates are relative to ring top-left (ringX, ringY).
   This hides the part of the front sight that can appear below the ring.
   FIX: extend further left to cover the 3x14px strip at extreme bottom-left.
*/
#define OCCL_REL_X (-6)          /* was -1 */
#define OCCL_REL_Y (REARSIGHT_H) /* 115 */
#define OCCL_W 124               /* was 119 (add 5px to the left) */
#define OCCL_H 39

/* ---- Parallax lead (front sight relative to ring) ---- */
#define LEAD_MAX_PX 34
#define LEAD_MAX_FP (LEAD_MAX_PX * 256)

/* Smoothing: smaller = snappier, bigger = more floaty */
#define LEAD_FOLLOW_DIV 4  /* how fast lead follows current velocity */
#define LEAD_DECAY_NUM 210 /* decay when stopped (210/256 ~= 0.82) */
#define LEAD_DECAY_DEN 256
#define LEAD_STOP_FP (2 * 256) /* snap-to-zero threshold */

/* -------------------------------------------------------------- */

/* Helper: rectangle intersection (returns FALSE if empty) */
static BOOL IntersectRect(WORD ax, WORD ay, WORD aw, WORD ah, WORD bx, WORD by, WORD bw, WORD bh,
                          WORD *outX, WORD *outY, WORD *outW, WORD *outH) {
    WORD x1 = (ax > bx) ? ax : bx;
    WORD y1 = (ay > by) ? ay : by;
    WORD x2 = ((ax + aw) < (bx + bw)) ? (ax + aw) : (bx + bw);
    WORD y2 = ((ay + ah) < (by + bh)) ? (ay + ah) : (by + bh);

    WORD w = (WORD)(x2 - x1);
    WORD h = (WORD)(y2 - y1);

    if (w <= 0 || h <= 0)
        return FALSE;

    *outX = x1;
    *outY = y1;
    *outW = w;
    *outH = h;
    return TRUE;
}

/*
 * Manual clipping for BltMaskBitMapRastPort()
 * (prevents wraparound/ghost halves when dst is negative/off-screen)
 */
static void DrawMaskedClipped(const struct BitMap *srcBm, PLANEPTR maskPlane,
                              struct RastPort *dstRP, WORD dstX, WORD dstY, WORD srcW, WORD srcH) {
    WORD sx = 0, sy = 0;
    WORD w = srcW, h = srcH;
    WORD dx = dstX, dy = dstY;

    if (!srcBm || !maskPlane || !dstRP || !dstRP->BitMap)
        return;

    /* Left */
    if (dx < 0) {
        sx = (WORD)(-dx);
        w = (WORD)(w - sx);
        dx = 0;
    }
    /* Top */
    if (dy < 0) {
        sy = (WORD)(-dy);
        h = (WORD)(h - sy);
        dy = 0;
    }
    /* Right */
    if ((dx + w) > SCR_W)
        w = (WORD)(SCR_W - dx);
    /* Bottom */
    if ((dy + h) > SCR_H)
        h = (WORD)(SCR_H - dy);

    if (w <= 0 || h <= 0)
        return;

    BltMaskBitMapRastPort((struct BitMap *)srcBm, sx, sy, dstRP, dx, dy, w, h, 0xE0, maskPlane);
    WaitBlit();
}

/* Clamp fixed-point lead to [-LEAD_MAX_FP..+LEAD_MAX_FP] */
static LONG ClampLeadFP(LONG v) {
    if (v > LEAD_MAX_FP)
        return LEAD_MAX_FP;
    if (v < -LEAD_MAX_FP)
        return -LEAD_MAX_FP;
    return v;
}

/* -------------------------------------------------------------- */

void RunRangeWithFrontSight(BOOL useDBuf) {
    AmacsBob frontSight;
    AmacsBob rearSight;

    /* We treat RING as the base position moving on screen */
    WORD ringX = (SCR_W - REARSIGHT_W) / 2;
    WORD ringY = (SCR_H - REARSIGHT_H) / 2;

    /* Fixed-point accumulators (1/256 px units) and signed velocities for RING */
    LONG ax = 0, ay = 0;
    LONG vx = 0, vy = 0;

    /* Tuning for ring movement */
    const LONG V_MAX = 8192;
    const LONG V_MIN = 128;
    const LONG V_STOP = 32;
    const LONG ACCEL_DIV = 6;
    const LONG DECAY_NUM = 64;
    const LONG DECAY_DEN = 256;

    const UWORD START_DELAY = 3;

    static int prevDirX = 0, prevDirY = 0;
    static UWORD holdX = 0, holdY = 0;

    /* Front sight lead relative to ring (fixed point) */
    LONG leadX = 0, leadY = 0;

    struct BitMap bg;
    BOOL haveBg = FALSE;

    /* Temporary 1-bit mask for per-frame occlusion */
    PLANEPTR tempMaskPlane = NULL;
    struct BitMap maskSrcBm;
    struct BitMap maskTmpBm;
    struct RastPort maskTmpRP;

    /* ---- Load BOBs ---- */
    if (!Bob_LoadRawAndMask(&frontSight, FRONTSIGHT_RAW, FRONTSIGHT_MASK, FRONTSIGHT_W,
                            FRONTSIGHT_H, 5))
        return;

    if (!Bob_LoadRawAndMask(&rearSight, REARSIGHT_RAW, REARSIGHT_MASK, REARSIGHT_W, REARSIGHT_H,
                            5)) {
        Bob_Free(&frontSight);
        return;
    }

    /* Targets (demo cycle). Drawn between background and optic. */
    if (!TargetsHandler_Init()) {
        Bob_Free(&frontSight);
        Bob_Free(&rearSight);
        return;
    }

    /* Allocate temp mask (1-bit plane) */
    tempMaskPlane = (PLANEPTR)AllocRaster(FRONTSIGHT_W, FRONTSIGHT_H);
    if (!tempMaskPlane) {
        TargetsHandler_Shutdown();
        Bob_Free(&frontSight);
        Bob_Free(&rearSight);
        return;
    }

    /* Wrap mask planes into 1-bit bitmaps */
    InitBitMap(&maskSrcBm, 1, FRONTSIGHT_W, FRONTSIGHT_H);
    maskSrcBm.Planes[0] = frontSight.mask;

    InitBitMap(&maskTmpBm, 1, FRONTSIGHT_W, FRONTSIGHT_H);
    maskTmpBm.Planes[0] = tempMaskPlane;

    InitRastPort(&maskTmpRP);
    maskTmpRP.BitMap = &maskTmpBm;

    /* ---- Capture background ---- */
    InitBitMap(&bg, 5, SCR_W, SCR_H);

    for (UWORD p = 0; p < 5; p++) {
        bg.Planes[p] = (PLANEPTR)AllocRaster(SCR_W, SCR_H);
        if (!bg.Planes[p]) {
            for (UWORD q = 0; q < 5; q++) {
                if (bg.Planes[q]) {
                    FreeRaster(bg.Planes[q], SCR_W, SCR_H);
                    bg.Planes[q] = NULL;
                }
            }
            FreeRaster(tempMaskPlane, FRONTSIGHT_W, FRONTSIGHT_H);
            tempMaskPlane = NULL;
            TargetsHandler_Shutdown();
            Bob_Free(&frontSight);
            Bob_Free(&rearSight);
            return;
        }
    }

    {
        struct Screen *scr = Gfx_GetScreen();
        if (scr && scr->RastPort.BitMap) {
            WaitBlit();
            BltBitMap(scr->RastPort.BitMap, 0, 0, &bg, 0, 0, SCR_W, SCR_H, 0xC0, 0xFF, NULL);
            WaitBlit();
            haveBg = TRUE;
        }
    }

    prevDirX = prevDirY = 0;
    holdX = holdY = 0;
    ax = ay = 0;
    vx = vy = 0;
    leadX = leadY = 0;

    BOOL paused = FALSE;

    for (;;) {
        /* IMPORTANT: only Input_PollWindow() drains IDCMP now */
        Input_PollWindow(Gfx_GetWindow());

        /* Controls */
        if (Input_KeyPressed(0x45)) { /* ESC */
            break;
        }

        if (Input_KeyPressed(0x19)) { /* P */
            paused = (BOOL)!paused;
        }

        /* Fire is reserved for shooting (do not exit range) */
        (void)Input_FirePressed();

        if (paused) {
            WaitTOF();
            continue;
        }

        /* Advance targets once per frame (only when not paused). */
        TargetsHandler_Tick();

        int dirX = (Input_Right() ? 1 : 0) - (Input_Left() ? 1 : 0);
        int dirY = (Input_Down() ? 1 : 0) - (Input_Up() ? 1 : 0);

        /* ---------------- RING movement ---------------- */

        /* X axis */
        if (dirX != 0) {
            if (prevDirX == 0) {
                ringX += (WORD)dirX; /* tap = 1 px */
                holdX = 1;
                vx = 0;
                ax = 0;
            } else if (dirX != prevDirX) {
                ringX += (WORD)dirX;
                holdX = 1;
                vx = 0;
                ax = 0;
            } else {
                if (holdX < 0xFFFF)
                    holdX++;

                if (holdX >= START_DELAY) {
                    LONG target = (LONG)dirX * V_MAX;
                    LONG dv = target - vx;

                    vx += dv / ACCEL_DIV;

                    if (vx < V_MIN && vx > -V_MIN)
                        vx = (LONG)dirX * V_MIN;

                    ax += vx;

                    while (ax >= 256) {
                        ax -= 256;
                        ringX++;
                    }
                    while (ax <= -256) {
                        ax += 256;
                        ringX--;
                    }
                }
            }
        } else {
            holdX = 0;
            prevDirX = 0;

            vx = (vx * DECAY_NUM) / DECAY_DEN;

            if (vx < V_STOP && vx > -V_STOP)
                vx = 0;

            ax += vx;

            while (ax >= 256) {
                ax -= 256;
                ringX++;
            }
            while (ax <= -256) {
                ax += 256;
                ringX--;
            }

            if (vx == 0)
                ax = 0;
        }

        /* Y axis */
        if (dirY != 0) {
            if (prevDirY == 0) {
                ringY += (WORD)dirY;
                holdY = 1;
                vy = 0;
                ay = 0;
            } else if (dirY != prevDirY) {
                ringY += (WORD)dirY;
                holdY = 1;
                vy = 0;
                ay = 0;
            } else {
                if (holdY < 0xFFFF)
                    holdY++;

                if (holdY >= START_DELAY) {
                    LONG target = (LONG)dirY * V_MAX;
                    LONG dv = target - vy;

                    vy += dv / ACCEL_DIV;

                    if (vy < V_MIN && vy > -V_MIN)
                        vy = (LONG)dirY * V_MIN;

                    ay += vy;

                    while (ay >= 256) {
                        ay -= 256;
                        ringY++;
                    }
                    while (ay <= -256) {
                        ay += 256;
                        ringY--;
                    }
                }
            }
        } else {
            holdY = 0;
            prevDirY = 0;

            vy = (vy * DECAY_NUM) / DECAY_DEN;

            if (vy < V_STOP && vy > -V_STOP)
                vy = 0;

            ay += vy;

            while (ay >= 256) {
                ay -= 256;
                ringY++;
            }
            while (ay <= -256) {
                ay += 256;
                ringY--;
            }

            if (vy == 0)
                ay = 0;
        }

        prevDirX = dirX;
        prevDirY = dirY;

        /* Clamp ring position (ring is the base now) */
        if (ringX < -OVERSCAN_X_TOTAL)
            ringX = -OVERSCAN_X_TOTAL;
        if (ringX > SCR_W - REARSIGHT_W + OVERSCAN_X_TOTAL)
            ringX = (SCR_W - REARSIGHT_W + OVERSCAN_X_TOTAL);

        if (ringY < 0)
            ringY = 0;
        if (ringY > SCR_H - REARSIGHT_H + OVERSCAN_Y)
            ringY = (SCR_H - REARSIGHT_H + OVERSCAN_Y);

        /* ---------------- FRONT lead from ring velocity ---------------- */
        {
            LONG targetLeadX = (vx * LEAD_MAX_FP) / V_MAX;
            LONG targetLeadY = (vy * LEAD_MAX_FP) / V_MAX;

            targetLeadX = ClampLeadFP(targetLeadX);
            targetLeadY = ClampLeadFP(targetLeadY);

            leadX += (targetLeadX - leadX) / LEAD_FOLLOW_DIV;
            leadY += (targetLeadY - leadY) / LEAD_FOLLOW_DIV;

            if (vx == 0 && vy == 0) {
                leadX = (leadX * LEAD_DECAY_NUM) / LEAD_DECAY_DEN;
                leadY = (leadY * LEAD_DECAY_NUM) / LEAD_DECAY_DEN;

                if (leadX < LEAD_STOP_FP && leadX > -LEAD_STOP_FP)
                    leadX = 0;
                if (leadY < LEAD_STOP_FP && leadY > -LEAD_STOP_FP)
                    leadY = 0;
            }
        }

        /* Compute front top-left from ring top-left + inverse REST offset + lead
           Also apply the 1px upward tweak.
        */
        WORD frontX = (WORD)(ringX - RING_OFFSET_X + (leadX / 256));
        WORD frontY = (WORD)(ringY - RING_OFFSET_Y + (leadY / 256) - 1);

        /* ---------------- Draw frame ---------------- */
        {
            struct RastPort *rp = useDBuf ? Gfx_GetDrawRastPort() : &Gfx_GetScreen()->RastPort;

            if (haveBg) {
                WaitBlit();
                BltBitMap(&bg, 0, 0, rp->BitMap, 0, 0, SCR_W, SCR_H, 0xC0, 0xFF, NULL);
                WaitBlit();
            }

            /* Targets are rendered on top of background, below the optic. */
            TargetsHandler_Draw(rp);

            /* 1) Copy original front mask -> temp mask */
            WaitBlit();
            BltBitMap(&maskSrcBm, 0, 0, &maskTmpBm, 0, 0, FRONTSIGHT_W, FRONTSIGHT_H, 0xC0, 0xFF,
                      NULL);
            WaitBlit();

            /* 2) Compute occlusion rect in SCREEN coords (based on ring) */
            WORD occX = (WORD)(ringX + OCCL_REL_X);
            WORD occY = (WORD)(ringY + OCCL_REL_Y);

            WORD ix, iy, iw, ih;
            if (IntersectRect(frontX, frontY, FRONTSIGHT_W, FRONTSIGHT_H, occX, occY, OCCL_W,
                              OCCL_H, &ix, &iy, &iw, &ih)) {

                WORD localX1 = (WORD)(ix - frontX);
                WORD localY1 = (WORD)(iy - frontY);
                WORD localX2 = (WORD)(localX1 + iw - 1);
                WORD localY2 = (WORD)(localY1 + ih - 1);

                SetAPen(&maskTmpRP, 0);
                RectFill(&maskTmpRP, localX1, localY1, localX2, localY2);
                WaitBlit();
            }

            /* 3) Draw front with modified mask (clipped) */
            DrawMaskedClipped(&frontSight.bm, tempMaskPlane, rp, frontX, frontY, FRONTSIGHT_W,
                              FRONTSIGHT_H);

            /* 4) Draw ring on top (clipped) */
            DrawMaskedClipped(&rearSight.bm, rearSight.mask, rp, ringX, ringY, REARSIGHT_W,
                              REARSIGHT_H);

            if (useDBuf)
                Gfx_SwapBuffers();
        }

        WaitTOF();
    }

    /* ---- Cleanup ---- */
    TargetsHandler_Shutdown();

    Bob_Free(&frontSight);
    Bob_Free(&rearSight);

    if (tempMaskPlane) {
        FreeRaster(tempMaskPlane, FRONTSIGHT_W, FRONTSIGHT_H);
        tempMaskPlane = NULL;
    }

    for (UWORD p = 0; p < 5; p++) {
        if (bg.Planes[p]) {
            FreeRaster(bg.Planes[p], SCR_W, SCR_H);
            bg.Planes[p] = NULL;
        }
    }
}