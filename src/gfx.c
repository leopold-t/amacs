#include "gfx.h"

#include <exec/ports.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "imageHandler.h"

/* Module-owned handles */
static struct Screen *blackScreen = NULL;
static struct Screen *screen = NULL;
static struct Window *window = NULL;

/* Double buffering */
static BOOL dbufEnabled = FALSE;
static struct ScreenBuffer *screenBuffers[2] = {NULL, NULL};
/* sbIndex = index aktualnie WYŚWIETLANEGO bufora */
static WORD sbIndex = 0;
static struct RastPort drawRP; /* RastPort podpięty pod back buffer */

/* DBufInfo messages */
static struct MsgPort *dbufPort = NULL;
static BOOL dbufPrimed = FALSE;

/*
 * Invisible pointer.
 *
 * IMPORTANT:
 * Pointer image data must live in CHIP RAM on real Amiga hardware.
 * A normal static array may end up outside CHIP RAM and produce
 * corrupted vertical bars / sprite glitches.
 */
static UWORD *blankPointer = NULL;

static BOOL InitBlankPointer(void) {
    if (blankPointer) {
        return TRUE;
    }

    /* Height = 1 line, 2 planes => 2 words total */
    blankPointer = (UWORD *)AllocMem(2 * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
    return (blankPointer != NULL);
}

static void FreeBlankPointer(void) {
    if (blankPointer) {
        FreeMem(blankPointer, 2 * sizeof(UWORD));
        blankPointer = NULL;
    }
}

static void HidePointer(struct Window *win) {
    if (win && blankPointer) {
        SetPointer(win, blankPointer, 1, 1, 0, 0);
    }
}

static void ShowPointer(struct Window *win) {
    if (win) {
        ClearPointer(win);
    }
}

/* ---- Stability helpers ---- */

static void SettleDisplay(int frames) {
    for (int i = 0; i < frames; i++) {
        WaitTOF();
    }
}

static void DrainIDCMP(struct Window *win) {
    if (!win || !win->UserPort) {
        return;
    }

    struct IntuiMessage *msg;
    while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {
        ReplyMsg((struct Message *)msg);
    }
}

/* ---------------- Fade helpers (RGB4 up to 32 colors) ---------------- */

static UWORD LerpRGB4(UWORD a, UWORD b, int step, int steps) {
    int ar = (a >> 8) & 0xF, ag = (a >> 4) & 0xF, ab = a & 0xF;
    int br = (b >> 8) & 0xF, bg = (b >> 4) & 0xF, bb = b & 0xF;

    int r = ar + (br - ar) * step / steps;
    int g = ag + (bg - ag) * step / steps;
    int bl = ab + (bb - ab) * step / steps;

    return (UWORD)((r << 8) | (g << 4) | bl);
}

static void FadeToPalette(struct ViewPort *vp, const UWORD *from, const UWORD *to, UWORD colors,
                          int steps, int framesPerStep) {
    static UWORD tmp[32];

    for (int s = 0; s <= steps; s++) {
        for (UWORD i = 0; i < colors; i++) {
            tmp[i] = LerpRGB4(from[i], to[i], s, steps);
        }

        LoadRGB4(vp, tmp, colors);

        for (int f = 0; f < framesPerStep; f++) {
            WaitTOF();
        }
    }

    SettleDisplay(1);
}

static void FadeOutToBlack(struct Screen *scr, const UWORD *currentPal, UWORD colors) {
    UWORD black[32] = {0};
    FadeToPalette(&scr->ViewPort, currentPal, black, colors, 12, 1);
}

static void FadeInFromBlack(struct Screen *scr, const UWORD *targetPal, UWORD colors) {
    UWORD black[32] = {0};
    FadeToPalette(&scr->ViewPort, black, targetPal, colors, 12, 1);
}

/* ---------------- Double buffering helpers ---------------- */

static void WaitSafe(struct ScreenBuffer *sb) {
    if (!sb || !sb->sb_DBufInfo) {
        return;
    }

    struct MsgPort *port = sb->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort;
    if (!port) {
        return;
    }

    while (!GetMsg(port)) {
        WaitPort(port);
    }
}

BOOL Gfx_EnableDoubleBuffering(void) {
    if (dbufEnabled) {
        return TRUE;
    }
    if (!screen) {
        return FALSE;
    }

    if (!dbufPort) {
        dbufPort = CreateMsgPort();
        if (!dbufPort) {
            return FALSE;
        }
    }

    /* buffer 0 = bitmap ekranu */
    screenBuffers[0] = AllocScreenBuffer(screen, NULL, SB_SCREEN_BITMAP);
    if (!screenBuffers[0]) {
        Gfx_DisableDoubleBuffering();
        return FALSE;
    }

    /* buffer 1 = NOWY dodatkowy bitmap (to jest klucz!) */
    screenBuffers[1] = AllocScreenBuffer(screen, NULL, 0);
    if (!screenBuffers[1]) {
        Gfx_DisableDoubleBuffering();
        return FALSE;
    }

    /* Podpinamy MsgPort do DBufInfo */
    for (int i = 0; i < 2; i++) {
        if (screenBuffers[i] && screenBuffers[i]->sb_DBufInfo) {
            screenBuffers[i]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = dbufPort;
            screenBuffers[i]->sb_DBufInfo->dbi_DispMessage.mn_ReplyPort = dbufPort;
        }
    }

    sbIndex = 0; /* pokazujemy buffer 0 */
    dbufPrimed = FALSE;

    InitRastPort(&drawRP);
    drawRP.BitMap = screenBuffers[1]->sb_BitMap; /* rysujemy do back buffera */

    /* wyczyść back buffer */
    SetRast(&drawRP, 0);
    WaitBlit();

    dbufEnabled = TRUE;
    return TRUE;
}

void Gfx_DisableDoubleBuffering(void) {
    if (!dbufEnabled && !screenBuffers[0] && !screenBuffers[1] && !dbufPort) {
        return;
    }

    /* Wróć na buffer 0 (bitmap ekranu) */
    if (screen && screenBuffers[0]) {
        ChangeScreenBuffer(screen, screenBuffers[0]);
        WaitTOF();
        WaitTOF();
    }

    if (screenBuffers[1]) {
        FreeScreenBuffer(screen, screenBuffers[1]);
        screenBuffers[1] = NULL;
    }
    if (screenBuffers[0]) {
        FreeScreenBuffer(screen, screenBuffers[0]);
        screenBuffers[0] = NULL;
    }

    dbufEnabled = FALSE;
    dbufPrimed = FALSE;

    if (dbufPort) {
        DeleteMsgPort(dbufPort);
        dbufPort = NULL;
    }
}

BOOL Gfx_IsDoubleBufferingEnabled(void) {
    return dbufEnabled;
}

struct RastPort *Gfx_GetDrawRastPort(void) {
    if (dbufEnabled) {
        return &drawRP;
    }
    if (screen) {
        return &screen->RastPort;
    }
    return NULL;
}

void Gfx_SwapBuffers(void) {
    if (!dbufEnabled || !screen) {
        return;
    }

    WORD newShow = 1 - sbIndex; /* ten, który chcemy pokazać */
    struct ScreenBuffer *showBuf = screenBuffers[newShow];

    /* Flip */
    if (!ChangeScreenBuffer(screen, showBuf)) {
        return;
    }

    /* Po flipie: ten bufor staje się wyświetlany */
    sbIndex = newShow;

    /* NOWY back buffer = przeciwny do wyświetlanego */
    drawRP.BitMap = screenBuffers[1 - sbIndex]->sb_BitMap;

    /* I dopiero TERAZ czekamy aż back buffer będzie „safe” do rysowania */
    if (dbufPrimed) {
        WaitSafe(screenBuffers[1 - sbIndex]);
    } else {
        dbufPrimed = TRUE;
    }
}

/* ---------------- Public API ---------------- */

struct Screen *Gfx_GetScreen(void) {
    return screen;
}

struct Window *Gfx_GetWindow(void) {
    return window;
}

BOOL Gfx_OpenBlackScreen(UWORD width, UWORD height, UBYTE depth) {
    struct TagItem tags[] = {{SA_Width, width},
                             {SA_Height, height},
                             {SA_Depth, depth},
                             {SA_DisplayID, LORES_KEY},
                             {SA_Type, CUSTOMSCREEN},
                             {SA_ShowTitle, FALSE},
                             {SA_Quiet, TRUE},
                             {SA_Behind, TRUE},
                             {SA_BackFill, (ULONG)LAYERS_NOBACKFILL},
                             {SA_Interleaved, FALSE},
                             {TAG_DONE, 0}};

    blackScreen = OpenScreenTagList(NULL, tags);
    if (!blackScreen) {
        return FALSE;
    }

    SetRast(&blackScreen->RastPort, 0);
    WaitBlit();
    WaitTOF();
    WaitTOF();

    {
        UWORD black4[4] = {0x000, 0x000, 0x000, 0x000};

        LoadRGB4(&blackScreen->ViewPort, black4, (depth >= 2) ? 4 : 2);
        ScreenToBack(blackScreen);
        RemakeDisplay();
        WaitTOF();
        WaitTOF();
    }

    return TRUE;
}

void Gfx_CloseBlackScreen(void) {
    if (blackScreen) {
        WaitBlit();
        SettleDisplay(1);
        CloseScreen(blackScreen);
        blackScreen = NULL;
        SettleDisplay(2);
    }
}

BOOL Gfx_OpenScreenAndWindow(UWORD width, UWORD height, UBYTE depth, ULONG displayID) {
    struct TagItem screenTags[] = {{SA_Width, width},       {SA_Height, height},
                                   {SA_Depth, depth},       {SA_DisplayID, displayID},
                                   {SA_Type, CUSTOMSCREEN}, {SA_ShowTitle, FALSE},
                                   {SA_Quiet, TRUE},        {SA_BackFill, (ULONG)LAYERS_NOBACKFILL},
                                   {SA_Interleaved, FALSE}, {TAG_DONE, 0}};

    screen = OpenScreenTagList(NULL, screenTags);
    if (!screen) {
        return FALSE;
    }

    SetRast(&screen->RastPort, 0);
    WaitBlit();
    WaitTOF();
    WaitTOF();

    struct TagItem windowTags[] = {{WA_CustomScreen, (ULONG)screen},
                                   {WA_Width, width},
                                   {WA_Height, height},
                                   {WA_Borderless, TRUE},
                                   {WA_Backdrop, TRUE},
                                   {WA_Activate, TRUE},
                                   {WA_RMBTrap, TRUE},
                                   {WA_IDCMP, IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS},
                                   {TAG_DONE, 0}};

    window = OpenWindowTagList(NULL, windowTags);
    if (!window) {
        WaitBlit();
        SettleDisplay(1);
        CloseScreen(screen);
        screen = NULL;
        return FALSE;
    }

    if (!InitBlankPointer()) {
        ShowPointer(window);
    } else {
        HidePointer(window);
    }

    SettleDisplay(1);
    return TRUE;
}

void Gfx_CloseScreenAndWindow(void) {
    DrainIDCMP(window);

    WaitBlit();
    SettleDisplay(1);

    if (dbufEnabled) {
        Gfx_DisableDoubleBuffering();
    }

    ShowPointer(window);

    if (window) {
        CloseWindow(window);
        window = NULL;
        SettleDisplay(2);
    }

    /* Pointer image no longer needed once app window is gone */
    FreeBlankPointer();

    WaitBlit();
    SettleDisplay(1);

    if (screen) {
        CloseScreen(screen);
        screen = NULL;
        SettleDisplay(2);
    }
}

void Gfx_FadeOutCurrentScreenToBlack(const UWORD *currentPal, UWORD colors) {
    if (!screen || !currentPal || colors == 0) {
        return;
    }

    FadeOutToBlack(screen, currentPal, colors);
    WaitBlit();
    SettleDisplay(1);
}

BOOL Gfx_ShowImageFadeInFromBlack(const char *file, const UWORD *targetPal, UWORD colors) {
    UWORD black[32] = {0};

    if (!screen) {
        return FALSE;
    }

    LoadRGB4(&screen->ViewPort, black, colors);
    SettleDisplay(2);

    if (!LoadRawImageToScreen(file, screen)) {
        return FALSE;
    }

    WaitBlit();
    SettleDisplay(1);

    ScreenToFront(screen);
    RemakeDisplay();
    SettleDisplay(2);

    FadeInFromBlack(screen, targetPal, colors);
    return TRUE;
}

BOOL Gfx_CrossFadeToImage(const char *file, const UWORD *fromPal, UWORD fromColors,
                          const UWORD *toPal, UWORD toColors) {
    if (!screen) {
        return FALSE;
    }

    FadeOutToBlack(screen, fromPal, fromColors);

    {
        UWORD black[32] = {0};
        LoadRGB4(&screen->ViewPort, black, toColors);
        SettleDisplay(2);
    }

    if (!LoadRawImageToScreen(file, screen)) {
        return FALSE;
    }

    WaitBlit();
    SettleDisplay(1);

    ScreenToFront(screen);
    RemakeDisplay();
    SettleDisplay(2);

    FadeInFromBlack(screen, toPal, toColors);
    return TRUE;
}

BOOL Gfx_SwitchHiResToLoResOnBlack(const UWORD *currentHiPal16, UWORD loWidth, UWORD loHeight,
                                   UBYTE loDepth) {
    struct Screen *oldScreen = screen;
    struct Window *oldWindow = window;

    if (!oldScreen || !oldWindow) {
        return FALSE;
    }

    FadeOutToBlack(oldScreen, currentHiPal16, 16);
    SettleDisplay(2);

    screen = NULL;
    window = NULL;

    if (!Gfx_OpenScreenAndWindow(loWidth, loHeight, loDepth, LORES_KEY)) {
        screen = oldScreen;
        window = oldWindow;
        return FALSE;
    }

    {
        UWORD black32[32] = {0};
        LoadRGB4(&screen->ViewPort, black32, 32);

        WaitBlit();
        SettleDisplay(1);

        ScreenToFront(screen);
        RemakeDisplay();
        SettleDisplay(2);
    }

    DrainIDCMP(oldWindow);
    ShowPointer(oldWindow);

    WaitBlit();
    SettleDisplay(1);

    CloseWindow(oldWindow);

    WaitBlit();
    SettleDisplay(1);

    CloseScreen(oldScreen);
    SettleDisplay(2);

    return TRUE;
}