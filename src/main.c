#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "assets.h"
#include "bob.h"
#include "gfx.h"
#include "input.h"

/* If your input.h doesn't expose directions yet, keep these externs here.
   They must exist in input.c (or you'll get linker errors). */
extern BOOL Input_Left(void);
extern BOOL Input_Right(void);
extern BOOL Input_Up(void);
extern BOOL Input_Down(void);

/* HiRes screens (LOGO + TITLE) */
#define HI_WIDTH 640
#define HI_HEIGHT 256
#define HI_DEPTH 4

/* LoRes screens (TRNGINFO + FUNDAMENTALS + RANGE) */
#define LO_WIDTH 320
#define LO_HEIGHT 256
#define LO_DEPTH 5

/* Black safety screen */
#define BLK_WIDTH 320
#define BLK_HEIGHT 256
#define BLK_DEPTH 2

#define LOGO_FILE "gfx/LOGO.RAW"
#define TITLE_FILE "gfx/TITLE.RAW"
#define TRNGINFO_FILE "gfx/TRNGINFO.RAW"
#define FUNDAMENTALS_FILE "gfx/FUNDAMENTALS.RAW"
#define RANGE_FILE "gfx/OAHU_RANGE.RAW"

#define FRONTSIGHT_RAW "gfx/FrontSight.raw"
#define FRONTSIGHT_MASK "gfx/FrontSight.mask"

/* Timing (PAL: 50 ticks/sec). */
#define TICKS_PER_SEC 50
#define LOGO_SECONDS 3
#define TITLE_SECONDS 12
#define INFO_SECONDS 6

typedef enum { WAIT_TIMEOUT = 0, WAIT_ADVANCE, WAIT_ESC } WaitResult;

/* ---------- Helpers ---------- */

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

/*
 * IMPORTANT:
 * Do not poll IDCMP in two separate functions (ESC + ADVANCE), because the first reader
 * drains messages and the second sees nothing.
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

/*
 * Wait up to `seconds` for:
 * - ESC => WAIT_ESC
 * - Fire or LMB => WAIT_ADVANCE
 * - timeout => WAIT_TIMEOUT
 *
 * Debounces Fire to prevent double-advancing.
 */
static WaitResult WaitForAdvanceOrTimeout(int seconds) {
    LONG ticks = (LONG)seconds * TICKS_PER_SEC;

    DrainWindowMessages();

    while (ticks-- > 0) {
        BOOL adv = FALSE, esc = FALSE;

        PollAdvanceAndEsc(&adv, &esc);

        if (esc) {
            return WAIT_ESC;
        }

        if (adv) {
            /* Debounce Fire (mouse click is one-shot anyway). */
            WaitTOF();
            WaitTOF();
            while (IsJoystickFirePressed()) {
                WaitTOF();
            }

            DrainWindowMessages();
            return WAIT_ADVANCE;
        }

        WaitTOF();
    }

    return WAIT_TIMEOUT;
}

/* ------------------------------------------------------------------ */
/* Range screen: DBuf + moving front sight                              */
/* ------------------------------------------------------------------ */
static void RunRangeWithFrontSight(void) {
    AmacsBob frontSight;

    WORD x = (320 - 85) / 2;
    WORD y = (256 - 88) / 2;

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

    /* Background copy for DBuf (keep both buffers consistent) */
    struct BitMap bg;
    BOOL haveBg = FALSE;

    if (!Bob_LoadRawAndMask(&frontSight, FRONTSIGHT_RAW, FRONTSIGHT_MASK, 85, 88, 5)) {
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
        struct RastPort *rp = Gfx_GetDrawRastPort();
        if (haveBg) {
            WaitBlit();
            BltBitMap(&bg, 0, 0, rp->BitMap, 0, 0, 320, 256, 0xC0, 0xFF, NULL);
            WaitBlit();
        }
        Bob_DrawMaskedToRastPort(&frontSight, rp, x, y);
        Gfx_SwapBuffers();
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
            break;
        }

        int dirX = (Input_Right() ? 1 : 0) - (Input_Left() ? 1 : 0);
        int dirY = (Input_Down() ? 1 : 0) - (Input_Up() ? 1 : 0);

        /* ---- X axis: tap=1px + delayed ramp ---- */
        if (dirX != 0) {
            if (prevDirX == 0) {
                /* fresh tap -> exactly 1px */
                x += (WORD)dirX;
                holdX = 1;

                /* kill inertia on tap start */
                vx = 0;
                ax = 0;
            } else if (dirX != prevDirX) {
                /* direction changed -> hard reset then 1px */
                x += (WORD)dirX;
                holdX = 1;
                vx = 0;
                ax = 0;
            } else {
                /* still held */
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
                /* else: within START_DELAY -> no extra movement (prevents multi-px on short tap) */
            }
        } else {
            holdX = 0;
            prevDirX = 0;

            /* exponential decay */
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

            /* optional: if nearly stopped, clear accumulator to avoid “creep” */
            if (vx == 0)
                ax = 0;
        }

        /* ---- Y axis: tap=1px + delayed ramp ---- */
        if (dirY != 0) {
            if (prevDirY == 0) {
                y += (WORD)dirY;
                holdY = 1;
                vy = 0;
                ay = 0;
            } else if (dirY != prevDirY) {
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

        /* Clamp */
        if (x < 0)
            x = 0;
        if (y < 0)
            y = 0;
        if (x > (320 - 85))
            x = (320 - 85);
        if (y > (256 - 88))
            y = (256 - 88);

        /* Draw */
        {
            struct RastPort *rp = Gfx_GetDrawRastPort();

            if (haveBg) {
                WaitBlit();
                BltBitMap(&bg, 0, 0, rp->BitMap, 0, 0, 320, 256, 0xC0, 0xFF, NULL);
                WaitBlit();
            }

            Bob_DrawMaskedToRastPort(&frontSight, rp, x, y);
            Gfx_SwapBuffers();
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

/* ------------------------------------------------------------------ */
/* MAIN                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    BOOL engaged = FALSE;
    UWORD currentLoPal[32];
    UWORD nextLoPal[32];

    if (!Input_Init()) {
        return RETURN_FAIL;
    }

    if (!Gfx_OpenBlackScreen(BLK_WIDTH, BLK_HEIGHT, BLK_DEPTH)) {
        Input_Shutdown();
        return RETURN_FAIL;
    }

    /* ---------------- HiRes: LOGO ---------------- */
    if (!Gfx_OpenScreenAndWindow(HI_WIDTH, HI_HEIGHT, HI_DEPTH, HIRES_KEY)) {
        Gfx_CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }

    if (!Gfx_ShowImageFadeInFromBlack(LOGO_FILE, logoPalette, 16)) {
        Gfx_CloseScreenAndWindow();
        Gfx_CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }

    {
        WaitResult r = WaitForAdvanceOrTimeout(LOGO_SECONDS);
        if (r == WAIT_ESC) {
            goto exit_ok;
        }
    }

    /* ---------------- HiRes: TITLE ---------------- */
    if (!Gfx_CrossFadeToImage(TITLE_FILE, logoPalette, 16, titlePalette, 16)) {
        Gfx_CloseScreenAndWindow();
        Gfx_CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }

    {
        WaitResult r = WaitForAdvanceOrTimeout(TITLE_SECONDS);
        if (r == WAIT_ESC) {
            goto exit_ok;
        }
        if (r == WAIT_ADVANCE) {
            engaged = TRUE;
        }
    }

    /* ---------------- Switch to LoRes once ---------------- */
    if (!Gfx_SwitchHiResToLoResOnBlack(titlePalette, LO_WIDTH, LO_HEIGHT, LO_DEPTH)) {
        Gfx_CloseScreenAndWindow();
        Gfx_CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }

    if (!Gfx_ShowImageFadeInFromBlack(TRNGINFO_FILE, trngInfoPalette, 32)) {
        Gfx_CloseScreenAndWindow();
        Gfx_CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }

    for (int i = 0; i < 32; i++) {
        currentLoPal[i] = trngInfoPalette[i];
    }

    /* ---------------- Attract loop: TRNGINFO <-> FUNDAMENTALS ---------------- */
    for (;;) {
        WaitResult r = WaitForAdvanceOrTimeout(INFO_SECONDS);
        if (r == WAIT_ESC) {
            goto exit_ok;
        }
        if (r == WAIT_ADVANCE) {
            engaged = TRUE;
        }

        if (currentLoPal[0] == trngInfoPalette[0] && currentLoPal[1] == trngInfoPalette[1] &&
            currentLoPal[2] == trngInfoPalette[2]) {

            for (int i = 0; i < 32; i++) {
                nextLoPal[i] = fundamentalsPalette[i];
            }

            if (!Gfx_CrossFadeToImage(FUNDAMENTALS_FILE, currentLoPal, 32, nextLoPal, 32)) {
                goto fail;
            }

        } else {

            for (int i = 0; i < 32; i++) {
                nextLoPal[i] = trngInfoPalette[i];
            }

            if (!Gfx_CrossFadeToImage(TRNGINFO_FILE, currentLoPal, 32, nextLoPal, 32)) {
                goto fail;
            }
        }

        for (int i = 0; i < 32; i++) {
            currentLoPal[i] = nextLoPal[i];
        }

        if (engaged) {
            if (!Gfx_CrossFadeToImage(RANGE_FILE, currentLoPal, 32, oahuRangePalette, 32)) {
                goto fail;
            }

            /* Enable DBuf only on the range screen */
            if (!Gfx_EnableDoubleBuffering()) {
                goto fail;
            }

            /* Run range loop with moving front sight */
            RunRangeWithFrontSight();

            goto exit_ok;
        }
    }

fail:
    Gfx_CloseScreenAndWindow();
    Gfx_CloseBlackScreen();
    Input_Shutdown();
    return RETURN_FAIL;

exit_ok:
    Gfx_CloseScreenAndWindow();
    Gfx_CloseBlackScreen();
    Input_Shutdown();
    return RETURN_OK;
}