#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

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

/* Timing (PAL: 50 ticks/sec). */
#define TICKS_PER_SEC 50
#define LOGO_SECONDS 3
#define TITLE_SECONDS 12
#define INFO_SECONDS 6

typedef enum { WAIT_TIMEOUT = 0, WAIT_ADVANCE, WAIT_ESC } WaitResult;

/* ---------- Palettes ---------- */

/* LOGO palette (16 colors RGB4) */
static UWORD logoPalette[16] = {0x000, 0xEEE, 0x569, 0xAAA, 0x129, 0x333, 0x888, 0x555,
                                0x449, 0xCCC, 0x222, 0x88A, 0x777, 0xDDD, 0xBBB, 0x77A};

/* TITLE palette (16 colors RGB4) */
static UWORD titlePalette[16] = {0x221, 0xC01, 0x03E, 0x259, 0x456, 0x653, 0xB59, 0x38E,
                                 0xD83, 0x69E, 0x7AB, 0xD88, 0xAA8, 0xABD, 0xEDA, 0xDDD};

/* TRNGINFO palette (32 colors RGB4) */
static UWORD trngInfoPalette[32] = {0x0000, 0x0775, 0x0443, 0x0232, 0x0211, 0x0333, 0x0322, 0x0111,
                                    0x0100, 0x0010, 0x0000, 0x0000, 0x0000, 0x0322, 0x0332, 0x0332,
                                    0x0332, 0x0AA9, 0x0CCB, 0x0555, 0x0888, 0x0654, 0x0CCC, 0x0BBB,
                                    0x0999, 0x0777, 0x0986, 0x0444, 0x0EDD, 0x0BAA, 0x0665, 0x0999};

/* FUNDAMENTALS palette (32 colors RGB4) */
static UWORD fundamentalsPalette[32] = {
    0x0000, 0x0888, 0x0007, 0x0005, 0x0447, 0x000A, 0x0222, 0x0003, 0x0111, 0x0001, 0x000C,
    0x0227, 0x0009, 0x000D, 0x022A, 0x000B, 0x0444, 0x0BBC, 0x088B, 0x044A, 0x0DDE, 0x0AAA,
    0x066A, 0x0666, 0x0777, 0x0225, 0x0999, 0x0CCC, 0x0AAC, 0x0339, 0x077B, 0x0448};

/* RANGE palette (32 colors RGB4) */
static UWORD oahuRangePalette[32] = {
    0x0000, 0x09BD, 0x07AD, 0x069D, 0x0110, 0x0110, 0x0220, 0x0230, 0x0341, 0x0453, 0x0563,
    0x0674, 0x0210, 0x0321, 0x0432, 0x0543, 0x0654, 0x0765, 0x0876, 0x0987, 0x0444, 0x0DDE,
    0x0FFF, 0x0FFF, 0x0000, 0x0111, 0x0222, 0x0333, 0x0444, 0x0555, 0x0888, 0x0BBB};

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

static BOOL CheckEscInPort(void) {
    struct Window *win = Gfx_GetWindow();
    struct IntuiMessage *msg;

    if (!win || !win->UserPort) {
        return FALSE;
    }

    while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {
        BOOL esc = FALSE;

        if (msg->Class == IDCMP_RAWKEY && msg->Code == 0x45) {
            esc = TRUE;
        }

        /* Ignore other messages here; they are handled by the advance check. */
        ReplyMsg((struct Message *)msg);

        if (esc) {
            return TRUE;
        }
    }

    return FALSE;
}

static BOOL CheckAdvance(void) {
    struct Window *win = Gfx_GetWindow();
    struct IntuiMessage *msg;

    if (!win || !win->UserPort) {
        return FALSE;
    }

    while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {
        BOOL adv = FALSE;

        if (msg->Class == IDCMP_MOUSEBUTTONS && msg->Code == SELECTDOWN) {
            adv = TRUE; /* LMB */
        }

        /* Do not consume ESC here (handled in CheckEscInPort). */

        ReplyMsg((struct Message *)msg);

        if (adv) {
            return TRUE;
        }
    }

    if (IsJoystickFirePressed()) {
        return TRUE;
    }

    return FALSE;
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
        if (CheckEscInPort()) {
            return WAIT_ESC;
        }

        if (CheckAdvance()) {
            /* Debounce. */
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
        /* Fire/LMB skips ahead but still shows Title next. */
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

    /* Enter LoRes with TRNGINFO and its palette as the starting point. */
    if (!Gfx_ShowImageFadeInFromBlack(TRNGINFO_FILE, trngInfoPalette, 32)) {
        Gfx_CloseScreenAndWindow();
        Gfx_CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }

    /* Track current palette for crossfades. */
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

        /* Next screen in the loop. */
        if (currentLoPal[0] == trngInfoPalette[0] && currentLoPal[1] == trngInfoPalette[1] &&
            currentLoPal[2] == trngInfoPalette[2]) {
            /* Heuristic: assume we are on TRNGINFO -> go to FUNDAMENTALS */
            for (int i = 0; i < 32; i++) {
                nextLoPal[i] = fundamentalsPalette[i];
            }

            if (!Gfx_CrossFadeToImage(FUNDAMENTALS_FILE, currentLoPal, 32, nextLoPal, 32)) {
                Gfx_CloseScreenAndWindow();
                Gfx_CloseBlackScreen();
                Input_Shutdown();
                return RETURN_FAIL;
            }
        } else {
            /* Otherwise, go back to TRNGINFO */
            for (int i = 0; i < 32; i++) {
                nextLoPal[i] = trngInfoPalette[i];
            }

            if (!Gfx_CrossFadeToImage(TRNGINFO_FILE, currentLoPal, 32, nextLoPal, 32)) {
                Gfx_CloseScreenAndWindow();
                Gfx_CloseBlackScreen();
                Input_Shutdown();
                return RETURN_FAIL;
            }
        }

        /* Commit palette change. */
        for (int i = 0; i < 32; i++) {
            currentLoPal[i] = nextLoPal[i];
        }

        /* If the user engaged, finish this loop step and enter the range. */
        if (engaged) {
            if (!Gfx_CrossFadeToImage(RANGE_FILE, currentLoPal, 32, oahuRangePalette, 32)) {
                Gfx_CloseScreenAndWindow();
                Gfx_CloseBlackScreen();
                Input_Shutdown();
                return RETURN_FAIL;
            }

            /* Range is the last screen for now: wait for ESC or Fire/LMB to exit. */
            for (;;) {
                WaitResult rr = WaitForAdvanceOrTimeout(3600);
                if (rr == WAIT_ESC || rr == WAIT_ADVANCE) {
                    goto exit_ok;
                }
            }
        }
    }

exit_ok:
    Gfx_CloseScreenAndWindow();
    Gfx_CloseBlackScreen();
    Input_Shutdown();
    return RETURN_OK;
}