#include <exec/types.h>
#include <intuition/intuition.h>
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

/* RANGE (Oahu) palette (32 colors RGB4) */
static UWORD oahuRangePalette[32] = {
    0x0000, 0x09BD, 0x07AD, 0x069D, 0x0110, 0x0110, 0x0220, 0x0230, 0x0341, 0x0453, 0x0563,
    0x0674, 0x0210, 0x0321, 0x0432, 0x0543, 0x0654, 0x0765, 0x0876, 0x0987, 0x0444, 0x0DDE,
    0x0FFF, 0x0FFF, 0x0000, 0x0111, 0x0222, 0x0333, 0x0444, 0x0555, 0x0888, 0x0BBB};

/* Input helper: ESC / LMB / Fire */
static BOOL IsActionPressed(void) {
    struct Window *win = Gfx_GetWindow();
    struct IntuiMessage *msg;

    /* Drain window messages and detect ESC/LMB. */
    while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {
        BOOL hit = FALSE;

        if (msg->Class == IDCMP_RAWKEY && msg->Code == 0x45) {
            hit = TRUE; /* ESC */
        }
        if (msg->Class == IDCMP_MOUSEBUTTONS && msg->Code == SELECTDOWN) {
            hit = TRUE; /* LMB */
        }

        ReplyMsg((struct Message *)msg);

        if (hit) {
            return TRUE;
        }
    }

    /* Also accept joystick Fire. */
    return IsJoystickFirePressed() ? TRUE : FALSE;
}

static void WaitForActionWithDebounce(void) {
    /* Wait for press. */
    while (!IsActionPressed()) {
        WaitTOF();
    }

    /* Small debounce. */
    WaitTOF();
    WaitTOF();

    /* If Fire is held, wait for release to avoid double-skip. */
    while (IsJoystickFirePressed()) {
        WaitTOF();
    }

    /* Drain any queued mouse/key events. */
    while (GetMsg(Gfx_GetWindow()->UserPort)) {
        /* discard */
    }

    WaitTOF();
    WaitTOF();
}

int main(void) {
    if (!Input_Init()) {
        return RETURN_FAIL;
    }

    if (!Gfx_OpenBlackScreen(BLK_WIDTH, BLK_HEIGHT, BLK_DEPTH)) {
        Input_Shutdown();
        return RETURN_FAIL;
    }

    /* HiRes intro: LOGO */
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
    WaitForActionWithDebounce();

    /* HiRes intro: TITLE (crossfade) */
    if (!Gfx_CrossFadeToImage(TITLE_FILE, logoPalette, 16, titlePalette, 16)) {
        Gfx_CloseScreenAndWindow();
        Gfx_CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }
    WaitForActionWithDebounce();

    /* Switch to LoRes */
    if (!Gfx_SwitchHiResToLoResOnBlack(titlePalette, LO_WIDTH, LO_HEIGHT, LO_DEPTH)) {
        Gfx_CloseScreenAndWindow();
        Gfx_CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }

    /* LoRes: TRNGINFO */
    if (!Gfx_ShowImageFadeInFromBlack(TRNGINFO_FILE, trngInfoPalette, 32)) {
        Gfx_CloseScreenAndWindow();
        Gfx_CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }
    WaitForActionWithDebounce();

    /* LoRes: FUNDAMENTALS (crossfade) */
    if (!Gfx_CrossFadeToImage(FUNDAMENTALS_FILE, trngInfoPalette, 32, fundamentalsPalette, 32)) {
        Gfx_CloseScreenAndWindow();
        Gfx_CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }
    WaitForActionWithDebounce();

    /* LoRes: RANGE (crossfade) */
    if (!Gfx_CrossFadeToImage(RANGE_FILE, fundamentalsPalette, 32, oahuRangePalette, 32)) {
        Gfx_CloseScreenAndWindow();
        Gfx_CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }
    WaitForActionWithDebounce();

    Gfx_CloseScreenAndWindow();
    Gfx_CloseBlackScreen();
    Input_Shutdown();
    return RETURN_OK;
}