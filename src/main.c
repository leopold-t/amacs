#include <exec/types.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "bob.h"
#include "gfx.h"
#include "input.h"
#include "levelManager.h"
#include "rangeHandler.h"
#include "soundHandler.h"
#include "targetScoring.h"
#include "targetsHandler.h"

/* If your input.h doesn't expose directions yet, keep these externs here.
   They must exist in input.c (or you'll get linker errors). */
extern BOOL Input_Left(void);
extern BOOL Input_Right(void);
extern BOOL Input_Up(void);
extern BOOL Input_Down(void);

/* HiRes screen (LOGO only) */
#define HI_WIDTH 640
#define HI_HEIGHT 256
#define HI_DEPTH 4

/* LoRes screens (TITLE SCREEN, TRAINING_INFO, FUNDAMENTALS, TARGET_RANGES, RANGE) */
#define TITLE_WIDTH 320
#define TITLE_HEIGHT 256
#define TITLE_DEPTH 5
#define TITLE_DISPLAY_ID LORES_KEY

#define LO_WIDTH 320
#define LO_HEIGHT 256
#define LO_DEPTH 5

/* Black safety screen */
#define BLK_WIDTH 320
#define BLK_HEIGHT 256
#define BLK_DEPTH 2

#define LOGO_FILE "gfx/Logo.raw"
#define TITLE_FILE "gfx/Title.raw"
#define TRAINING_INFO_FILE "gfx/TrainingInfo.raw"
#define FUNDAMENTALS_FILE "gfx/Fundamentals.raw"
#define TARGET_RANGES_FILE "gfx/TargetRanges.raw"
#define PERFORMANCE_FILE "gfx/Performance.raw"
#define RANGE_FILE "gfx/OahuRange.raw"
#define SUMMARY_FILE "gfx/Summary.raw"

static const UWORD SummaryPaletteRGB4[32] = {
    0x0000, 0x0005, 0x000C, 0x010D, 0x0119, 0x010B, 0x0113, 0x0C00, 0x0555, 0x0557, 0x055A,
    0x0D61, 0x065D, 0x0DC1, 0x01A0, 0x088E, 0x0DC1, 0x0999, 0x099C, 0x0A9E, 0x0CCC, 0x0CCE,
    0x01F0, 0x099C, 0x0222, 0x03C2, 0x088E, 0x065D, 0x0005, 0x0119, 0x010B, 0x0EEE};

#define SUMMARY_TEXT_PEN 25
#define SUMMARY_SHADOW_PEN 31
#define SUMMARY_TITLE_Y 96
#define SUMMARY_SCORE_Y 120
#define SUMMARY_ACCURACY_Y 136
#define SUMMARY_HOLD_WAIT 0
#define TARGET050_RAW "gfx/Target050.raw"
#define TARGET050_MASK "gfx/Target050.mask"
#define TARGET100_RAW "gfx/Target100.raw"
#define TARGET100_MASK "gfx/Target100.mask"
#define TARGET150_RAW "gfx/Target150.raw"
#define TARGET150_MASK "gfx/Target150.mask"
#define TARGET200_RAW "gfx/Target200.raw"
#define TARGET200_MASK "gfx/Target200.mask"
#define TARGET250_RAW "gfx/Target250.raw"
#define TARGET250_MASK "gfx/Target250.mask"
#define TARGET300_RAW "gfx/Target300.raw"
#define TARGET300_MASK "gfx/Target300.mask"

#define FRONTSIGHT_RAW "gfx/FrontSight.raw"
#define FRONTSIGHT_MASK "gfx/FrontSight.mask"

/* Front sight dimensions (planar RAW + 1-bit MASK)
 * Updated: trimmed sprite to save memory.
 */
#define FRONTSIGHT_W 83
#define FRONTSIGHT_H 79

/* Timing (PAL: 50 ticks/sec). */
#define TICKS_PER_SEC 50
#define LOGO_SECONDS 3
#define TITLE_SECONDS 12
#define INFO_SECONDS 6
#define TARGETRANGES_DELAY_TICKS 25
#define TARGETRANGES_RISE_TICKS 15

typedef enum { WAIT_TIMEOUT = 0, WAIT_ADVANCE, WAIT_ESC } WaitResult;

static void DrainWindowMessages(void);
static void PollAdvanceAndEsc(BOOL *outAdvance, BOOL *outEsc);

static const struct TextAttr gSummaryFontAttr = {"topaz.font", 8, FS_NORMAL, FPF_ROMFONT};

static UWORD AppendUnsignedMain(char *buf, UWORD pos, UWORD value) {
    char digits[5];
    UWORD count = 0;

    if (!buf) {
        return pos;
    }

    if (value == 0) {
        buf[pos++] = '0';
        buf[pos] = '\0';
        return pos;
    }

    while (value > 0 && count < 5) {
        digits[count++] = (char)('0' + (value % 10));
        value = (UWORD)(value / 10);
    }

    while (count > 0) {
        buf[pos++] = digits[--count];
    }

    buf[pos] = '\0';
    return pos;
}

static void BuildSummaryScoreLine(char *buf, UWORD score) {
    static const char prefix[] = "TOTAL SCORE: ";
    UWORD i = 0;
    UWORD pos = 0;

    if (!buf) {
        return;
    }

    while (prefix[i] != '\0') {
        buf[pos++] = prefix[i++];
    }

    buf[pos] = '\0';
    AppendUnsignedMain(buf, pos, score);
}

static void BuildSummaryAccuracyLine(char *buf, UWORD accuracy) {
    static const char prefix[] = "ACCURACY: ";
    UWORD i = 0;
    UWORD pos = 0;

    if (!buf) {
        return;
    }

    while (prefix[i] != '\0') {
        buf[pos++] = prefix[i++];
    }

    buf[pos] = '\0';
    pos = AppendUnsignedMain(buf, pos, accuracy);
    buf[pos++] = '%';
    buf[pos] = '\0';
}

static void DrawTextWithShadowExMain(struct RastPort *rp, struct TextFont *font, WORD x, WORD y,
                                     UWORD pen, UWORD shadowPenValue, const char *text, UWORD len) {
    if (!rp || !text || len == 0) {
        return;
    }

    if (font) {
        SetFont(rp, font);
    }

    SetDrMd(rp, JAM1);

    SetAPen(rp, shadowPenValue);
    Move(rp, x + 1, y + 1 + rp->TxBaseline);
    Text(rp, (STRPTR)text, len);

    SetAPen(rp, pen);
    Move(rp, x, y + rp->TxBaseline);
    Text(rp, (STRPTR)text, len);
}

static void DrawCenteredTextWithShadowMain(struct RastPort *rp, struct TextFont *font, WORD y,
                                           UWORD pen, UWORD shadowPenValue, const char *text) {
    UWORD len;
    WORD width;
    WORD x;

    if (!rp || !text) {
        return;
    }

    len = (UWORD)strlen(text);

    if (font) {
        SetFont(rp, font);
    }

    width = TextLength(rp, (STRPTR)text, len);
    x = (WORD)((LO_WIDTH - width) / 2);
    DrawTextWithShadowExMain(rp, font, x, y, pen, shadowPenValue, text, len);
}

static WaitResult WaitForAdvanceOnly(void) {
    DrainWindowMessages();

    for (;;) {
        BOOL adv = FALSE, esc = FALSE;
        PollAdvanceAndEsc(&adv, &esc);

        if (esc) {
            return WAIT_ESC;
        }

        if (adv) {
            return WAIT_ADVANCE;
        }

        Sound_Update();
        WaitTOF();
    }
}

static BOOL ShowSummaryScreen(const RangeSummaryData *summary) {
    struct RastPort *rp;
    struct TextFont *font = NULL;
    char line[32];
    UWORD score;
    UWORD acc;

    if (!Gfx_CrossFadeToImage(SUMMARY_FILE, rangePalette, 32, SummaryPaletteRGB4, 32)) {
        return FALSE;
    }

    rp = Gfx_GetDrawRastPort();

    if (!rp)
        return FALSE;

    font = OpenFont(&gSummaryFontAttr);

    score = summary ? summary->score : 0;
    acc = summary ? summary->accuracy : 0;

    BuildSummaryScoreLine(line, score);
    DrawCenteredTextWithShadowMain(rp, font, SUMMARY_SCORE_Y, 31, 24, line);

    BuildSummaryAccuracyLine(line, acc);
    DrawCenteredTextWithShadowMain(rp, font, SUMMARY_ACCURACY_Y, 31, 24, line);

    if (font)
        CloseFont(font);

    switch (WaitForAdvanceOnly()) {
        case WAIT_ESC:
            return FALSE;
        case WAIT_ADVANCE:
        default:
            break;
    }

    if (!Gfx_CrossFadeToImage(TITLE_FILE, SummaryPaletteRGB4, 32, titlePalette, 32)) {
        return FALSE;
    }

    return TRUE;
}

/* ---------- Helpers ---------- */

static void DrainWindowMessages(void) {
    struct Window *win = Gfx_GetWindow();
    struct IntuiMessage *msg;

    if (!win || !win->UserPort) {
        return;
    }

    while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {
        ReplyMsg((struct Message *)msg);
    }
}

/*
 * IMPORTANT:
 * Do not poll IDCMP in two separate functions (ESC + ADVANCE), because the first reader
 * drains messages and the second sees nothing.
 *
 * This function consumes IDCMP once per call and returns both flags.
 */
static void PollAdvanceAndEsc(BOOL *outAdvance, BOOL *outEsc) {
    struct Window *win = Gfx_GetWindow();
    struct IntuiMessage *msg;

    *outAdvance = FALSE;
    *outEsc = FALSE;

    if (win && win->UserPort) {
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {

            if (msg->Class == IDCMP_RAWKEY && msg->Code == 0x45) {
                *outEsc = TRUE; /* ESC */
            }

            if (msg->Class == IDCMP_MOUSEBUTTONS && msg->Code == SELECTDOWN) {
                *outAdvance = TRUE; /* LMB */
            }

            ReplyMsg((struct Message *)msg);
        }
    }

    /* Joystick fire (port handling is inside input.c) */
    if (IsJoystickFirePressed()) {
        *outAdvance = TRUE;
    }
}

/*
 * Wait up to `seconds` for:
 * - ESC => WAIT_ESC
 * - Fire or LMB => WAIT_ADVANCE
 * - timeout => WAIT_TIMEOUT
 *
 * Debounces Fire to prevent double-advancing.
 */
static WaitResult WaitForAdvanceOrTimeout(int seconds) {
    LONG ticks = (LONG)seconds * TICKS_PER_SEC;

    DrainWindowMessages();

    while (ticks-- > 0) {
        BOOL adv = FALSE, esc = FALSE;

        PollAdvanceAndEsc(&adv, &esc);

        if (esc) {
            return WAIT_ESC;
        }

        if (adv) {
            /* Debounce Fire (mouse click is one-shot anyway). */
            WaitTOF();
            WaitTOF();
            while (IsJoystickFirePressed()) {
                WaitTOF();
            }

            DrainWindowMessages();
            return WAIT_ADVANCE;
        }

        Sound_Update();
        WaitTOF();
    }

    return WAIT_TIMEOUT;
}

static WaitResult WaitForAdvanceNoTimeout(void) {
    DrainWindowMessages();

    for (;;) {
        BOOL adv = FALSE, esc = FALSE;

        PollAdvanceAndEsc(&adv, &esc);

        if (esc) {
            return WAIT_ESC;
        }

        if (adv) {
            WaitTOF();
            WaitTOF();
            while (IsJoystickFirePressed()) {
                WaitTOF();
            }

            DrainWindowMessages();
            return WAIT_ADVANCE;
        }

        Sound_Update();
        WaitTOF();
    }
}

/* Check if double buffering is safe on this system (graphics.library >= 39, Kick 3.0+) */
static BOOL UseDoubleBuffering(void) {
    struct Library *GfxBase = OpenLibrary("graphics.library", 0);
    if (!GfxBase)
        return FALSE;

    BOOL safe = (GfxBase->lib_Version >= 39);

    CloseLibrary(GfxBase);
    return safe;
}

typedef struct TargetRangesBob {
    AmacsBob bob;
    WORD x;
    WORD bottomY;
} TargetRangesBob;

static void DrawTargetRangesBobReveal(struct RastPort *rp, const TargetRangesBob *tb,
                                      UWORD revealTicks) {
    WORD risePx;
    WORD dstY;
    WORD visibleH;

    if (!rp || !tb || !tb->bob.mask || !tb->bob.bm.Planes[0]) {
        return;
    }

    if (revealTicks >= TARGETRANGES_RISE_TICKS) {
        risePx = tb->bob.height;
    } else {
        risePx = (WORD)((revealTicks * tb->bob.height) / TARGETRANGES_RISE_TICKS);
    }

    if (risePx <= 0) {
        return;
    }

    dstY = (WORD)((tb->bottomY + 1) - risePx);
    visibleH = risePx;

    if (dstY < 0) {
        visibleH = (WORD)(visibleH + dstY);
        dstY = 0;
    }

    if ((dstY + visibleH) > LO_HEIGHT) {
        visibleH = (WORD)(LO_HEIGHT - dstY);
    }

    if (visibleH <= 0) {
        return;
    }

    BltMaskBitMapRastPort((struct BitMap *)&tb->bob.bm, 0, 0, rp, tb->x, dstY, tb->bob.width,
                          visibleH, 0xE0, tb->bob.mask);
    WaitBlit();
}

static void FreeTargetRangesBobs(TargetRangesBob *targets, UWORD count) {
    UWORD i;

    if (!targets) {
        return;
    }

    for (i = 0; i < count; i++) {
        Bob_Free(&targets[i].bob);
    }
}

static BOOL LoadTargetRangesBobs(TargetRangesBob *targets) {
    if (!targets) {
        return FALSE;
    }

    targets[0].x = 14;
    targets[0].bottomY = 187;
    targets[1].x = 90;
    targets[1].bottomY = 170;
    targets[2].x = 148;
    targets[2].bottomY = 157;
    targets[3].x = 194;
    targets[3].bottomY = 147;
    targets[4].x = 239;
    targets[4].bottomY = 132;
    targets[5].x = 280;
    targets[5].bottomY = 118;

    if (!Bob_LoadRawAndMask(&targets[0].bob, TARGET050_RAW, TARGET050_MASK, T050_W, T050_H,
                            LO_DEPTH) ||
        !Bob_LoadRawAndMask(&targets[1].bob, TARGET100_RAW, TARGET100_MASK, T100_W, T100_H,
                            LO_DEPTH) ||
        !Bob_LoadRawAndMask(&targets[2].bob, TARGET150_RAW, TARGET150_MASK, T150_W, T150_H,
                            LO_DEPTH) ||
        !Bob_LoadRawAndMask(&targets[3].bob, TARGET200_RAW, TARGET200_MASK, T200_W, T200_H,
                            LO_DEPTH) ||
        !Bob_LoadRawAndMask(&targets[4].bob, TARGET250_RAW, TARGET250_MASK, T250_W, T250_H,
                            LO_DEPTH) ||
        !Bob_LoadRawAndMask(&targets[5].bob, TARGET300_RAW, TARGET300_MASK, T300_W, T300_H,
                            LO_DEPTH)) {
        FreeTargetRangesBobs(targets, 6);
        return FALSE;
    }

    return TRUE;
}

static WaitResult WaitForTargetRangesAdvance(void) {
    TargetRangesBob targets[6] = {0};
    BOOL loaded = FALSE;
    UWORD delayTicks = 0;
    UWORD revealTicks = 0;
    BOOL inputEnabled = FALSE;
    struct Screen *screen = Gfx_GetScreen();
    struct RastPort *rp;
    UWORD i;

    if (!screen) {
        return WAIT_ESC;
    }

    DrainWindowMessages();

    if (!LoadTargetRangesBobs(targets)) {
        return WAIT_ESC;
    }

    loaded = TRUE;
    rp = &screen->RastPort;

    for (;;) {
        BOOL adv = FALSE, esc = FALSE;

        if (!inputEnabled) {
            if (delayTicks < TARGETRANGES_DELAY_TICKS) {
                delayTicks++;
            } else {
                if (revealTicks < TARGETRANGES_RISE_TICKS) {
                    revealTicks++;

                    for (i = 0; i < 6; i++) {
                        DrawTargetRangesBobReveal(rp, &targets[i], revealTicks);
                    }
                }

                if (revealTicks >= TARGETRANGES_RISE_TICKS) {
                    inputEnabled = TRUE;
                }
            }
        }

        PollAdvanceAndEsc(&adv, &esc);

        if (esc) {
            if (loaded) {
                FreeTargetRangesBobs(targets, 6);
            }

            return WAIT_ESC;
        }

        if (inputEnabled && adv) {
            WaitTOF();
            WaitTOF();

            while (IsJoystickFirePressed()) {
                WaitTOF();
            }

            DrainWindowMessages();

            if (loaded) {
                FreeTargetRangesBobs(targets, 6);
            }

            return WAIT_ADVANCE;
        }

        Sound_Update();
        WaitTOF();
    }
}

/* ------------------------------------------------------------------ */
/*                            MAIN                                    */
/* ------------------------------------------------------------------ */

typedef enum { ATTR_TRAINING_INFO = 0, ATTR_FUNDAMENTALS } AttractScreen;

int main(void) {
    BOOL engaged = FALSE;
    UWORD currentLoPal[32];
    UWORD nextLoPal[32];

    if (!Input_Init()) {
        return RETURN_FAIL;
    }

    LevelManager_Init();

    if (!Gfx_OpenBlackScreen(BLK_WIDTH, BLK_HEIGHT, BLK_DEPTH)) {
        LevelManager_Shutdown();
        Input_Shutdown();
        return RETURN_FAIL;
    }

    /* ---------------- HiRes: LOGO ---------------- */
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

    {
        WaitResult r = WaitForAdvanceOrTimeout(LOGO_SECONDS);

        if (r == WAIT_ESC) {
            goto exit_ok;
        }
    }

    /* ---------------- Title: standard Low Res 32 colors ---------------- */
    if (!Gfx_SwitchHiResToLoResOnBlack(logoPalette, TITLE_WIDTH, TITLE_HEIGHT, TITLE_DEPTH)) {
        Gfx_CloseScreenAndWindow();
        Gfx_CloseBlackScreen();
        LevelManager_Shutdown();
        Input_Shutdown();
        return RETURN_FAIL;
    }

    if (!Gfx_ShowImageFadeInFromBlack(TITLE_FILE, titlePalette, 32)) {
        Gfx_CloseScreenAndWindow();
        Gfx_CloseBlackScreen();
        LevelManager_Shutdown();
        Input_Shutdown();
        return RETURN_FAIL;
    }

    for (int i = 0; i < 32; i++) {
        currentLoPal[i] = titlePalette[i];
    }

show_title:
    IS_NEW_GAME_SESSION = TRUE;
    engaged = FALSE;

    if (Sound_InitTitleMusic()) {
        Sound_PlayTitleMusic();
    }

    {
        WaitResult r = WaitForAdvanceOrTimeout(TITLE_SECONDS);

        if (r == WAIT_ESC) {
            Sound_StopTitleMusic(FALSE);
            goto exit_ok;
        }

        if (r == WAIT_ADVANCE) {
            engaged = TRUE;
        }
    }

    Sound_StopTitleMusic(TRUE);

    if (Sound_InitAmbientLoop()) {
        Sound_PlayAmbientLoop();
    }

    if (!Gfx_CrossFadeToImage(TRAINING_INFO_FILE, titlePalette, 32, trainingInfoPalette, 32)) {
        Gfx_CloseScreenAndWindow();
        Gfx_CloseBlackScreen();
        LevelManager_Shutdown();
        Input_Shutdown();
        return RETURN_FAIL;
    }

    for (int i = 0; i < 32; i++) {
        currentLoPal[i] = trainingInfoPalette[i];
    }

    AttractScreen attr = ATTR_TRAINING_INFO;

    /* ---------------- Attract loop: TRAINING_INFO -> FUNDAMENTALS ---------------- */
    for (;;) {
        WaitResult r = WaitForAdvanceOrTimeout(INFO_SECONDS);

        if (r == WAIT_ESC) {
            goto exit_ok;
        }

        if (r == WAIT_ADVANCE) {
            engaged = TRUE;
        }

        if (engaged) {
            /* Engaged path: ensure order FUNDAMENTALS -> TARGET_RANGES -> RANGE */
            if (attr == ATTR_TRAINING_INFO) {
                for (int i = 0; i < 32; i++) {
                    nextLoPal[i] = fundamentalsPalette[i];
                }

                if (!Gfx_CrossFadeToImage(FUNDAMENTALS_FILE, currentLoPal, 32, nextLoPal, 32)) {
                    goto fail;
                }

                for (int i = 0; i < 32; i++) {
                    currentLoPal[i] = nextLoPal[i];
                }

                attr = ATTR_FUNDAMENTALS;
            }

            if (attr == ATTR_FUNDAMENTALS) {
                for (int i = 0; i < 32; i++) {
                    nextLoPal[i] = targetRangesPalette[i];
                }

                if (!Gfx_CrossFadeToImage(TARGET_RANGES_FILE, currentLoPal, 32, nextLoPal, 32)) {
                    goto fail;
                }

                for (int i = 0; i < 32; i++) {
                    currentLoPal[i] = nextLoPal[i];
                }
                {
                    WaitResult rr = WaitForTargetRangesAdvance();

                    if (rr == WAIT_ESC) {
                        goto exit_ok;
                    }
                }

                for (int i = 0; i < 32; i++) {
                    nextLoPal[i] = performancePalette[i];
                }

                if (!Gfx_CrossFadeToImage(PERFORMANCE_FILE, currentLoPal, 32, nextLoPal, 32)) {
                    goto fail;
                }

                for (int i = 0; i < 32; i++) {
                    currentLoPal[i] = nextLoPal[i];
                }
                {
                    WaitResult rr = WaitForAdvanceNoTimeout();

                    if (rr == WAIT_ESC) {
                        goto exit_ok;
                    }
                }
            }

            /* Enter RANGE */
            Sound_StopAmbientLoop(TRUE);

            if (!Gfx_CrossFadeToImage(RANGE_FILE, currentLoPal, 32, rangePalette, 32)) {
                goto fail;
            }

            BOOL useDBuf = UseDoubleBuffering();
            RangeSummaryData summaryData;
            memset(&summaryData, 0, sizeof(summaryData));

            if (useDBuf) {
                if (!Gfx_EnableDoubleBuffering()) {
                    goto fail;
                }
            }

            if (LevelManager_RunCurrent(useDBuf, &summaryData)) {
                if (useDBuf) {
                    Gfx_DisableDoubleBuffering();
                }

                if (!ShowSummaryScreen(&summaryData)) {
                    goto exit_ok;
                }

                for (int i = 0; i < 32; i++) {
                    currentLoPal[i] = titlePalette[i];
                }

                goto show_title;
            }

            goto exit_ok;
        }

        /* Not engaged: carousel advance */
        if (attr == ATTR_TRAINING_INFO) {
            attr = ATTR_FUNDAMENTALS;

            for (int i = 0; i < 32; i++) {
                nextLoPal[i] = fundamentalsPalette[i];
            }

            if (!Gfx_CrossFadeToImage(FUNDAMENTALS_FILE, currentLoPal, 32, nextLoPal, 32)) {
                goto fail;
            }

        } else {
            attr = ATTR_TRAINING_INFO;

            for (int i = 0; i < 32; i++) {
                nextLoPal[i] = trainingInfoPalette[i];
            }

            if (!Gfx_CrossFadeToImage(TRAINING_INFO_FILE, currentLoPal, 32, nextLoPal, 32)) {
                goto fail;
            }
        }

        for (int i = 0; i < 32; i++) {
            currentLoPal[i] = nextLoPal[i];
        }
    }

fail:
    Sound_StopAmbientLoop(FALSE);
    Sound_ShutdownAmbientLoop();
    Sound_StopTitleMusic(FALSE);
    Sound_ShutdownTitleMusic();
    Gfx_CloseScreenAndWindow();
    Gfx_CloseBlackScreen();
    LevelManager_Shutdown();
    Input_Shutdown();
    return RETURN_FAIL;

exit_ok:
    Sound_StopAmbientLoop(FALSE);
    Sound_ShutdownAmbientLoop();
    Sound_StopTitleMusic(FALSE);
    Sound_ShutdownTitleMusic();
    Gfx_CloseScreenAndWindow();
    Gfx_CloseBlackScreen();
    LevelManager_Shutdown();
    Input_Shutdown();
    return RETURN_OK;
}