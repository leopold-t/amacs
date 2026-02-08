#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/screens.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "imageHandler.h"

/*
 * Main application entry point.
 * This program displays a LOGO screen, waits for user input,
 * then displays a TITLE screen, waits again, and exits.
 */

struct GfxBase *GfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct Screen *screen = NULL;
struct Window *window = NULL;

/* File paths for image assets */
#define LOGO_FILE  "gfx/Logo.raw"
#define TITLE_FILE "gfx/Title.raw"

/* Palette for LOGO (16 colors, RGB4) */
static UWORD logoPalette[16] = {
    0x000, 0xEEE, 0x569, 0xAAA,
    0x129, 0x333, 0x888, 0x555,
    0x449, 0xCCC, 0x222, 0x88A,
    0x777, 0xDDD, 0xBBB, 0x77A
};

/* Palette for TITLE (16 colors, RGB4) */
static UWORD titlePalette[16] = {
    0x221, 0xC01, 0x03E, 0x259,
    0x456, 0x653, 0xB59, 0x38E,
    0xD83, 0x69E, 0x7AB, 0xD88,
    0xAA8, 0xABD, 0xEDA, 0xDDD
};

/*
 * Cleanup function closes window, screen, and libraries.
 * It also clears any pending Intuition messages to avoid crash.
 */
void cleanup(void)
{
    struct IntuiMessage *msg;

    if (window && window->UserPort) {
        while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort))) {
            ReplyMsg((struct Message *)msg);
        }
    }

    if (window) {
        window->UserPort = NULL; /* detach port to avoid late callbacks */
        CloseWindow(window);
        window = NULL;
        Delay(5); /* short delay for system to settle */
    }

    if (screen) {
        CloseScreen(screen);
        screen = NULL;
    }

    if (IntuitionBase) {
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
    }

    if (GfxBase) {
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = NULL;
    }
}

/*
 * Wait for user to press left mouse button or ESC key.
 * Blocks until one of those events is received.
 */
void waitForInput(struct Window *win)
{
    BOOL done = FALSE;
    struct IntuiMessage *msg;

    while (!done) {
        Wait(1L << win->UserPort->mp_SigBit);

        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {
            if (msg->Class == IDCMP_MOUSEBUTTONS &&
                msg->Code == SELECTDOWN) {
                done = TRUE;
            }
            else if (msg->Class == IDCMP_RAWKEY &&
                     msg->Code == 0x45) { /* ESC */
                done = TRUE;
            }
            ReplyMsg((struct Message *)msg);
        }
    }
}

/*
 * Main program function
 */
int main(void)
{
    struct IntuiMessage *msg;
    BOOL done = FALSE;

    /* Open required libraries */
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 36);
    if (!IntuitionBase || !GfxBase) {
        Printf("AMACS: Required libraries could not be opened\n");
        cleanup();
        return RETURN_FAIL;
    }

    /* Open a custom screen (HiRes 640x256, 4 bitplanes) */
    screen = OpenScreenTags(NULL,
    SA_Width,     640,
    SA_Height,    256,
    SA_Depth,     4,
    SA_DisplayID, HIRES_KEY,
    SA_Title,     (ULONG)"AMACS",
    TAG_DONE);

    if (!screen) {
        Printf("AMACS: Unable to open custom screen\n");
        cleanup();
        return RETURN_FAIL;
    }

    /* Open a backdrop window to receive input events */
    window = (struct Window *)OpenWindowTags(NULL,
        WA_CustomScreen, (ULONG)screen,
        WA_Width, 640,
        WA_Height, 256,
        WA_Borderless, TRUE,
        WA_Backdrop, TRUE,
        WA_Activate, TRUE,
        WA_RMBTrap, TRUE,
        WA_IDCMP, IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS,
        TAG_DONE);
    if (!window) {
        Printf("AMACS: Unable to open input window\n");
        cleanup();
        return RETURN_FAIL;
    }

    /*
     * First phase: display LOGO screen
     */
    LoadRGB4(&screen->ViewPort, logoPalette, 16);
    WaitTOF(); /* sync with display */

    if (!LoadRawImageToScreen(LOGO_FILE, screen)) {
        Printf("AMACS: Failed to load logo image\n");
        cleanup();
        return RETURN_FAIL;
    }
    /* Give user time to see logo => wait for input */
    waitForInput(window);

    /*
     * Second phase: display TITLE screen
     */
    LoadRGB4(&screen->ViewPort, titlePalette, 16);
    WaitTOF(); /* sync palette change */

    if (!LoadRawImageToScreen(TITLE_FILE, screen)) {
        Printf("AMACS: Failed to load title image\n");
        cleanup();
        return RETURN_FAIL;
    }
    /* Wait for input again, then exit */
    waitForInput(window);

    /* Clean up and exit */
    cleanup();
    return RETURN_OK;
}
