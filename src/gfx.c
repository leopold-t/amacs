#include "gfx.h"

#include <exec/ports.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <graphics/text.h>
#include <string.h>

#include "imageHandler.h"
#include "input.h"

/* Module-owned handles */
static struct Screen *blackScreen = NULL;
static struct Screen *screen = NULL;
static struct Window *window = NULL;

/* Global display palette mode.  The logical/original palette of the
 * currently visible screen is kept separately so a display-mode toggle can
 * always restore it exactly. */
typedef enum {
    DISPLAY_STANDARD = 0,
    DISPLAY_NIGHT_VISION,
    DISPLAY_RED_ROOM,
    DISPLAY_AMBER
} DisplayMode;

static DisplayMode displayMode = DISPLAY_STANDARD;
static UWORD logicalPalette[32];
static UWORD logicalPaletteColors = 0;

static UWORD PaletteLuminanceRGB4(UWORD rgb) {
    UWORD r = (rgb >> 8) & 0x0F;
    UWORD g = (rgb >> 4) & 0x0F;
    UWORD b = rgb & 0x0F;

    return (UWORD)((r * 30 + g * 59 + b * 11) / 100);
}

static UWORD NightVisionRGB4(UWORD rgb) {
    UWORD y = PaletteLuminanceRGB4(rgb);
    return (UWORD)(y << 4);
}

static UWORD AmberRGB4(UWORD rgb) {
    UWORD y = PaletteLuminanceRGB4(rgb);
    UWORD r, g, b;

    /* Give the amber ramp more of the luminous midtone/highlight character
     * of a classic amber CRT.  RGB4 tops out at 15, so boost luminance by
     * 25 percent and clamp before mapping it to the warm yellow-gold ramp. */
    y = (UWORD)((y * 5) / 4);
    if (y > 15) {
        y = 15;
    }

    r = y;
    g = (UWORD)((y * 7) / 8);
    b = (UWORD)(y / 4);

    return (UWORD)((r << 8) | (g << 4) | b);
}

static UWORD RedRoomRGB4(UWORD rgb) {
    UWORD y = PaletteLuminanceRGB4(rgb);

    /* Keep the red-room palette deliberately subdued: even the brightest
     * original pen reaches only about 75 percent of full red intensity. */
    y = (UWORD)((y * 3) / 4);
    return (UWORD)(y << 8);
}

static void RememberLogicalPalette(const UWORD *pal, UWORD colors) {
    UWORD i;

    if (!pal || colors == 0) {
        logicalPaletteColors = 0;
        return;
    }

    if (colors > 32) {
        colors = 32;
    }

    for (i = 0; i < colors; i++) {
        logicalPalette[i] = pal[i];
    }
    logicalPaletteColors = colors;
}

static void LoadDisplayPalette(struct ViewPort *vp, const UWORD *pal, UWORD colors) {
    UWORD transformed[32];
    UWORD i;

    if (!vp || !pal || colors == 0) {
        return;
    }

    if (displayMode == DISPLAY_STANDARD) {
        LoadRGB4(vp, pal, colors);
        return;
    }

    if (colors > 32) {
        colors = 32;
    }
    for (i = 0; i < colors; i++) {
        if (displayMode == DISPLAY_RED_ROOM) {
            transformed[i] = RedRoomRGB4(pal[i]);
        } else if (displayMode == DISPLAY_AMBER) {
            transformed[i] = AmberRGB4(pal[i]);
        } else {
            transformed[i] = NightVisionRGB4(pal[i]);
        }
    }
    LoadRGB4(vp, transformed, colors);
}

/* Double buffering */
static BOOL dbufEnabled = FALSE;
static struct ScreenBuffer *screenBuffers[2] = {NULL, NULL};
/* sbIndex = index of the currently DISPLAYED buffer */
static WORD sbIndex = 0;
static struct RastPort drawRP; /* RastPort attached to the back buffer */

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
void SettleDisplay(int frames) {
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

        LoadDisplayPalette(vp, tmp, colors);

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

    /* buffer 0 = Screen bitmap */
    screenBuffers[0] = AllocScreenBuffer(screen, NULL, SB_SCREEN_BITMAP);

    if (!screenBuffers[0]) {
        Gfx_DisableDoubleBuffering();
        return FALSE;
    }

    /* buffer 1 = NEW additional bitmap (this is the key!) */
    screenBuffers[1] = AllocScreenBuffer(screen, NULL, 0);

    if (!screenBuffers[1]) {
        Gfx_DisableDoubleBuffering();
        return FALSE;
    }

    /* Attach MsgPort to DBufInfo */
    for (int i = 0; i < 2; i++) {
        if (screenBuffers[i] && screenBuffers[i]->sb_DBufInfo) {
            screenBuffers[i]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = dbufPort;
            screenBuffers[i]->sb_DBufInfo->dbi_DispMessage.mn_ReplyPort = dbufPort;
        }
    }

    sbIndex = 0; /* display buffer 0 */
    dbufPrimed = FALSE;

    InitRastPort(&drawRP);
    drawRP.BitMap = screenBuffers[1]->sb_BitMap; /* draw to back buffer */

    /* clear back buffer */
    SetRast(&drawRP, 0);
    WaitBlit();

    dbufEnabled = TRUE;
    return TRUE;
}

void Gfx_DisableDoubleBuffering(void) {
    if (!dbufEnabled && !screenBuffers[0] && !screenBuffers[1] && !dbufPort) {
        return;
    }

    /*
     * Preserve the currently displayed frame before returning to buffer 0.
     * Otherwise, if buffer 1 is on screen, switching back to the screen bitmap
     * can briefly reveal an older range frame under the end-of-round overlay.
     */
    if (screen && screenBuffers[0] && screenBuffers[1] && sbIndex != 0) {
        WaitSafe(screenBuffers[sbIndex]);
        WaitBlit();
        BltBitMap(screenBuffers[sbIndex]->sb_BitMap, 0, 0, screenBuffers[0]->sb_BitMap, 0, 0,
                  screen->Width, screen->Height, 0xC0, 0xFF, NULL);
        WaitBlit();
    }

    /* Return to buffer 0 (screen bitmap) */
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

    WORD newShow = 1 - sbIndex; /* the one we want to display */
    struct ScreenBuffer *showBuf = screenBuffers[newShow];

    /* Flip */
    if (!ChangeScreenBuffer(screen, showBuf)) {
        return;
    }

    /* After flip: this buffer becomes displayed */
    sbIndex = newShow;

    /* NEW back buffer = opposite of the displayed one */
    drawRP.BitMap = screenBuffers[1 - sbIndex]->sb_BitMap;

    /* Only NOW we wait until the back buffer is safe for drawing */
    if (dbufPrimed) {
        WaitSafe(screenBuffers[1 - sbIndex]);
    } else {
        dbufPrimed = TRUE;
    }
}



/* ---------------- Modal quit requester ---------------- */
#define QUIT_REQ_W 232
#define QUIT_REQ_H 80
#define RAWKEY_Y 0x15
#define RAWKEY_N 0x36
#define RAWKEY_ESC 0x45

static void FindRequesterPens(struct Screen *scr, UWORD *outBg, UWORD *outText) {
    UWORD i;
    ULONG bestDark = 0xFFFFFFFFUL;
    LONG bestText = -2147483647L;
    UWORD bg = 0;
    UWORD requesterTextPen = 1;
    UWORD colors = 1;

    if (!scr || !scr->ViewPort.ColorMap) {
        *outBg = bg;
        *outText = requesterTextPen;
        return;
    }

    colors = (UWORD)(1U << scr->RastPort.BitMap->Depth);
    if (colors > 32) colors = 32;

    for (i = 0; i < colors; i++) {
        UWORD rgb = GetRGB4(scr->ViewPort.ColorMap, i);
        LONG r = (LONG)((rgb >> 8) & 0x0F);
        LONG g = (LONG)((rgb >> 4) & 0x0F);
        LONG b = (LONG)(rgb & 0x0F);
        ULONG lum = (ULONG)(30 * r + 59 * g + 11 * b);
        LONG chroma = r > g ? r - g : g - r;
        LONG d = r > b ? r - b : b - r;
        LONG textScore;

        if (d > chroma) chroma = d;
        d = g > b ? g - b : b - g;
        if (d > chroma) chroma = d;

        if (lum < bestDark) {
            bestDark = lum;
            bg = i;
        }

        /* Prefer a bright, reasonably neutral existing palette entry. */
        textScore = (LONG)(lum * 4UL) - (chroma * 75L);
        if (textScore > bestText) {
            bestText = textScore;
            requesterTextPen = i;
        }
    }

    *outBg = bg;
    *outText = requesterTextPen;
}

static void DrawQuitRequester(struct RastPort *rp, struct TextFont *font, WORD reqX, WORD reqY,
                              UWORD bgPen, UWORD requesterTextPen, BOOL yesSelected) {
    static const char title[] = "QUIT TO WORKBENCH?";
    static const char yes[] = "YES";
    static const char no[] = "NO";
    WORD titleX;
    WORD yesX = (WORD)(reqX + 68);
    WORD noX = (WORD)(reqX + 150);
    WORD baseY = (WORD)(reqY + 49);
    WORD underlineY = (WORD)(reqY + 52);

    if (!rp) return;
    if (font) SetFont(rp, font);

    SetAPen(rp, bgPen);
    RectFill(rp, reqX, reqY, reqX + QUIT_REQ_W - 1,
             reqY + QUIT_REQ_H - 1);

    SetAPen(rp, requesterTextPen);
    Move(rp, reqX, reqY);
    Draw(rp, reqX + QUIT_REQ_W - 1, reqY);
    Draw(rp, reqX + QUIT_REQ_W - 1, reqY + QUIT_REQ_H - 1);
    Draw(rp, reqX, reqY + QUIT_REQ_H - 1);
    Draw(rp, reqX, reqY);

    titleX = (WORD)(reqX + (QUIT_REQ_W - TextLength(rp, (STRPTR)title, sizeof(title) - 1)) / 2);
    Move(rp, titleX, reqY + 25);
    Text(rp, (STRPTR)title, sizeof(title) - 1);
    Move(rp, yesX, baseY);
    Text(rp, (STRPTR)yes, sizeof(yes) - 1);
    Move(rp, noX, baseY);
    Text(rp, (STRPTR)no, sizeof(no) - 1);

    if (yesSelected) {
        Move(rp, yesX, underlineY);
        Draw(rp, (WORD)(yesX + TextLength(rp, (STRPTR)yes, sizeof(yes) - 1) - 1), underlineY);
    } else {
        Move(rp, noX, underlineY);
        Draw(rp, (WORD)(noX + TextLength(rp, (STRPTR)no, sizeof(no) - 1) - 1), underlineY);
    }
}

BOOL Gfx_ShowQuitRequester(BOOL useDBuf) {
    struct Screen *scr = Gfx_GetScreen();
    struct RastPort *rp;
    struct TextAttr ta = {"topaz.font", 8, FS_NORMAL, FPF_ROMFONT};
    struct TextFont *font = NULL;
    struct TextFont *oldFont = NULL;
    struct BitMap saved;
    BOOL savedReady = FALSE;
    BOOL yesSelected = FALSE;
    BOOL prevLeft = FALSE;
    BOOL prevRight = FALSE;
    struct RastPort modalRP;
    BOOL usingModalRP = FALSE;
    UBYTE oldDrawMode = JAM1;
    UBYTE oldFgPen = 1;
    UBYTE oldBgPen = 0;
    UWORD bgPen, requesterTextPen;
    WORD reqX, reqY;
    UWORD p;

    if (!scr || !scr->RastPort.BitMap) return FALSE;

    /* The requester is modal, so the range loop cannot render or swap while
     * this function is active.  With double buffering enabled, draw directly
     * into the buffer that is CURRENTLY DISPLAYED.  Do not disable/re-enable
     * ScreenBuffer double buffering here: doing so from the middle of the
     * range loop can wait for a SafeMessage that will never arrive. */
    if (useDBuf && Gfx_IsDoubleBufferingEnabled() && screenBuffers[sbIndex] &&
        screenBuffers[sbIndex]->sb_BitMap) {
        modalRP = scr->RastPort;
        modalRP.BitMap = screenBuffers[sbIndex]->sb_BitMap;
        rp = &modalRP;
        usingModalRP = TRUE;
    } else {
        rp = &scr->RastPort;
    }

    if (!rp || !rp->BitMap) return FALSE;

    /* Center the requester on the active screen.  This keeps the same modal
     * UI usable on both the 320px low-res pages and the 640px hi-res logo. */
    reqX = (WORD)((scr->Width - QUIT_REQ_W) / 2);
    reqY = (WORD)((scr->Height - QUIT_REQ_H) / 2);
    if (reqX < 0) reqX = 0;
    if (reqY < 0) reqY = 0;

    oldFont = rp->Font;
    oldDrawMode = rp->DrawMode;
    oldFgPen = rp->FgPen;
    oldBgPen = rp->BgPen;

    memset(&saved, 0, sizeof(saved));
    InitBitMap(&saved, rp->BitMap->Depth, QUIT_REQ_W, QUIT_REQ_H);
    savedReady = TRUE;
    for (p = 0; p < saved.Depth; p++) {
        saved.Planes[p] = AllocRaster(QUIT_REQ_W, QUIT_REQ_H);
        if (!saved.Planes[p]) {
            savedReady = FALSE;
            break;
        }
    }
    if (savedReady) {
        WaitBlit();
        BltBitMap(rp->BitMap, reqX, reqY, &saved, 0, 0, QUIT_REQ_W,
                  QUIT_REQ_H, 0xC0, 0xFF, NULL);
        WaitBlit();
    }

    font = OpenFont(&ta);
    FindRequesterPens(scr, &bgPen, &requesterTextPen);
    Input_ResetState();

    /* Some screens leave the RastPort in a drawing mode intended for their
     * own effects.  Repeated requester drawing in such a mode can toggle text
     * and border pixels.  The requester always uses normal JAM1 drawing. */
    SetDrMd(rp, JAM1);
    DrawQuitRequester(rp, font, reqX, reqY, bgPen, requesterTextPen, yesSelected);
    WaitBlit();

    for (;;) {
        BOOL left, right;
        BOOL selectionChanged = FALSE;

        Input_PollWindow(Gfx_GetWindow());

        if (Input_KeyPressed(RAWKEY_Y)) {
            yesSelected = TRUE;
            goto confirmed;
        }
        if (Input_KeyPressed(RAWKEY_N)) {
            goto cancelled;
        }

        /* Esc belongs to the separate firing-range exit requester.  While
         * QUIT TO WORKBENCH is modal it must not cancel or open another
         * requester, so consume and ignore it here. */
        (void)Input_KeyPressed(RAWKEY_ESC);

        left = Input_Left();
        right = Input_Right();
        if (left && !prevLeft && !yesSelected) {
            yesSelected = TRUE;
            selectionChanged = TRUE;
        }
        if (right && !prevRight && yesSelected) {
            yesSelected = FALSE;
            selectionChanged = TRUE;
        }
        prevLeft = left;
        prevRight = right;

        if (selectionChanged) {
            DrawQuitRequester(rp, font, reqX, reqY, bgPen, requesterTextPen, yesSelected);
            WaitBlit();
        }

        if (Input_FirePressed()) {
            if (yesSelected) goto confirmed;
            goto cancelled;
        }

        WaitTOF();
    }

confirmed:
    if (oldFont) SetFont(rp, oldFont);
    SetDrMd(rp, oldDrawMode);
    SetAPen(rp, oldFgPen);
    SetBPen(rp, oldBgPen);
    if (font) CloseFont(font);
    Input_ResetState();
    for (p = 0; p < saved.Depth; p++) {
        if (saved.Planes[p]) FreeRaster(saved.Planes[p], QUIT_REQ_W, QUIT_REQ_H);
    }
    return TRUE;

cancelled:
    if (savedReady) {
        WaitBlit();
        BltBitMap(&saved, 0, 0, rp->BitMap, reqX, reqY, QUIT_REQ_W,
                  QUIT_REQ_H, 0xC0, 0xFF, NULL);
        WaitBlit();
    }

    if (oldFont) SetFont(rp, oldFont);
    SetDrMd(rp, oldDrawMode);
    SetAPen(rp, oldFgPen);
    SetBPen(rp, oldBgPen);
    if (font) CloseFont(font);
    Input_ResetState();
    for (p = 0; p < saved.Depth; p++) {
        if (saved.Planes[p]) FreeRaster(saved.Planes[p], QUIT_REQ_W, QUIT_REQ_H);
    }

    (void)usingModalRP;
    return FALSE;
}


/* ---------------- Modal firing-range exit requester ---------------- */
#define RANGE_EXIT_REQ_W 304
#define RANGE_EXIT_REQ_H 96

static void DrawRangeExitRequester(struct RastPort *rp, struct TextFont *font,
                                   WORD reqX, WORD reqY, UWORD bgPen,
                                   UWORD requesterTextPen, BOOL yesSelected) {
    static const char line1[] = "ARE YOU SURE YOU WANT TO EXIT";
    static const char line2[] = "THE FIRING RANGE?";
    static const char line3[] = "YOUR SCORE WILL BE DISCARDED.";
    static const char yes[] = "YES";
    static const char no[] = "NO";
    WORD x;
    WORD yesX = (WORD)(reqX + 92);
    WORD noX = (WORD)(reqX + 204);
    WORD baseY = (WORD)(reqY + 79);
    WORD underlineY = (WORD)(reqY + 82);

    if (!rp) return;
    if (font) SetFont(rp, font);

    SetAPen(rp, bgPen);
    RectFill(rp, reqX, reqY, reqX + RANGE_EXIT_REQ_W - 1,
             reqY + RANGE_EXIT_REQ_H - 1);

    SetAPen(rp, requesterTextPen);
    Move(rp, reqX, reqY);
    Draw(rp, reqX + RANGE_EXIT_REQ_W - 1, reqY);
    Draw(rp, reqX + RANGE_EXIT_REQ_W - 1, reqY + RANGE_EXIT_REQ_H - 1);
    Draw(rp, reqX, reqY + RANGE_EXIT_REQ_H - 1);
    Draw(rp, reqX, reqY);

    x = (WORD)(reqX + (RANGE_EXIT_REQ_W - TextLength(rp, (STRPTR)line1, sizeof(line1) - 1)) / 2);
    Move(rp, x, reqY + 18);
    Text(rp, (STRPTR)line1, sizeof(line1) - 1);
    x = (WORD)(reqX + (RANGE_EXIT_REQ_W - TextLength(rp, (STRPTR)line2, sizeof(line2) - 1)) / 2);
    Move(rp, x, reqY + 31);
    Text(rp, (STRPTR)line2, sizeof(line2) - 1);
    x = (WORD)(reqX + (RANGE_EXIT_REQ_W - TextLength(rp, (STRPTR)line3, sizeof(line3) - 1)) / 2);
    Move(rp, x, reqY + 52);
    Text(rp, (STRPTR)line3, sizeof(line3) - 1);

    Move(rp, yesX, baseY);
    Text(rp, (STRPTR)yes, sizeof(yes) - 1);
    Move(rp, noX, baseY);
    Text(rp, (STRPTR)no, sizeof(no) - 1);

    if (yesSelected) {
        Move(rp, yesX, underlineY);
        Draw(rp, (WORD)(yesX + TextLength(rp, (STRPTR)yes, sizeof(yes) - 1) - 1), underlineY);
    } else {
        Move(rp, noX, underlineY);
        Draw(rp, (WORD)(noX + TextLength(rp, (STRPTR)no, sizeof(no) - 1) - 1), underlineY);
    }
}

BOOL Gfx_ShowRangeExitRequester(BOOL useDBuf) {
    struct Screen *scr = Gfx_GetScreen();
    struct RastPort *rp;
    struct TextAttr ta = {"topaz.font", 8, FS_NORMAL, FPF_ROMFONT};
    struct TextFont *font = NULL;
    struct TextFont *oldFont = NULL;
    struct BitMap saved;
    BOOL savedReady = FALSE;
    BOOL yesSelected = FALSE;
    BOOL prevLeft = FALSE;
    BOOL prevRight = FALSE;
    struct RastPort modalRP;
    UBYTE oldDrawMode = JAM1;
    UBYTE oldFgPen = 1;
    UBYTE oldBgPen = 0;
    UWORD bgPen, requesterTextPen;
    WORD reqX, reqY;
    UWORD p;

    if (!scr || !scr->RastPort.BitMap) return FALSE;

    if (useDBuf && Gfx_IsDoubleBufferingEnabled() && screenBuffers[sbIndex] &&
        screenBuffers[sbIndex]->sb_BitMap) {
        modalRP = scr->RastPort;
        modalRP.BitMap = screenBuffers[sbIndex]->sb_BitMap;
        rp = &modalRP;
    } else {
        rp = &scr->RastPort;
    }
    if (!rp || !rp->BitMap) return FALSE;

    reqX = (WORD)((scr->Width - RANGE_EXIT_REQ_W) / 2);
    reqY = (WORD)((scr->Height - RANGE_EXIT_REQ_H) / 2);
    if (reqX < 0) reqX = 0;
    if (reqY < 0) reqY = 0;

    oldFont = rp->Font;
    oldDrawMode = rp->DrawMode;
    oldFgPen = rp->FgPen;
    oldBgPen = rp->BgPen;

    memset(&saved, 0, sizeof(saved));
    InitBitMap(&saved, rp->BitMap->Depth, RANGE_EXIT_REQ_W, RANGE_EXIT_REQ_H);
    savedReady = TRUE;
    for (p = 0; p < saved.Depth; p++) {
        saved.Planes[p] = AllocRaster(RANGE_EXIT_REQ_W, RANGE_EXIT_REQ_H);
        if (!saved.Planes[p]) {
            savedReady = FALSE;
            break;
        }
    }
    if (savedReady) {
        WaitBlit();
        BltBitMap(rp->BitMap, reqX, reqY, &saved, 0, 0, RANGE_EXIT_REQ_W,
                  RANGE_EXIT_REQ_H, 0xC0, 0xFF, NULL);
        WaitBlit();
    }

    font = OpenFont(&ta);
    FindRequesterPens(scr, &bgPen, &requesterTextPen);
    Input_ResetState();
    SetDrMd(rp, JAM1);
    DrawRangeExitRequester(rp, font, reqX, reqY, bgPen, requesterTextPen, yesSelected);
    WaitBlit();

    for (;;) {
        BOOL left, right;
        BOOL selectionChanged = FALSE;

        Input_PollWindow(Gfx_GetWindow());

        if (Input_KeyPressed(RAWKEY_Y)) {
            yesSelected = TRUE;
            goto range_confirmed;
        }
        if (Input_KeyPressed(RAWKEY_N)) {
            goto range_cancelled;
        }

        /* Amiga+Q is deliberately consumed while this requester owns input.
         * It must never open the Workbench requester on top of this one. */
        (void)Input_QuitPressed();
        /* Repeated Esc while the Esc requester is already open is inert. */
        (void)Input_KeyPressed(RAWKEY_ESC);

        left = Input_Left();
        right = Input_Right();
        if (left && !prevLeft && !yesSelected) {
            yesSelected = TRUE;
            selectionChanged = TRUE;
        }
        if (right && !prevRight && yesSelected) {
            yesSelected = FALSE;
            selectionChanged = TRUE;
        }
        prevLeft = left;
        prevRight = right;

        if (selectionChanged) {
            DrawRangeExitRequester(rp, font, reqX, reqY, bgPen, requesterTextPen, yesSelected);
            WaitBlit();
        }

        if (Input_FirePressed()) {
            if (yesSelected) goto range_confirmed;
            goto range_cancelled;
        }
        WaitTOF();
    }

range_confirmed:
    if (oldFont) SetFont(rp, oldFont);
    SetDrMd(rp, oldDrawMode);
    SetAPen(rp, oldFgPen);
    SetBPen(rp, oldBgPen);
    if (font) CloseFont(font);
    Input_ResetState();
    for (p = 0; p < saved.Depth; p++) {
        if (saved.Planes[p]) FreeRaster(saved.Planes[p], RANGE_EXIT_REQ_W, RANGE_EXIT_REQ_H);
    }
    return TRUE;

range_cancelled:
    if (savedReady) {
        WaitBlit();
        BltBitMap(&saved, 0, 0, rp->BitMap, reqX, reqY, RANGE_EXIT_REQ_W,
                  RANGE_EXIT_REQ_H, 0xC0, 0xFF, NULL);
        WaitBlit();
    }
    if (oldFont) SetFont(rp, oldFont);
    SetDrMd(rp, oldDrawMode);
    SetAPen(rp, oldFgPen);
    SetBPen(rp, oldBgPen);
    if (font) CloseFont(font);
    Input_ResetState();
    for (p = 0; p < saved.Depth; p++) {
        if (saved.Planes[p]) FreeRaster(saved.Planes[p], RANGE_EXIT_REQ_W, RANGE_EXIT_REQ_H);
    }
    return FALSE;
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
                                   /* Keep normal gameplay/menu input RAWKEY-only.  IDCMP_VANILLAKEY is
                                    * enabled temporarily by the high-score name-entry screen.
                                    * Requesting both for the whole program can divert printable
                                    * keys (including W/A/S/D) away from RAWKEY, which means the
                                    * shared input layer cannot reliably see both key-down and
                                    * key-up events for held movement. */
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
    SettleDisplay(2);

    /* Najpierw double buffering */
    if (dbufEnabled) {
        Gfx_DisableDoubleBuffering();
        dbufEnabled = FALSE;
        WaitBlit();
        SettleDisplay(2);
    }

    ShowPointer(window);

    if (window) {
        CloseWindow(window);
        window = NULL;
        WaitBlit();
        SettleDisplay(2);
    }

    FreeBlankPointer();

    if (screen) {
        CloseScreen(screen);
        screen = NULL;
        WaitBlit();
        SettleDisplay(3);
    }
}

void Gfx_ToggleNightVision(void) {
    UWORD i;
    UWORD transformed[32];

    displayMode = (displayMode == DISPLAY_NIGHT_VISION)
        ? DISPLAY_STANDARD : DISPLAY_NIGHT_VISION;

    if (!screen || !logicalPaletteColors) {
        return;
    }

    if (displayMode == DISPLAY_STANDARD) {
        LoadRGB4(&screen->ViewPort, logicalPalette, logicalPaletteColors);
    } else {
        for (i = 0; i < logicalPaletteColors; i++) {
            transformed[i] = NightVisionRGB4(logicalPalette[i]);
        }
        LoadRGB4(&screen->ViewPort, transformed, logicalPaletteColors);
    }

    WaitTOF();
}

void Gfx_ToggleRedRoom(void) {
    UWORD i;
    UWORD transformed[32];

    displayMode = (displayMode == DISPLAY_RED_ROOM)
        ? DISPLAY_STANDARD : DISPLAY_RED_ROOM;

    if (!screen || !logicalPaletteColors) {
        return;
    }

    if (displayMode == DISPLAY_STANDARD) {
        LoadRGB4(&screen->ViewPort, logicalPalette, logicalPaletteColors);
    } else {
        for (i = 0; i < logicalPaletteColors; i++) {
            transformed[i] = RedRoomRGB4(logicalPalette[i]);
        }
        LoadRGB4(&screen->ViewPort, transformed, logicalPaletteColors);
    }

    WaitTOF();
}

void Gfx_ToggleAmber(void) {
    UWORD i;
    UWORD transformed[32];

    displayMode = (displayMode == DISPLAY_AMBER)
        ? DISPLAY_STANDARD : DISPLAY_AMBER;

    if (!screen || !logicalPaletteColors) {
        return;
    }

    if (displayMode == DISPLAY_STANDARD) {
        LoadRGB4(&screen->ViewPort, logicalPalette, logicalPaletteColors);
    } else {
        for (i = 0; i < logicalPaletteColors; i++) {
            transformed[i] = AmberRGB4(logicalPalette[i]);
        }
        LoadRGB4(&screen->ViewPort, transformed, logicalPaletteColors);
    }

    WaitTOF();
}

BOOL Gfx_IsNightVisionEnabled(void) {
    return (displayMode == DISPLAY_NIGHT_VISION) ? TRUE : FALSE;
}

BOOL Gfx_IsRedRoomEnabled(void) {
    return (displayMode == DISPLAY_RED_ROOM) ? TRUE : FALSE;
}

BOOL Gfx_IsAmberEnabled(void) {
    return (displayMode == DISPLAY_AMBER) ? TRUE : FALSE;
}

void Gfx_FadeOutCurrentScreenToBlack(const UWORD *currentPal, UWORD colors) {
    if (!screen || !currentPal || colors == 0) {
        return;
    }

    FadeOutToBlack(screen, currentPal, colors);
    WaitBlit();
    SettleDisplay(1);
}

void Gfx_FadeInCurrentScreenFromBlack(const UWORD *targetPal, UWORD colors) {
    if (!screen || !targetPal || colors == 0) {
        return;
    }

    ScreenToFront(screen);
    RemakeDisplay();
    SettleDisplay(2);
    FadeInFromBlack(screen, targetPal, colors);
    RememberLogicalPalette(targetPal, colors);
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
    RememberLogicalPalette(targetPal, colors);
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
    RememberLogicalPalette(toPal, toColors);
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