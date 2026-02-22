#include "sightHandler.h"

#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "bob.h"
#include "gfx.h"
#include "input.h"

/* External input directions */
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

/* Extra movement down */
#define OVERSCAN_Y 16

/* Ring offset relative to front sight (ring follows front sight for now) */
#define RING_OFFSET_X -16
#define RING_OFFSET_Y -51

/* ---- "Occlusion" rectangle under the ring ----
   Coordinates are relative to ring top-left (ringX, ringY):
   - 1 px left
   - 115 px down (i.e. directly under the ring)
*/
#define OCCL_REL_X (-1)
#define OCCL_REL_Y (REARSIGHT_H) /* 115 */
#define OCCL_W 119
#define OCCL_H 39

/* -------------------------------------------------------------- */

static void DrainWindowMessages(void) {
    struct Window *win = Gfx_GetWindow();
    struct IntuiMessage *msg;

    if (!win || !win->UserPort)
        return;

    while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)))
        ReplyMsg((struct Message *)msg);
}

static void PollAdvanceAndEsc(BOOL *outAdvance, BOOL *outEsc) {
    struct Window *win = Gfx_GetWindow();
    struct IntuiMessage *msg;

    *outAdvance = FALSE;
    *outEsc = FALSE;

    if (win && win->UserPort) {
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {
            if (msg->Class == IDCMP_RAWKEY && msg->Code == 0x45)
                *outEsc = TRUE;

            if (msg->Class == IDCMP_MOUSEBUTTONS && msg->Code == SELECTDOWN)
                *outAdvance = TRUE;

            ReplyMsg((struct Message *)msg);
        }
    }

    if (IsJoystickFirePressed())
        *outAdvance = TRUE;
}

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
 * CRITICAL FIX:
 * Manual clipping for BltMaskBitMapRastPort().
 * Blitter does NOT clip when dstx/dsty are negative or exceed screen.
 * Without this, you'll get wraparound artifacts (ghost halves, flicker bands).
 */
static void DrawMaskedClipped(const struct BitMap *srcBm, PLANEPTR maskPlane,
                              struct RastPort *dstRP, WORD dstX, WORD dstY, WORD srcW, WORD srcH) {
    /* Clip destination to visible screen area [0..SCR_W-1, 0..SCR_H-1] */
    WORD sx = 0;
    WORD sy = 0;
    WORD w = srcW;
    WORD h = srcH;
    WORD dx = dstX;
    WORD dy = dstY;

    if (!srcBm || !maskPlane || !dstRP || !dstRP->BitMap)
        return;

    /* Left clip */
    if (dx < 0) {
        sx = (WORD)(-dx);
        w = (WORD)(w - sx);
        dx = 0;
    }
    /* Top clip */
    if (dy < 0) {
        sy = (WORD)(-dy);
        h = (WORD)(h - sy);
        dy = 0;
    }
    /* Right clip */
    if ((dx + w) > SCR_W) {
        w = (WORD)(SCR_W - dx);
    }
    /* Bottom clip */
    if ((dy + h) > SCR_H) {
        h = (WORD)(SCR_H - dy);
    }

    if (w <= 0 || h <= 0)
        return;

    /* Perform masked blit using clipped rect and src offsets */
    BltMaskBitMapRastPort((struct BitMap *)srcBm, sx, sy, dstRP, dx, dy, w, h, 0xE0, maskPlane);
    WaitBlit();
}

/* -------------------------------------------------------------- */

void RunRangeWithFrontSight(BOOL useDBuf) {
    AmacsBob frontSight;
    AmacsBob rearSight;

    WORD x = (SCR_W - FRONTSIGHT_W) / 2;
    WORD y = (SCR_H - FRONTSIGHT_H) / 2;

    LONG ax = 0, ay = 0;
    LONG vx = 0, vy = 0;

    const LONG V_MAX = 8192;
    const LONG V_MIN = 128;
    const LONG V_STOP = 32;
    const LONG ACCEL_DIV = 6;
    const LONG DECAY_NUM = 200;
    const LONG DECAY_DEN = 256;

    const UWORD START_DELAY = 3;

    static int prevDirX = 0, prevDirY = 0;
    static UWORD holdX = 0, holdY = 0;

    struct BitMap bg;
    BOOL haveBg = FALSE;

    /* ---- Temporary 1-bit mask for per-frame occlusion ---- */
    PLANEPTR tempMaskPlane = NULL;
    struct BitMap maskSrcBm; /* wraps frontSight.mask */
    struct BitMap maskTmpBm; /* wraps tempMaskPlane */
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

    /* Allocate temp mask (1-bit plane) */
    tempMaskPlane = (PLANEPTR)AllocRaster(FRONTSIGHT_W, FRONTSIGHT_H);
    if (!tempMaskPlane) {
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

    DrainWindowMessages();

    prevDirX = prevDirY = 0;
    holdX = holdY = 0;
    ax = ay = 0;
    vx = vy = 0;

    /* ========================================================== */

    for (;;) {
        BOOL adv = FALSE, esc = FALSE;

        PollAdvanceAndEsc(&adv, &esc);
        if (esc || adv)
            break;

        int dirX = (Input_Right() ? 1 : 0) - (Input_Left() ? 1 : 0);
        int dirY = (Input_Down() ? 1 : 0) - (Input_Up() ? 1 : 0);

        /* ---------------- X axis ---------------- */

        if (dirX != 0) {
            if (prevDirX == 0) {
                x += (WORD)dirX;
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
                        x++;
                    }
                    while (ax <= -256) {
                        ax += 256;
                        x--;
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
                x++;
            }
            while (ax <= -256) {
                ax += 256;
                x--;
            }

            if (vx == 0)
                ax = 0;
        }

        /* ---------------- Y axis ---------------- */

        if (dirY != 0) {
            if (prevDirY == 0) {
                y += (WORD)dirY;
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
                        y++;
                    }
                    while (ay <= -256) {
                        ay += 256;
                        y--;
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
                y++;
            }
            while (ay <= -256) {
                ay += 256;
                y--;
            }

            if (vy == 0)
                ay = 0;
        }

        prevDirX = dirX;
        prevDirY = dirY;

        /* ---------------- Clamp (movement logic stays) ---------------- */

        if (x < -OVERSCAN_X)
            x = -OVERSCAN_X;

        if (x > SCR_W - FRONTSIGHT_W + OVERSCAN_X)
            x = SCR_W - FRONTSIGHT_W + OVERSCAN_X;

        if (y < 0)
            y = 0;

        if (y > SCR_H - FRONTSIGHT_H + OVERSCAN_Y)
            y = SCR_H - FRONTSIGHT_H + OVERSCAN_Y;

        /* ---------------- Draw frame ---------------- */

        {
            struct RastPort *rp = useDBuf ? Gfx_GetDrawRastPort() : &Gfx_GetScreen()->RastPort;

            if (haveBg) {
                WaitBlit();
                BltBitMap(&bg, 0, 0, rp->BitMap, 0, 0, SCR_W, SCR_H, 0xC0, 0xFF, NULL);
                WaitBlit();
            }

            /* Ring position (follows front sight for now) */
            WORD ringX = (WORD)(x + RING_OFFSET_X);
            WORD ringY = (WORD)(y + RING_OFFSET_Y);

            /* 1) Copy original front mask -> temp mask */
            WaitBlit();
            BltBitMap(&maskSrcBm, 0, 0, &maskTmpBm, 0, 0, FRONTSIGHT_W, FRONTSIGHT_H, 0xC0, 0xFF,
                      NULL);
            WaitBlit();

            /* 2) Compute occlusion rect in SCREEN coords */
            WORD occX = (WORD)(ringX + OCCL_REL_X);
            WORD occY = (WORD)(ringY + OCCL_REL_Y);

            /* Intersect occlusion rect with front sight rect */
            WORD ix, iy, iw, ih;
            if (IntersectRect(x, y, FRONTSIGHT_W, FRONTSIGHT_H, occX, occY, OCCL_W, OCCL_H, &ix,
                              &iy, &iw, &ih)) {
                /* Convert to front-sight-local coords */
                WORD localX1 = (WORD)(ix - x);
                WORD localY1 = (WORD)(iy - y);
                WORD localX2 = (WORD)(localX1 + iw - 1);
                WORD localY2 = (WORD)(localY1 + ih - 1);

                SetAPen(&maskTmpRP, 0);
                RectFill(&maskTmpRP, localX1, localY1, localX2, localY2);
                WaitBlit();
            }

            /* 3) Draw front sight with clipped masked blit (FIX) */
            DrawMaskedClipped(&frontSight.bm, tempMaskPlane, rp, x, y, FRONTSIGHT_W, FRONTSIGHT_H);

            /* 4) Draw ring on top, also clipped (FIX) */
            DrawMaskedClipped(&rearSight.bm, rearSight.mask, rp, ringX, ringY, REARSIGHT_W,
                              REARSIGHT_H);

            if (useDBuf)
                Gfx_SwapBuffers();
        }

        WaitTOF();
    }

    /* ---- Cleanup ---- */

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