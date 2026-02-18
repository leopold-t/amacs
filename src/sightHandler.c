#include "sightHandler.h"

#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "bob.h"
#include "gfx.h"
#include "input.h"

/* If your input.h doesn't expose directions yet, keep these externs here. */
extern BOOL Input_Left(void);
extern BOOL Input_Right(void);
extern BOOL Input_Up(void);
extern BOOL Input_Down(void);

/* Front sight (muszka) assets */
#define FRONTSIGHT_RAW "gfx/FrontSight.raw"
#define FRONTSIGHT_MASK "gfx/FrontSight.mask"
#define FRONTSIGHT_W 83
#define FRONTSIGHT_H 87

/* ------------------------------------------------------------------ */
/* Local input polling helpers                                         */
/* ------------------------------------------------------------------ */

/*
 * IMPORTANT:
 * Do not poll IDCMP in two separate functions (ESC + ADVANCE),
 * because the first reader drains messages and the second sees nothing.
 *
 * This function consumes IDCMP once per call and returns both flags.
 */
static void PollAdvanceAndEsc(BOOL *outAdvance, BOOL *outEsc) {
    struct Window *win = Gfx_GetWindow();
    struct IntuiMessage *msg;

    *outAdvance = FALSE;
    *outEsc = FALSE;

    if (win && win->UserPort) {
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {

            if (msg->Class == IDCMP_RAWKEY && msg->Code == 0x45) {
                *outEsc = TRUE; /* ESC */
            }

            if (msg->Class == IDCMP_MOUSEBUTTONS && msg->Code == SELECTDOWN) {
                *outAdvance = TRUE; /* LMB */
            }

            ReplyMsg((struct Message *)msg);
        }
    }

    /* Joystick fire (port handling is inside input.c) */
    if (IsJoystickFirePressed()) {
        *outAdvance = TRUE;
    }
}

static void DrainWindowMessages(void) {
    struct Window *win = Gfx_GetWindow();
    struct IntuiMessage *msg;

    if (!win || !win->UserPort) {
        return;
    }

    while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {
        ReplyMsg((struct Message *)msg);
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

void RunRangeWithFrontSight(BOOL useDBuf) {
    AmacsBob frontSight;

    WORD x = (320 - FRONTSIGHT_W) / 2;
    WORD y = (256 - FRONTSIGHT_H) / 2;

    /* Fixed-point accumulators (1/256 px units) and signed velocities */
    LONG ax = 0, ay = 0;
    LONG vx = 0, vy = 0;

    /* --- Tuning --- */
    const LONG V_MAX = 8192;    /* 32.0 px/frame */
    const LONG V_MIN = 128;     /* 0.5 px/frame once ramp starts */
    const LONG V_STOP = 32;     /* snap to 0 when slower than this */
    const LONG ACCEL_DIV = 6;   /* bigger = gentler acceleration */
    const LONG DECAY_NUM = 200; /* 200/256 ~= 0.78 per frame */
    const LONG DECAY_DEN = 256;

    /* Tap/precision handling */
    const UWORD START_DELAY = 3; /* frames held before inertia/ramp starts */

    /* Per-axis state */
    static int prevDirX = 0, prevDirY = 0;
    static UWORD holdX = 0, holdY = 0;

    /* Background copy for restore (keep both buffers consistent if DBuf) */
    struct BitMap bg;
    BOOL haveBg = FALSE;

    if (!Bob_LoadRawAndMask(&frontSight, FRONTSIGHT_RAW, FRONTSIGHT_MASK, FRONTSIGHT_W,
                            FRONTSIGHT_H, 5)) {
        return;
    }

    /* Capture background from current front buffer */
    InitBitMap(&bg, 5, 320, 256);
    for (UWORD p = 0; p < 5; p++) {
        bg.Planes[p] = (PLANEPTR)AllocRaster(320, 256);
        if (!bg.Planes[p]) {
            for (UWORD q = 0; q < p; q++) {
                if (bg.Planes[q]) {
                    FreeRaster(bg.Planes[q], 320, 256);
                    bg.Planes[q] = NULL;
                }
            }
            Bob_Free(&frontSight);
            return;
        }
    }

    {
        struct Screen *scr = Gfx_GetScreen();
        if (scr && scr->RastPort.BitMap) {
            WaitBlit();
            BltBitMap(scr->RastPort.BitMap, 0, 0, &bg, 0, 0, 320, 256, 0xC0, 0xFF, NULL);
            WaitBlit();
            haveBg = TRUE;
        }
    }

    /* First frame */
    {
        struct RastPort *rp = useDBuf ? Gfx_GetDrawRastPort() : &Gfx_GetScreen()->RastPort;

        if (haveBg) {
            WaitBlit();
            BltBitMap(&bg, 0, 0, rp->BitMap, 0, 0, 320, 256, 0xC0, 0xFF, NULL);
            WaitBlit();
        }

        Bob_DrawMaskedToRastPort(&frontSight, rp, x, y);

        if (useDBuf) {
            Gfx_SwapBuffers();
        }
    }

    DrainWindowMessages();

    /* Reset axis state on entry */
    prevDirX = prevDirY = 0;
    holdX = holdY = 0;
    ax = ay = 0;
    vx = vy = 0;

    for (;;) {
        BOOL adv = FALSE, esc = FALSE;

        PollAdvanceAndEsc(&adv, &esc);
        if (esc || adv) {
            break; /* ESC or Fire/LMB exits range */
        }

        int dirX = (Input_Right() ? 1 : 0) - (Input_Left() ? 1 : 0);
        int dirY = (Input_Down() ? 1 : 0) - (Input_Up() ? 1 : 0);

        /* ---- X axis: tap=1px + delayed ramp ---- */
        if (dirX != 0) {
            if (prevDirX == 0 || dirX != prevDirX) {
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

                    /* ensure minimum motion once ramp is active */
                    {
                        LONG avx = (vx < 0) ? -vx : vx;
                        if (avx < V_MIN)
                            vx = (LONG)dirX * V_MIN;
                    }

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

        /* ---- Y axis: tap=1px + delayed ramp ---- */
        if (dirY != 0) {
            if (prevDirY == 0 || dirY != prevDirY) {
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

                    {
                        LONG avy = (vy < 0) ? -vy : vy;
                        if (avy < V_MIN)
                            vy = (LONG)dirY * V_MIN;
                    }

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

        /* Clamp to screen */
        if (x < 0)
            x = 0;
        if (y < 0)
            y = 0;
        if (x > (320 - FRONTSIGHT_W))
            x = (320 - FRONTSIGHT_W);
        if (y > (256 - FRONTSIGHT_H))
            y = (256 - FRONTSIGHT_H);

        /* Draw */
        {
            struct RastPort *rp = useDBuf ? Gfx_GetDrawRastPort() : &Gfx_GetScreen()->RastPort;

            if (haveBg) {
                WaitBlit();
                BltBitMap(&bg, 0, 0, rp->BitMap, 0, 0, 320, 256, 0xC0, 0xFF, NULL);
                WaitBlit();
            }

            Bob_DrawMaskedToRastPort(&frontSight, rp, x, y);

            if (useDBuf) {
                Gfx_SwapBuffers();
            }
        }

        WaitTOF();
    }

    Bob_Free(&frontSight);

    for (UWORD p = 0; p < 5; p++) {
        if (bg.Planes[p]) {
            FreeRaster(bg.Planes[p], 320, 256);
            bg.Planes[p] = NULL;
        }
    }
}