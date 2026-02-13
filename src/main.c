#include <exec/types.h>
#include <graphics/gfx.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "imageHandler.h"
#include "input.h"

/* HiRes screens (LOGO + TITLE) */
#define HI_WIDTH 640
#define HI_HEIGHT 256
#define HI_DEPTH 4

/* LoRes screens (TRNGINFO + FUNDAMENTALS) */
#define LO_WIDTH 320
#define LO_HEIGHT 256
#define LO_DEPTH 5

/* Black safety screen (always behind) */
#define BLK_WIDTH 320
#define BLK_HEIGHT 256
#define BLK_DEPTH 2

#define LOGO_FILE "gfx/LOGO.RAW"
#define TITLE_FILE "gfx/TITLE.RAW"
#define TRNGINFO_FILE "gfx/TRNGINFO.RAW"
#define FUNDAMENTALS_FILE "gfx/FUNDAMENTALS.RAW"

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

static struct Screen *blackScreen = NULL; /* always behind */
static struct Screen *screen = NULL;      /* current front screen */
static struct Window *window = NULL;      /* current window */

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
}

static void FadeOutToBlack(struct Screen *scr, const UWORD *currentPal, UWORD colors) {
    UWORD black[32] = {0};
    FadeToPalette(&scr->ViewPort, currentPal, black, colors, 12, 1);
}

static void FadeInFromBlack(struct Screen *scr, const UWORD *targetPal, UWORD colors) {
    UWORD black[32] = {0};
    FadeToPalette(&scr->ViewPort, black, targetPal, colors, 12, 1);
}

/* ---------------- Screen helpers ---------------- */

static BOOL OpenBlackScreen(void) {
    struct TagItem tags[] = {{SA_Width, BLK_WIDTH},
                             {SA_Height, BLK_HEIGHT},
                             {SA_Depth, BLK_DEPTH},
                             {SA_DisplayID, LORES_KEY},
                             {SA_Type, CUSTOMSCREEN},
                             {SA_ShowTitle, FALSE},
                             {SA_Quiet, TRUE},
                             {SA_Behind, TRUE},
                             {SA_BackFill, (ULONG)LAYERS_NOBACKFILL}, /* cast avoids warning */
                             {SA_Interleaved, FALSE},
                             {TAG_DONE, 0}};

    blackScreen = OpenScreenTagList(NULL, tags);
    if (!blackScreen) {
        return FALSE;
    }

    /* Ensure it is black */
    {
        UWORD black4[4] = {0x000, 0x000, 0x000, 0x000};
        LoadRGB4(&blackScreen->ViewPort, black4, 4);
        ScreenToBack(blackScreen);
        RemakeDisplay();
        WaitTOF();
        WaitTOF();
    }

    return TRUE;
}

static void CloseBlackScreen(void) {
    if (blackScreen) {
        CloseScreen(blackScreen);
        blackScreen = NULL;
        Delay(2);
    }
}

static void CloseScreenAndWindow(void) {
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

static BOOL OpenScreenAndWindow(UWORD width, UWORD height, UBYTE depth, ULONG displayID) {
    struct TagItem screenTags[] = {
        {SA_Width, width},       {SA_Height, height},
        {SA_Depth, depth},       {SA_DisplayID, displayID},
        {SA_Type, CUSTOMSCREEN}, {SA_ShowTitle, FALSE},
        {SA_Quiet, TRUE},        {SA_BackFill, (ULONG)LAYERS_NOBACKFILL}, /* cast avoids warning */
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
    return TRUE;
}

/* ---------------- Input helpers ---------------- */

static BOOL IsActionPressed(void) {
    struct IntuiMessage *msg;

    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort))) {
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

    return IsJoystickFirePressed() ? TRUE : FALSE;
}

static void WaitForActionWithDebounce(void) {
    while (!IsActionPressed()) {
        WaitTOF();
    }

    WaitTOF();
    WaitTOF();

    while (IsJoystickFirePressed()) {
        WaitTOF();
    }

    while (GetMsg(window->UserPort)) {
        /* discard */
    }

    WaitTOF();
    WaitTOF();
}

/* ---------------- Image display helpers ---------------- */

static BOOL ShowImageFadeInFromBlack(const char *file, const UWORD *targetPal, UWORD colors) {
    UWORD black[32] = {0};

    LoadRGB4(&screen->ViewPort, black, colors);
    WaitTOF();
    WaitTOF();

    if (!LoadRawImageToScreen(file, screen)) {
        return FALSE;
    }

    ScreenToFront(screen);
    RemakeDisplay();
    WaitTOF();
    WaitTOF();

    FadeInFromBlack(screen, targetPal, colors);
    return TRUE;
}

static BOOL CrossFadeToImage(const char *file, const UWORD *fromPal, UWORD fromColors,
                             const UWORD *toPal, UWORD toColors) {
    FadeOutToBlack(screen, fromPal, fromColors);

    {
        UWORD black[32] = {0};
        LoadRGB4(&screen->ViewPort, black, toColors);
        WaitTOF();
        WaitTOF();
    }

    if (!LoadRawImageToScreen(file, screen)) {
        return FALSE;
    }

    ScreenToFront(screen);
    RemakeDisplay();
    WaitTOF();
    WaitTOF();

    FadeInFromBlack(screen, toPal, toColors);
    return TRUE;
}

/* ---------------- HiRes -> LoRes switch ---------------- */

static BOOL SwitchHiResToLoResOnBlack(const UWORD *currentHiPal16) {
    struct Screen *oldScreen = screen;
    struct Window *oldWindow = window;

    FadeOutToBlack(oldScreen, currentHiPal16, 16);

    screen = NULL;
    window = NULL;

    if (!OpenScreenAndWindow(LO_WIDTH, LO_HEIGHT, LO_DEPTH, LORES_KEY)) {
        screen = oldScreen;
        window = oldWindow;
        return FALSE;
    }

    {
        UWORD black32[32] = {0};
        LoadRGB4(&screen->ViewPort, black32, 32);
        ScreenToFront(screen);
        RemakeDisplay();
        WaitTOF();
        WaitTOF();
    }

    ShowPointer(oldWindow);
    CloseWindow(oldWindow);
    CloseScreen(oldScreen);
    Delay(2);

    return TRUE;
}

/* ---------------- main ---------------- */

int main(void) {
    if (!Input_Init()) {
        return RETURN_FAIL;
    }

    if (!OpenBlackScreen()) {
        Input_Shutdown();
        return RETURN_FAIL;
    }

    /* HiRes intro: LOGO */
    if (!OpenScreenAndWindow(HI_WIDTH, HI_HEIGHT, HI_DEPTH, HIRES_KEY)) {
        CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }

    if (!ShowImageFadeInFromBlack(LOGO_FILE, logoPalette, 16)) {
        CloseScreenAndWindow();
        CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }
    WaitForActionWithDebounce();

    /* HiRes intro: TITLE (crossfade) */
    if (!CrossFadeToImage(TITLE_FILE, logoPalette, 16, titlePalette, 16)) {
        CloseScreenAndWindow();
        CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }
    WaitForActionWithDebounce();

    /* Switch to LoRes */
    if (!SwitchHiResToLoResOnBlack(titlePalette)) {
        CloseScreenAndWindow();
        CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }

    /* LoRes: TRNGINFO */
    if (!ShowImageFadeInFromBlack(TRNGINFO_FILE, trngInfoPalette, 32)) {
        CloseScreenAndWindow();
        CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }
    WaitForActionWithDebounce();

    /* LoRes: FUNDAMENTALS (crossfade) */
    if (!CrossFadeToImage(FUNDAMENTALS_FILE, trngInfoPalette, 32, fundamentalsPalette, 32)) {
        CloseScreenAndWindow();
        CloseBlackScreen();
        Input_Shutdown();
        return RETURN_FAIL;
    }
    WaitForActionWithDebounce();

    CloseScreenAndWindow();
    CloseBlackScreen();
    Input_Shutdown();
    return RETURN_OK;
}