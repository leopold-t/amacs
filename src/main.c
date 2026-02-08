#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

#define WIDTH 640
#define HEIGHT 256
#define DEPTH 4
#define IMAGE_FILE "gfx/Logo.raw"

struct GfxBase *GfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct Screen *screen = NULL;
struct Window *window = NULL;

/* High Res RGB4 (0–15) color palette */
UWORD palette[16] = {
    0x000, 0xEEE, 0x569, 0xAAA,
    0x129, 0x333, 0x888, 0x555,
    0x449, 0xCCC, 0x222, 0x88A,
    0x777, 0xDDD, 0xBBB, 0x77A
};

/* --- Resource release --- */
void cleanup(void)
{
    struct IntuiMessage *msg;

    if (window && window->UserPort) {
        while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort))) {
            ReplyMsg((struct Message *)msg);
        }
    }

    if (window) {
        window->UserPort = NULL;
        CloseWindow(window);
        window = NULL;
        Delay(5);
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

/* --- Loading RAW file into temporary bitmap and copying it to screen --- */
BOOL drawRawToWindow(const char *filename, struct Window *win)
{
    BPTR fh;
    struct BitMap *tmp;
    LONG bytesPerRow = WIDTH / 8;
    LONG planeSize = bytesPerRow * HEIGHT;

    fh = Open((STRPTR)filename, MODE_OLDFILE);
    if (!fh) {
        Printf("Unable to open file %s\n", (ULONG)filename);
        return FALSE;
    }

    tmp = AllocBitMap(WIDTH, HEIGHT, DEPTH, BMF_CLEAR, NULL);

    if (!tmp) {
        Printf("Error: unable to allocate temporary bitmap\n");
        Close(fh);
        return FALSE;
    }

    for (int i = 0; i < DEPTH; i++) {
        if (Read(fh, tmp->Planes[i], planeSize) != planeSize) {
            Printf("Error reading bitplane %ld\n", i);
            FreeBitMap(tmp);
            Close(fh);
            return FALSE;
        }
    }

    Close(fh);

    /* --- Blit for window bitmap --- */
    BltBitMap(tmp, 0, 0, win->RPort->BitMap, 0, 0, WIDTH, HEIGHT, 0xC0, 0xFF, NULL);
    WaitBlit();
    WaitTOF();

    FreeBitMap(tmp);
    return TRUE;
}

/* --- Main program function --- */
int main(void)
{
    struct IntuiMessage *msg;
    BOOL done = FALSE;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 36);

    if (!IntuitionBase || !GfxBase) {
        Printf("Unable to open system libraries\n");
        cleanup();
        return RETURN_FAIL;
    }

    /* Screen opening */
    screen = (struct Screen *)OpenScreenTags(NULL,
        SA_Width, WIDTH,
        SA_Height, HEIGHT,
        SA_Depth, DEPTH,
        SA_DisplayID, HIRES_KEY,
        SA_ShowTitle, FALSE,
        SA_Type, CUSTOMSCREEN,
        SA_BackFill, LAYERS_NOBACKFILL,
        TAG_DONE);

    if (!screen) {
        Printf("Unable to open screen\n");
        cleanup();
        return RETURN_FAIL;
    }

    /* Window opening */
    window = (struct Window *)OpenWindowTags(NULL,
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
        Printf("Unable to open window\n");
        cleanup();
        return RETURN_FAIL;
    }

    /* Set up palette and display image */
    LoadRGB4(&screen->ViewPort, palette, 16);
    WaitTOF();

    if (!drawRawToWindow(IMAGE_FILE, window)) {
        Printf("Unable to open image\n");
        cleanup();
        return RETURN_FAIL;
    }

    /* --- Main Loop --- */
    while (!done) {
        ULONG sig = Wait((1L << window->UserPort->mp_SigBit) | SIGBREAKF_CTRL_C);
        if (sig & SIGBREAKF_CTRL_C) break;

        if (sig & (1L << window->UserPort->mp_SigBit)) {
            while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort))) {
                if (msg->Class == IDCMP_MOUSEBUTTONS && msg->Code == SELECTDOWN) done = TRUE;
                else if (msg->Class == IDCMP_RAWKEY && msg->Code == 0x45) done = TRUE;
                ReplyMsg((struct Message *)msg);
            }
        }
    }

    cleanup();
    return RETURN_OK;
}
