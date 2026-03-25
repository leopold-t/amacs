#include "sightHandler.h"

#include <dos/dos.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "bob.h"
#include "gfx.h"
#include "input.h"
#include "soundHandler.h"
#include "targetsHandler.h"

extern BOOL Input_Left(void);
extern BOOL Input_Right(void);
extern BOOL Input_Up(void);
extern BOOL Input_Down(void);

#define FRONTSIGHT_RAW "gfx/FrontSight.raw"
#define FRONTSIGHT_MASK "gfx/FrontSight.mask"

#define REARSIGHT_RAW "gfx/RearSight.raw"
#define REARSIGHT_MASK "gfx/RearSight.mask"

#define FRONTSIGHT_W 83
#define FRONTSIGHT_H 79

#define REARSIGHT_W 115
#define REARSIGHT_H 115

#define SCR_W 320
#define SCR_H 256

#define OVERSCAN_X 41
#define OVERSCAN_X_EXTRA 16
#define OVERSCAN_X_TOTAL (OVERSCAN_X + OVERSCAN_X_EXTRA)
#define OVERSCAN_Y 16

#define RING_OFFSET_X (-16)
#define RING_OFFSET_Y (-51)

#define OCCL_REL_X (-6)
#define OCCL_REL_Y (REARSIGHT_H)
#define OCCL_W 124
#define OCCL_H 39

#define LEAD_MAX_PX 34
#define LEAD_MAX_FP (LEAD_MAX_PX * 256)

#define LEAD_FOLLOW_DIV 4
#define LEAD_DECAY_NUM 210
#define LEAD_DECAY_DEN 256
#define LEAD_STOP_FP (2 * 256)

#define DOS_TICKS_PER_SEC 50
#define SHOT_COOLDOWN_TICKS DOS_TICKS_PER_SEC

static void DebugBeepError(SoundError err) {
    UWORD count = 0;
    UWORD i;

    switch (err) {
        case SOUND_ERR_PORT:
            count = 1;
            break;
        case SOUND_ERR_IOREQ:
            count = 2;
            break;
        case SOUND_ERR_OPENDEVICE:
            count = 3;
            break;
        case SOUND_ERR_OPENFILE:
            count = 4;
            break;
        case SOUND_ERR_FILESIZE:
            count = 5;
            break;
        case SOUND_ERR_ALLOCMEM:
            count = 6;
            break;
        case SOUND_ERR_READFILE:
            count = 7;
            break;
        default:
            count = 1;
            break;
    }

    for (i = 0; i < count; i++) {
        DisplayBeep(Gfx_GetScreen());
        Delay(8);
    }
}

static ULONG ElapsedTicksSince(const struct DateStamp *start) {
    struct DateStamp now;
    LONG dd;
    LONG dm;
    LONG dt;
    LONG total;

    DateStamp(&now);

    dd = (LONG)now.ds_Days - (LONG)start->ds_Days;
    dm = (LONG)now.ds_Minute - (LONG)start->ds_Minute;
    dt = (LONG)now.ds_Tick - (LONG)start->ds_Tick;

    total = dd * (24L * 60L * 60L * DOS_TICKS_PER_SEC) + dm * (60L * DOS_TICKS_PER_SEC) + dt;

    if (total < 0) {
        total = 0;
    }

    return (ULONG)total;
}

static BOOL ShotCooldownReady(BOOL active, const struct DateStamp *lastShotStamp) {
    if (!active) {
        return TRUE;
    }

    return (ElapsedTicksSince(lastShotStamp) >= SHOT_COOLDOWN_TICKS) ? TRUE : FALSE;
}

static void MarkShotFired(BOOL *active, struct DateStamp *lastShotStamp) {
    DateStamp(lastShotStamp);
    *active = TRUE;
}

static BOOL IntersectRect(WORD ax, WORD ay, WORD aw, WORD ah, WORD bx, WORD by, WORD bw, WORD bh,
                          WORD *outX, WORD *outY, WORD *outW, WORD *outH) {
    WORD x1 = (ax > bx) ? ax : bx;
    WORD y1 = (ay > by) ? ay : by;
    WORD x2 = ((ax + aw) < (bx + bw)) ? (ax + aw) : (bx + bw);
    WORD y2 = ((ay + ah) < (by + bh)) ? (ay + ah) : (by + bh);
    WORD w = (WORD)(x2 - x1);
    WORD h = (WORD)(y2 - y1);

    if (w <= 0 || h <= 0) {
        return FALSE;
    }

    *outX = x1;
    *outY = y1;
    *outW = w;
    *outH = h;
    return TRUE;
}

static void DrawMaskedClipped(const struct BitMap *srcBm, PLANEPTR maskPlane,
                              struct RastPort *dstRP, WORD dstX, WORD dstY, WORD srcW, WORD srcH) {
    WORD sx = 0;
    WORD sy = 0;
    WORD w = srcW;
    WORD h = srcH;
    WORD dx = dstX;
    WORD dy = dstY;

    if (!srcBm || !maskPlane || !dstRP || !dstRP->BitMap) {
        return;
    }

    if (dx < 0) {
        sx = (WORD)(-dx);
        w = (WORD)(w - sx);
        dx = 0;
    }

    if (dy < 0) {
        sy = (WORD)(-dy);
        h = (WORD)(h - sy);
        dy = 0;
    }

    if ((dx + w) > SCR_W) {
        w = (WORD)(SCR_W - dx);
    }

    if ((dy + h) > SCR_H) {
        h = (WORD)(SCR_H - dy);
    }

    if (w <= 0 || h <= 0) {
        return;
    }

    BltMaskBitMapRastPort((struct BitMap *)srcBm, sx, sy, dstRP, dx, dy, w, h, 0xE0, maskPlane);
    WaitBlit();
}

static LONG ClampLeadFP(LONG v) {
    if (v > LEAD_MAX_FP) {
        return LEAD_MAX_FP;
    }

    if (v < -LEAD_MAX_FP) {
        return -LEAD_MAX_FP;
    }

    return v;
}

void RunRangeWithFrontSight(BOOL useDBuf) {
    AmacsBob frontSight;
    AmacsBob rearSight;
    WORD ringX = (SCR_W - REARSIGHT_W) / 2;
    WORD ringY = (SCR_H - REARSIGHT_H) / 2;
    LONG ax = 0;
    LONG ay = 0;
    LONG vx = 0;
    LONG vy = 0;
    const LONG V_MAX = 8192;
    const LONG V_MIN = 96;
    const LONG V_STOP = 32;
    const LONG ACCEL_DIV = 18;
    const LONG DECAY_NUM = 64;
    const LONG DECAY_DEN = 256;
    const UWORD START_DELAY = 3;
    static int prevDirX = 0;
    static int prevDirY = 0;
    static UWORD holdX = 0;
    static UWORD holdY = 0;
    LONG leadX = 0;
    LONG leadY = 0;
    struct BitMap bg;
    BOOL haveBg = FALSE;
    PLANEPTR tempMaskPlane = NULL;
    struct BitMap maskSrcBm;
    struct BitMap maskTmpBm;
    struct RastPort maskTmpRP;
    BOOL shotCooldownActive = FALSE;
    struct DateStamp lastShotStamp;
    BOOL paused = FALSE;

    if (!Bob_LoadRawAndMask(&frontSight, FRONTSIGHT_RAW, FRONTSIGHT_MASK, FRONTSIGHT_W,
                            FRONTSIGHT_H, 5)) {
        return;
    }

    if (!Bob_LoadRawAndMask(&rearSight, REARSIGHT_RAW, REARSIGHT_MASK, REARSIGHT_W, REARSIGHT_H,
                            5)) {
        Bob_Free(&frontSight);
        return;
    }

    TargetsHandler_Init();

    if (!Sound_Init()) {
        DebugBeepError(Sound_GetLastError());
    }

    tempMaskPlane = (PLANEPTR)AllocRaster(FRONTSIGHT_W, FRONTSIGHT_H);
    if (!tempMaskPlane) {
        Sound_Shutdown();
        TargetsHandler_Shutdown();
        Bob_Free(&frontSight);
        Bob_Free(&rearSight);
        return;
    }

    InitBitMap(&maskSrcBm, 1, FRONTSIGHT_W, FRONTSIGHT_H);
    maskSrcBm.Planes[0] = frontSight.mask;

    InitBitMap(&maskTmpBm, 1, FRONTSIGHT_W, FRONTSIGHT_H);
    maskTmpBm.Planes[0] = tempMaskPlane;

    InitRastPort(&maskTmpRP);
    maskTmpRP.BitMap = &maskTmpBm;

    InitBitMap(&bg, 5, SCR_W, SCR_H);

    {
        UWORD p;
        for (p = 0; p < 5; p++) {
            bg.Planes[p] = (PLANEPTR)AllocRaster(SCR_W, SCR_H);
            if (!bg.Planes[p]) {
                UWORD q;
                for (q = 0; q < 5; q++) {
                    if (bg.Planes[q]) {
                        FreeRaster(bg.Planes[q], SCR_W, SCR_H);
                        bg.Planes[q] = NULL;
                    }
                }

                FreeRaster(tempMaskPlane, FRONTSIGHT_W, FRONTSIGHT_H);
                tempMaskPlane = NULL;
                Sound_Shutdown();
                TargetsHandler_Shutdown();
                Bob_Free(&frontSight);
                Bob_Free(&rearSight);
                return;
            }
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

    prevDirX = 0;
    prevDirY = 0;
    holdX = 0;
    holdY = 0;
    ax = 0;
    ay = 0;
    vx = 0;
    vy = 0;
    leadX = 0;
    leadY = 0;

    for (;;) {
        Input_PollWindow(Gfx_GetWindow());
        Sound_Update();

        if (Input_KeyPressed(0x45)) {
            break;
        }

        if (Input_KeyPressed(0x19)) {
            paused = (BOOL)!paused;
        }

        if (paused) {
            WaitTOF();
            continue;
        }

        if (Input_FirePressed()) {
            if (ShotCooldownReady(shotCooldownActive, &lastShotStamp)) {
                Sound_PlayShot();
                MarkShotFired(&shotCooldownActive, &lastShotStamp);
            }
        }

        TargetsHandler_Tick();

        {
            int dirX = (Input_Right() ? 1 : 0) - (Input_Left() ? 1 : 0);
            int dirY = (Input_Down() ? 1 : 0) - (Input_Up() ? 1 : 0);

            if (dirX != 0) {
                if (prevDirX == 0 || dirX != prevDirX) {
                    ringX += (WORD)dirX;
                    holdX = 1;
                    vx = 0;
                    ax = 0;
                } else {
                    if (holdX < 0xFFFF) {
                        holdX++;
                    }

                    if (holdX >= START_DELAY) {
                        LONG target = (LONG)dirX * V_MAX;
                        LONG dv = target - vx;

                        vx += dv / ACCEL_DIV;

                        if (vx < V_MIN && vx > -V_MIN) {
                            vx = (LONG)dirX * V_MIN;
                        }

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

                if (vx < V_STOP && vx > -V_STOP) {
                    vx = 0;
                }

                ax += vx;

                while (ax >= 256) {
                    ax -= 256;
                    ringX++;
                }
                while (ax <= -256) {
                    ax += 256;
                    ringX--;
                }

                if (vx == 0) {
                    ax = 0;
                }
            }

            if (dirY != 0) {
                if (prevDirY == 0 || dirY != prevDirY) {
                    ringY += (WORD)dirY;
                    holdY = 1;
                    vy = 0;
                    ay = 0;
                } else {
                    if (holdY < 0xFFFF) {
                        holdY++;
                    }

                    if (holdY >= START_DELAY) {
                        LONG target = (LONG)dirY * V_MAX;
                        LONG dv = target - vy;

                        vy += dv / ACCEL_DIV;

                        if (vy < V_MIN && vy > -V_MIN) {
                            vy = (LONG)dirY * V_MIN;
                        }

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

                if (vy < V_STOP && vy > -V_STOP) {
                    vy = 0;
                }

                ay += vy;

                while (ay >= 256) {
                    ay -= 256;
                    ringY++;
                }
                while (ay <= -256) {
                    ay += 256;
                    ringY--;
                }

                if (vy == 0) {
                    ay = 0;
                }
            }

            prevDirX = dirX;
            prevDirY = dirY;
        }

        if (ringX < -OVERSCAN_X_TOTAL) {
            ringX = -OVERSCAN_X_TOTAL;
        }
        if (ringX > SCR_W - REARSIGHT_W + OVERSCAN_X_TOTAL) {
            ringX = (SCR_W - REARSIGHT_W + OVERSCAN_X_TOTAL);
        }

        if (ringY < 0) {
            ringY = 0;
        }
        if (ringY > SCR_H - REARSIGHT_H + OVERSCAN_Y) {
            ringY = (SCR_H - REARSIGHT_H + OVERSCAN_Y);
        }

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

                if (leadX < LEAD_STOP_FP && leadX > -LEAD_STOP_FP) {
                    leadX = 0;
                }
                if (leadY < LEAD_STOP_FP && leadY > -LEAD_STOP_FP) {
                    leadY = 0;
                }
            }
        }

        {
            WORD frontX = (WORD)(ringX - RING_OFFSET_X + (leadX / 256));
            WORD frontY = (WORD)(ringY - RING_OFFSET_Y + (leadY / 256) - 1);
            struct RastPort *rp = useDBuf ? Gfx_GetDrawRastPort() : &Gfx_GetScreen()->RastPort;

            if (haveBg) {
                WaitBlit();
                BltBitMap(&bg, 0, 0, rp->BitMap, 0, 0, SCR_W, SCR_H, 0xC0, 0xFF, NULL);
                WaitBlit();
            }

            TargetsHandler_Draw(rp);

            WaitBlit();
            BltBitMap(&maskSrcBm, 0, 0, &maskTmpBm, 0, 0, FRONTSIGHT_W, FRONTSIGHT_H, 0xC0, 0xFF,
                      NULL);
            WaitBlit();

            {
                WORD occX = ringX + OCCL_REL_X;
                WORD occY = ringY + OCCL_REL_Y;
                WORD ix;
                WORD iy;
                WORD iw;
                WORD ih;

                if (IntersectRect(frontX, frontY, FRONTSIGHT_W, FRONTSIGHT_H, occX, occY, OCCL_W,
                                  OCCL_H, &ix, &iy, &iw, &ih)) {
                    WORD relX = (WORD)(ix - frontX);
                    WORD relY = (WORD)(iy - frontY);

                    SetAPen(&maskTmpRP, 0);
                    RectFill(&maskTmpRP, relX, relY, relX + iw - 1, relY + ih - 1);
                }
            }

            DrawMaskedClipped(&frontSight.bm, tempMaskPlane, rp, frontX, frontY, FRONTSIGHT_W,
                              FRONTSIGHT_H);
            DrawMaskedClipped(&rearSight.bm, rearSight.mask, rp, ringX, ringY, REARSIGHT_W,
                              REARSIGHT_H);

            if (useDBuf) {
                Gfx_SwapBuffers();
            }
        }

        WaitTOF();
    }

    Sound_Shutdown();
    TargetsHandler_Shutdown();

    if (haveBg) {
        UWORD p;
        for (p = 0; p < 5; p++) {
            if (bg.Planes[p]) {
                FreeRaster(bg.Planes[p], SCR_W, SCR_H);
                bg.Planes[p] = NULL;
            }
        }
    }

    if (tempMaskPlane) {
        FreeRaster(tempMaskPlane, FRONTSIGHT_W, FRONTSIGHT_H);
        tempMaskPlane = NULL;
    }

    Bob_Free(&frontSight);
    Bob_Free(&rearSight);
}