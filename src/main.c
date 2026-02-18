#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "assets.h"
#include "bob.h"
#include "gfx.h"
#include "input.h"
#include "sightHandler.h"

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

/* LoRes screens (TRNGINFO + FUNDAMENTALS + TARGETRANGES + RANGE) */
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
#define TARGETRANGES_FILE "gfx/TARGETRANGES.RAW"
#define RANGE_FILE "gfx/OAHU_RANGE.RAW"

#define FRONTSIGHT_RAW "gfx/FrontSight.raw"
#define FRONTSIGHT_MASK "gfx/FrontSight.mask"

/* Front sight (muszka) dimensions (planar RAW + 1-bit MASK) */
#define FRONTSIGHT_W 83
#define FRONTSIGHT_H 87

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

/* Check if double buffering is safe on this system (graphics.library >= 39, Kick 3.0+) */
static BOOL UseDoubleBuffering(void) {
    struct Library *GfxBase = OpenLibrary("graphics.library", 0);
    if (!GfxBase)
        return FALSE;

    BOOL safe = (GfxBase->lib_Version >= 39);

    CloseLibrary(GfxBase);
    return safe;
}

/* ------------------------------------------------------------------ */
/* MAIN                                                               */
/* ------------------------------------------------------------------ */

typedef enum { ATTR_TRNGINFO = 0, ATTR_FUNDAMENTALS, ATTR_TARGETRANGES } AttractScreen;

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

    AttractScreen attr = ATTR_TRNGINFO;

    /* ---------------- Attract loop: TRNGINFO -> FUNDAMENTALS -> TARGETRANGES ---------------- */
    for (;;) {
        WaitResult r = WaitForAdvanceOrTimeout(INFO_SECONDS);
        if (r == WAIT_ESC) {
            goto exit_ok;
        }
        if (r == WAIT_ADVANCE) {
            engaged = TRUE;
        }

        if (engaged) {
            /* Engaged path: ensure order FUNDAMENTALS -> TARGETRANGES -> RANGE */
            if (attr == ATTR_TRNGINFO) {
                for (int i = 0; i < 32; i++) {
                    nextLoPal[i] = fundamentalsPalette[i];
                }
                if (!Gfx_CrossFadeToImage(FUNDAMENTALS_FILE, currentLoPal, 32, nextLoPal, 32)) {
                    goto fail;
                }
                for (int i = 0; i < 32; i++) {
                    currentLoPal[i] = nextLoPal[i];
                }
                attr = ATTR_FUNDAMENTALS;
            }

            if (attr == ATTR_FUNDAMENTALS) {
                for (int i = 0; i < 32; i++) {
                    nextLoPal[i] = targetRangesPalette[i];
                }
                if (!Gfx_CrossFadeToImage(TARGETRANGES_FILE, currentLoPal, 32, nextLoPal, 32)) {
                    goto fail;
                }
                for (int i = 0; i < 32; i++) {
                    currentLoPal[i] = nextLoPal[i];
                }
                attr = ATTR_TARGETRANGES;

                /* TargetRanges behaves like Fundamentals: time + LMB/Fire/ESC */
                {
                    WaitResult rr = WaitForAdvanceOrTimeout(INFO_SECONDS);
                    if (rr == WAIT_ESC) {
                        goto exit_ok;
                    }
                }
            }

            /* Enter RANGE */
            if (!Gfx_CrossFadeToImage(RANGE_FILE, currentLoPal, 32, oahuRangePalette, 32)) {
                goto fail;
            }

            BOOL useDBuf = UseDoubleBuffering();

            if (useDBuf) {
                if (!Gfx_EnableDoubleBuffering()) {
                    goto fail;
                }
            }

            RunRangeWithFrontSight(useDBuf);

            goto exit_ok;
        }

        /* Not engaged: carousel advance */
        if (attr == ATTR_TRNGINFO) {
            attr = ATTR_FUNDAMENTALS;
            for (int i = 0; i < 32; i++) {
                nextLoPal[i] = fundamentalsPalette[i];
            }
            if (!Gfx_CrossFadeToImage(FUNDAMENTALS_FILE, currentLoPal, 32, nextLoPal, 32)) {
                goto fail;
            }

        } else if (attr == ATTR_FUNDAMENTALS) {
            attr = ATTR_TARGETRANGES;
            for (int i = 0; i < 32; i++) {
                nextLoPal[i] = targetRangesPalette[i];
            }
            if (!Gfx_CrossFadeToImage(TARGETRANGES_FILE, currentLoPal, 32, nextLoPal, 32)) {
                goto fail;
            }

        } else { /* ATTR_TARGETRANGES */
            attr = ATTR_TRNGINFO;
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