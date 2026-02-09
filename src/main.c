#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

#include "imageHandler.h"
#include "input.h"

/* HiRes screens (LOGO + TITLE) */
#define HI_WIDTH   640
#define HI_HEIGHT  256
#define HI_DEPTH   4

/* LoRes screen (TRNGINFO) */
#define LO_WIDTH   320
#define LO_HEIGHT  256
#define LO_DEPTH   5

#define LOGO_FILE      "gfx/LOGO.RAW"
#define TITLE_FILE     "gfx/TITLE.RAW"
#define TRNGINFO_FILE  "gfx/TRNGINFO.RAW"

/* LOGO palette (16 colors RGB4) */
static UWORD logoPalette[16] = {
    0x000, 0xEEE, 0x569, 0xAAA,
    0x129, 0x333, 0x888, 0x555,
    0x449, 0xCCC, 0x222, 0x88A,
    0x777, 0xDDD, 0xBBB, 0x77A
};

/* TITLE palette (16 colors RGB4) */
static UWORD titlePalette[16] = {
    0x221, 0xC01, 0x03E, 0x259,
    0x456, 0x653, 0xB59, 0x38E,
    0xD83, 0x69E, 0x7AB, 0xD88,
    0xAA8, 0xABD, 0xEDA, 0xDDD
};

/* TRNGINFO palette (32 colors RGB4) */
static UWORD trngInfoPalette[32] = {
    0x0000,0x0775,0x0443,0x0232,0x0211,0x0333,0x0322,0x0111,
    0x0100,0x0010,0x0000,0x0000,0x0000,0x0322,0x0332,0x0332,
    0x0332,0x0AA9,0x0CCB,0x0555,0x0888,0x0654,0x0CCC,0x0BBB,
    0x0999,0x0777,0x0986,0x0444,0x0EDD,0x0BAA,0x0665,0x0999
};

static struct Screen *screen = NULL;
static struct Window *window = NULL;

/* Invisible pointer (1x1) */
static UWORD blankPointer[] = { 0x0000, 0x0000 };

static void HidePointer(struct Window *win)
{
    if (win) SetPointer(win, blankPointer, 1, 1, 0, 0);
}

static void ShowPointer(struct Window *win)
{
    if (win) ClearPointer(win);
}

static void CloseScreenAndWindow(void)
{
    /* Restore pointer before closing window */
    ShowPointer(window);

    if (window) {
        CloseWindow(window);
        window = NULL;
        Delay(2);
    }
    if (screen) {
        CloseScreen(screen);
        screen = NULL;
        Delay(2);
    }
}

static BOOL OpenScreenAndWindow(UWORD width, UWORD height, UBYTE depth, ULONG displayID)
{
    screen = OpenScreenTags(NULL,
        SA_Width, width,
        SA_Height, height,
        SA_Depth, depth,
        SA_DisplayID, displayID,
        SA_Type, CUSTOMSCREEN,
        SA_ShowTitle, FALSE,
        SA_Quiet, TRUE,
        SA_BackFill, LAYERS_NOBACKFILL,
        TAG_DONE);

    if (!screen) {
        Printf("AMACS: Cannot open screen\n");
        return FALSE;
    }

    window = OpenWindowTags(NULL,
        WA_CustomScreen, (ULONG)screen,
        WA_Width, width,
        WA_Height, height,
        WA_Borderless, TRUE,
        WA_Backdrop, TRUE,
        WA_Activate, TRUE,
        WA_RMBTrap, TRUE,
        WA_IDCMP, IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS,
        TAG_DONE);

    if (!window) {
        Printf("AMACS: Cannot open window\n");
        return FALSE;
    }

    HidePointer(window);
    return TRUE;
}

/* ESC / LMB / Fire */
static BOOL IsActionPressed(void)
{
    struct IntuiMessage *msg;

    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort))) {
        BOOL hit = FALSE;

        if (msg->Class == IDCMP_RAWKEY && msg->Code == 0x45) {
            hit = TRUE; /* ESC */
        } else if (msg->Class == IDCMP_MOUSEBUTTONS && msg->Code == SELECTDOWN) {
            hit = TRUE; /* LMB */
        }

        ReplyMsg((struct Message *)msg);
        if (hit) return TRUE;
    }

    if (IsJoystickFirePressed()) return TRUE;
    return FALSE;
}

/* Wait press, then wait release (debounce) */
static void WaitForActionWithDebounce(void)
{
    while (!IsActionPressed()) WaitTOF();

    /* small delay to prevent double-trigger */
    WaitTOF(); WaitTOF();

    /* wait until joystick fire is released */
    while (IsJoystickFirePressed()) WaitTOF();

    /* flush any pending Intuition messages */
    while (GetMsg(window->UserPort)) { /* discard */ }

    WaitTOF(); WaitTOF();
}

static BOOL ShowImageAndPalette(const char *file, UWORD *pal, UWORD colorCount)
{
    LoadRGB4(&screen->ViewPort, pal, colorCount);
    WaitTOF(); WaitTOF();

    if (!LoadRawImageToScreen(file, screen)) {
        Printf("AMACS: Cannot load %s\n", (ULONG)file);
        return FALSE;
    }

    ScreenToFront(screen);
    RemakeDisplay();
    WaitTOF(); WaitTOF();
    return TRUE;
}

static BOOL SwitchHiResToLoResNoWorkbenchFlash(void)
{
    /* Keep old handles */
    struct Screen *oldScreen = screen;
    struct Window *oldWindow = window;

    /* Open new LoRes first */
    screen = NULL;
    window = NULL;

    if (!OpenScreenAndWindow(LO_WIDTH, LO_HEIGHT, LO_DEPTH, LORES_KEY)) {
        /* Restore old if we failed */
        screen = oldScreen;
        window = oldWindow;
        return FALSE;
    }

    /* Bring new screen to front immediately */
    ScreenToFront(screen);
    RemakeDisplay();
    WaitTOF(); WaitTOF();

    /* Now it is safe to close the old HiRes screen/window */
    ShowPointer(oldWindow);
    CloseWindow(oldWindow);
    CloseScreen(oldScreen);
    Delay(2);

    return TRUE;
}

int main(void)
{
    if (!Input_Init()) {
        Printf("AMACS: Input init failed\n");
        return RETURN_FAIL;
    }

    /* --- LOGO (HiRes) --- */
    if (!OpenScreenAndWindow(HI_WIDTH, HI_HEIGHT, HI_DEPTH, HIRES_KEY)) {
        CloseScreenAndWindow();
        Input_Shutdown();
        return RETURN_FAIL;
    }
    if (!ShowImageAndPalette(LOGO_FILE, logoPalette, 16)) {
        CloseScreenAndWindow();
        Input_Shutdown();
        return RETURN_FAIL;
    }
    WaitForActionWithDebounce();

    /* --- TITLE (HiRes, same screen) --- */
    if (!ShowImageAndPalette(TITLE_FILE, titlePalette, 16)) {
        CloseScreenAndWindow();
        Input_Shutdown();
        return RETURN_FAIL;
    }
    WaitForActionWithDebounce();

    /* --- Switch to LoRes WITHOUT Workbench flash --- */
    if (!SwitchHiResToLoResNoWorkbenchFlash()) {
        /* If switching fails, close current (likely old) and exit */
        CloseScreenAndWindow();
        Input_Shutdown();
        return RETURN_FAIL;
    }

    /* --- TRNGINFO (LoRes) --- */
    if (!ShowImageAndPalette(TRNGINFO_FILE, trngInfoPalette, 32)) {
        CloseScreenAndWindow();
        Input_Shutdown();
        return RETURN_FAIL;
    }
    WaitForActionWithDebounce();

    /* Exit */
    CloseScreenAndWindow();
    Input_Shutdown();
    return RETURN_OK;
}
