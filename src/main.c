#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

#include "imageHandler.h"
#include "input.h"

#define WIDTH   640
#define HEIGHT  256
#define DEPTH   4

#define LOGO_FILE  "gfx/LOGO.RAW"
#define TITLE_FILE "gfx/TITLE.RAW"

/* LOGO palette (RGB4) */
static UWORD logoPalette[16] = {
    0x000, 0xEEE, 0x569, 0xAAA,
    0x129, 0x333, 0x888, 0x555,
    0x449, 0xCCC, 0x222, 0x88A,
    0x777, 0xDDD, 0xBBB, 0x77A
};

/* TITLE palette (RGB4) */
static UWORD titlePalette[16] = {
    0x221, 0xC01, 0x03E, 0x259,
    0x456, 0x653, 0xB59, 0x38E,
    0xD83, 0x69E, 0x7AB, 0xD88,
    0xAA8, 0xABD, 0xEDA, 0xDDD
};

static struct Screen *screen = NULL;
static struct Window *window = NULL;

/* Invisible mouse pointer (1x1) */
static UWORD blankPointer[] = { 0x0000, 0x0000 };

static void HidePointer(struct Window *win)
{
    if (win) {
        SetPointer(win, blankPointer, 1, 1, 0, 0);
    }
}

static void ShowPointer(struct Window *win)
{
    if (win) {
        ClearPointer(win);
    }
}

static void cleanup(void)
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

    Input_Shutdown();
}

/* Poll ESC/LMB/Fire. Returns TRUE when an action is detected. */
static BOOL IsActionPressed(void)
{
    struct IntuiMessage *msg;

    /* Check Intuition messages (ESC / LMB) */
    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort))) {
        BOOL hit = FALSE;

        if (msg->Class == IDCMP_RAWKEY && msg->Code == 0x45) {
            hit = TRUE; /* ESC */
        } else if (msg->Class == IDCMP_MOUSEBUTTONS && msg->Code == SELECTDOWN) {
            hit = TRUE; /* LMB */
        }

        ReplyMsg((struct Message *)msg);

        if (hit)
            return TRUE;
    }

    /* Check joystick fire */
    if (IsJoystickFirePressed())
        return TRUE;

    return FALSE;
}

/* Wait until any action is pressed, then wait until it is released (debounce). */
static void WaitForActionWithDebounce(void)
{
    /* Wait for press */
    while (!IsActionPressed()) {
        WaitTOF();
    }

    /* Small delay to avoid immediate re-trigger */
    WaitTOF();
    WaitTOF();

    /* Wait for release (important for joystick fire) */
    while (IsJoystickFirePressed()) {
        WaitTOF();
    }

    /* Also flush any pending Intuition messages (e.g., mouse up) */
    while (GetMsg(window->UserPort)) {
        /* discard */
    }

    /* Another tiny delay after release */
    WaitTOF();
    WaitTOF();
}

static BOOL OpenMainScreen(void)
{
    screen = OpenScreenTags(NULL,
        SA_Width, WIDTH,
        SA_Height, HEIGHT,
        SA_Depth, DEPTH,
        SA_DisplayID, HIRES_KEY,
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
        WA_Width, WIDTH,
        WA_Height, HEIGHT,
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

static BOOL ShowScreenImage(const char *file, UWORD *pal)
{
    /* Set palette first */
    LoadRGB4(&screen->ViewPort, pal, 16);

    /* Sync */
    WaitTOF();
    WaitTOF();

    /* Load planar RAW into screen bitmap */
    if (!LoadRawImageToScreen(file, screen)) {
        Printf("AMACS: Cannot load %s\n", (ULONG)file);
        return FALSE;
    }

    /* Bring to front and refresh */
    ScreenToFront(screen);
    RemakeDisplay();
    WaitTOF();
    WaitTOF();

    return TRUE;
}

int main(void)
{
    if (!Input_Init()) {
        Printf("AMACS: Cannot init input (lowlevel)\n");
        return RETURN_FAIL;
    }

    if (!OpenMainScreen()) {
        cleanup();
        return RETURN_FAIL;
    }

    /* LOGO */
    if (!ShowScreenImage(LOGO_FILE, logoPalette)) {
        cleanup();
        return RETURN_FAIL;
    }
    WaitForActionWithDebounce();

    /* TITLE */
    if (!ShowScreenImage(TITLE_FILE, titlePalette)) {
        cleanup();
        return RETURN_FAIL;
    }
    WaitForActionWithDebounce();

    cleanup();
    return RETURN_OK;
}
