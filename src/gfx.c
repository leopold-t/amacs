#include "gfx.h"

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "imageHandler.h"

/* Module-owned handles */
static struct Screen *blackScreen = NULL;
static struct Screen *screen = NULL;
static struct Window *window = NULL;

/* Invisible pointer */
static UWORD blankPointer[] = {0x0000, 0x0000};

static void HidePointer(struct Window *win) {
    if (win) {
        SetPointer(win, blankPointer, 1, 1, 0, 0);
    }
}

static void ShowPointer(struct Window *win) {
    if (win) {
        ClearPointer(win);
    }
}

/* ---- Stability helpers (safe: TOF only, no OwnBlitter/WaitBlit) ---- */

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

    /* One extra settle at the end helps under WB */
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

    /* Ensure it's black and behind. */
    {
        UWORD black4[4] = {0x000, 0x000, 0x000, 0x000};
        LoadRGB4(&blackScreen->ViewPort, black4, (depth >= 2) ? 4 : 2);
        ScreenToBack(blackScreen);
        RemakeDisplay();
        SettleDisplay(2);
    }

    return TRUE;
}

void Gfx_CloseBlackScreen(void) {
    if (blackScreen) {
        /* settle a bit before tearing down */
        SettleDisplay(2);
        CloseScreen(blackScreen);
        blackScreen = NULL;
        Delay(2);
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
        CloseScreen(screen);
        screen = NULL;
        return FALSE;
    }

    HidePointer(window);

    /* small settle right after opening helps under WB */
    SettleDisplay(1);

    return TRUE;
}

void Gfx_CloseScreenAndWindow(void) {
    /* If we have a window, flush messages first (important under WB) */
    DrainIDCMP(window);

    /* settle before closing */
    SettleDisplay(2);

    ShowPointer(window);

    if (window) {
        CloseWindow(window);
        window = NULL;
        Delay(2);
        SettleDisplay(2);
    }

    if (screen) {
        CloseScreen(screen);
        screen = NULL;
        Delay(2);
        SettleDisplay(2);
    }
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
        ScreenToFront(screen);
        RemakeDisplay();
        SettleDisplay(2);
    }

    /* Close old HiRes safely */
    DrainIDCMP(oldWindow);
    ShowPointer(oldWindow);

    /* settle just before teardown */
    SettleDisplay(2);

    CloseWindow(oldWindow);
    SettleDisplay(1);
    CloseScreen(oldScreen);

    Delay(2);
    SettleDisplay(2);

    return TRUE;
}