#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "assets.h"
#include "bob.h"
#include "gfx.h"
#include "input.h"

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

    if (IsJoystickFirePressed()) {
        *outAdvance = TRUE;
    }
}

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

    /* full-screen background snapshot bitmap */
    struct BitMap bg;
    BOOL haveBg = FALSE;

    WORD x = (320 - 85) / 2;
    WORD y = (256 - 88) / 2;

    UWORD ax = 0, ay = 0;
    UWORD vx = 0, vy = 0;

    const UWORD V_MIN = 64;   /* 0.25 px/frame */
    const UWORD V_MAX = 4094; /* 16 px/frame */
    const UWORD V_RAMP = 512; /* ramp per frame while held */

    if (!Bob_LoadRawAndMask(&frontSight, FRONTSIGHT_RAW, FRONTSIGHT_MASK, 85, 88, 5)) {
        return;
    }

    /* ---- VERY IMPORTANT: zero-init bg + planes ---- */
    bg.BytesPerRow = 0;
    bg.Rows = 0;
    bg.Flags = 0;
    bg.Depth = 0;
    for (int i = 0; i < 8; i++) {
        bg.Planes[i] = NULL;
    }

    /* Snapshot the static background into a full-screen bitmap so we can redraw it each frame. */
    {
        InitBitMap(&bg, 5, 320, 256);

        haveBg = TRUE;
        for (UWORD p = 0; p < 5; p++) {
            bg.Planes[p] = (PLANEPTR)AllocRaster(320, 256);
            if (!bg.Planes[p]) {
                haveBg = FALSE;
                break;
            }
        }

        if (haveBg) {
            WaitBlit();
            BltBitMap(Gfx_GetScreen()->RastPort.BitMap, 0, 0, &bg, 0, 0, 320, 256, 0xC0, 0xFF,
                      NULL);
            WaitBlit();
        }
    }

    /* Draw first frame */
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

    for (;;) {
        BOOL adv = FALSE, esc = FALSE;

        PollAdvanceAndEsc(&adv, &esc);
        if (esc || adv) {
            break;
        }

        /* Horizontal movement with ramp */
        if (Input_Left() ^ Input_Right()) {
            if (vx < V_MIN)
                vx = V_MIN;
            else if (vx < V_MAX)
                vx = (UWORD)(((vx + V_RAMP) > V_MAX) ? V_MAX : (vx + V_RAMP));

            ax = (UWORD)(ax + vx);
            while (ax >= 256) {
                ax -= 256;
                if (Input_Left())
                    x--;
                else
                    x++;
            }
        } else {
            vx = 0;
            ax = 0;
        }

        /* Vertical movement with ramp */
        if (Input_Up() ^ Input_Down()) {
            if (vy < V_MIN)
                vy = V_MIN;
            else if (vy < V_MAX)
                vy = (UWORD)(((vy + V_RAMP) > V_MAX) ? V_MAX : (vy + V_RAMP));

            ay = (UWORD)(ay + vy);
            while (ay >= 256) {
                ay -= 256;
                if (Input_Up())
                    y--;
                else
                    y++;
            }
        } else {
            vy = 0;
            ay = 0;
        }

        /* Clamp to screen */
        if (x < 0)
            x = 0;
        if (y < 0)
            y = 0;
        if (x > (320 - 85))
            x = (320 - 85);
        if (y > (256 - 88))
            y = (256 - 88);

        /* Draw on back buffer, then swap */
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

    /* cleanup */
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

            if (!Gfx_EnableDoubleBuffering()) {
                goto fail;
            }

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