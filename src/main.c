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
#define IMAGE_FILE "gfx/logo.raw"

struct GfxBase *GfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct Screen *screen = NULL;
struct Window *window = NULL;

/* Paleta RGB4 (0–15) */
UWORD palette[16] = {
    0x000, 0xEEE, 0x569, 0xAAA,
    0x129, 0x333, 0x888, 0x555,
    0x449, 0xCCC, 0x222, 0x88A,
    0x777, 0xDDD, 0xBBB, 0x77A
};

/* --- Zwolnienie zasobów --- */
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

/* --- Wczytanie pliku RAW do tymczasowej bitmapy i kopiowanie do ekranu --- */
BOOL drawRawToWindow(const char *filename, struct Window *win)
{
    BPTR fh;
    struct BitMap *tmp;
    LONG bytesPerRow = WIDTH / 8;
    LONG planeSize = bytesPerRow * HEIGHT;

    fh = Open((STRPTR)filename, MODE_OLDFILE);
    if (!fh) {
        Printf("Nie można otworzyć pliku %s\n", (ULONG)filename);
        return FALSE;
    }

    tmp = AllocBitMap(WIDTH, HEIGHT, DEPTH, BMF_CLEAR, NULL);
    if (!tmp) {
        Printf("Błąd: nie można zaalokować bitmapy tymczasowej\n");
        Close(fh);
        return FALSE;
    }

    for (int i = 0; i < DEPTH; i++) {
        if (Read(fh, tmp->Planes[i], planeSize) != planeSize) {
            Printf("Błąd odczytu płaszczyzny %ld\n", i);
            FreeBitMap(tmp);
            Close(fh);
            return FALSE;
        }
    }
    Close(fh);

    /* --- Blit do bitmapy okna --- */
    BltBitMap(tmp, 0, 0, win->RPort->BitMap, 0, 0, WIDTH, HEIGHT, 0xC0, 0xFF, NULL);
    WaitBlit();
    WaitTOF();

    FreeBitMap(tmp);
    return TRUE;
}

/* --- Główna funkcja --- */
int main(void)
{
    struct IntuiMessage *msg;
    BOOL done = FALSE;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 36);
    if (!IntuitionBase || !GfxBase) {
        Printf("Nie można otworzyć bibliotek systemowych\n");
        cleanup();
        return RETURN_FAIL;
    }

    /* Otwórz ekran */
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
        Printf("Nie można otworzyć ekranu\n");
        cleanup();
        return RETURN_FAIL;
    }

    /* Otwórz okno */
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
        Printf("Nie można otworzyć okna\n");
        cleanup();
        return RETURN_FAIL;
    }

    /* Ustaw paletę i wyświetl obraz */
    LoadRGB4(&screen->ViewPort, palette, 16);
    WaitTOF();

    if (!drawRawToWindow(IMAGE_FILE, window)) {
        Printf("Nie można wczytać grafiki\n");
        cleanup();
        return RETURN_FAIL;
    }

    /* --- Główna pętla --- */
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
