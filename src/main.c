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
#include "imageHandler.h"
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
    0x0000, 0x0005, 0x000C, 0x010D, 0x0119, 0x010B, 0x0113, 0x0C00, 0x099C, 0x0557, 0x055A,
    0x0D61, 0x065D, 0x0DC1, 0x01A0, 0x088E, 0x0DC1, 0x0999, 0x099C, 0x0A9E, 0x0CCC, 0x0CCE,
    0x01F0, 0x0666, 0x0222, 0x03C2, 0x088E, 0x065D, 0x0005, 0x0119, 0x010B, 0x0EEE};

#define SUMMARY_TEXT_PEN 25
#define SUMMARY_SHADOW_PEN 24
#define SUMMARY_TITLE_Y 96
#define SUMMARY_SCORE_Y 16
#define SUMMARY_ACCURACY_Y 32
#define SUMMARY_HOLD_WAIT 0
#define SUMMARY_SCORING_ETYPE_RAW "gfx/ScoringEType.raw"
#define SUMMARY_SCORING_ETYPE_MASK "gfx/ScoringEType.mask"
#define SUMMARY_SCORING_ETYPE_W 38
#define SUMMARY_SCORING_ETYPE_H 68
#define SUMMARY_SCORING_ETYPE_X 141
#define SUMMARY_SCORING_ETYPE_BOTTOM_Y 147
#define SUMMARY_SCORING_ETYPE_Y (SUMMARY_SCORING_ETYPE_BOTTOM_Y - SUMMARY_SCORING_ETYPE_H)
#define TARGET050_RAW "gfx/Target050.raw"
#define TARGET050_MASK "gfx/Target050.mask"

#define SUMMARY_TARGET050_RAW "gfx/Target050.raw"
#define SUMMARY_TARGET050_MASK "gfx/Target050.mask"
#define SUMMARY_TARGET050_W 48
#define SUMMARY_TARGET050_H 23
#define SUMMARY_TARGET050_X 136
#define SUMMARY_TARGET050_BOTTOM_Y 147
#define SUMMARY_TARGET050_Y (SUMMARY_TARGET050_BOTTOM_Y - SUMMARY_TARGET050_H)
#define SUMMARY_DISTANCE_Y 48
#define SUMMARY_DISTANCE_PEN 16
#define SUMMARY_HIT_MARK_PEN 20
#define SUMMARY_HIT_MARK_CENTER_PEN 24
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


typedef struct SummaryPoint {
    UBYTE x;
    UBYTE y;
} SummaryPoint;

#define SUMMARY_POINT_INVALID 255

static const SummaryPoint gSummaryMap100ToTarget050[T100_H][T100_W] = {
    /* y = 0 */
    {{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{20,1},{21,1},{23,1},
     {24,1},{26,1},{27,1},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255}},

    /* y = 1 */
    {{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{18,3},{20,3},{21,3},{23,3},
     {24,3},{26,3},{27,3},{29,3},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255}},

    /* y = 2 */
    {{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{16,5},{18,5},{20,5},{22,5},
     {25,5},{27,5},{29,5},{31,5},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255}},

    /* y = 3 */
    {{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{14,7},{16,7},{18,7},{20,7},{22,7},
     {25,7},{27,7},{29,7},{31,7},{33,7},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255}},

    /* y = 4 */
    {{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{12,9},{15,9},{17,9},{20,9},{22,9},
     {25,9},{27,9},{30,9},{32,9},{35,9},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255}},

    /* y = 5 */
    {{255,255},{255,255},{255,255},{255,255},{255,255},{255,255},{10,11},{12,11},{15,11},{17,11},{20,11},{22,11},
     {25,11},{27,11},{30,11},{32,11},{35,11},{37,11},{255,255},{255,255},{255,255},{255,255},{255,255},{255,255}},

    /* y = 6 */
    {{255,255},{255,255},{255,255},{255,255},{255,255},{8,13},{10,13},{13,13},{15,13},{18,13},{20,13},{22,13},
     {25,13},{27,13},{29,13},{32,13},{34,13},{37,13},{39,13},{255,255},{255,255},{255,255},{255,255},{255,255}},

    /* y = 7 */
    {{255,255},{255,255},{255,255},{255,255},{5,15},{7,15},{10,15},{12,15},{15,15},{17,15},{20,15},{22,15},
     {25,15},{27,15},{30,15},{32,15},{35,15},{37,15},{40,15},{42,15},{255,255},{255,255},{255,255},{255,255}},

    /* y = 8 */
    {{255,255},{255,255},{2,17},{4,17},{7,17},{9,17},{11,17},{13,17},{16,17},{18,17},{20,17},{22,17},
     {25,17},{27,17},{29,17},{31,17},{34,17},{36,17},{38,17},{40,17},{43,17},{45,17},{255,255},{255,255}},

    /* y = 9 */
    {{255,255},{0,19},{2,19},{4,19},{7,19},{9,19},{11,19},{13,19},{16,19},{18,19},{20,19},{22,19},
     {25,19},{27,19},{29,19},{31,19},{34,19},{36,19},{38,19},{40,19},{43,19},{45,19},{47,19},{255,255}},

    /* y = 10 */
    {{0,21},{2,21},{4,21},{6,21},{8,21},{10,21},{12,21},{14,21},{16,21},{18,21},{20,21},{22,21},
     {25,21},{27,21},{29,21},{31,21},{33,21},{35,21},{37,21},{39,21},{41,21},{43,21},{45,21},{47,21}}
};

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

static void WritePixelOnScreen(struct RastPort *rp, WORD x, WORD y) {
    if (!rp) {
        return;
    }

    if (x < 0 || y < 0) {
        return;
    }

    if (x >= LO_WIDTH || y >= LO_HEIGHT) {
        return;
    }

    WritePixel(rp, x, y);
}

static void WritePixelInRect(struct RastPort *rp, WORD x, WORD y, WORD rectX, WORD rectY,
                             UWORD rectW, UWORD rectH) {
    if (!rp) {
        return;
    }

    if (x < rectX || y < rectY) {
        return;
    }

    if (x >= (WORD)(rectX + rectW) || y >= (WORD)(rectY + rectH)) {
        return;
    }

    WritePixel(rp, x, y);
}

static void DrawSummaryHitMark(struct RastPort *rp, WORD x, WORD y, WORD rectX, WORD rectY,
                               UWORD rectW, UWORD rectH) {
    /*
     * The center pixel is the actual hit and must stay inside the target hit map.
     * The surrounding pixels are only a visual marker and may extend outside
     * the target BOB outline/bounding box, as long as they stay on screen.
     */
    SetAPen(rp, SUMMARY_HIT_MARK_PEN);
    WritePixelOnScreen(rp, x - 1, y);
    WritePixelOnScreen(rp, x + 1, y);
    WritePixelOnScreen(rp, x, y - 1);
    WritePixelOnScreen(rp, x, y + 1);

    SetAPen(rp, SUMMARY_HIT_MARK_CENTER_PEN);
    WritePixelInRect(rp, x, y, rectX, rectY, rectW, rectH);
}

static void DrawSummaryHitMarks050(struct RastPort *rp, WORD targetX, WORD targetY) {
    const UWORD *hitMap;
    UWORD width;
    UWORD height;
    UWORD x;
    UWORD y;

    hitMap = TargetScoring_GetHitMap050(&width, &height);
    if (!rp || !hitMap) {
        return;
    }

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (hitMap[(y * width) + x] != 0) {
                DrawSummaryHitMark(rp, (WORD)(targetX + x), (WORD)(targetY + y),
                                   targetX, targetY, width, height);
            }
        }
    }
}

static void DrawSummaryHitMarks100(struct RastPort *rp, WORD targetX, WORD targetY) {
    const UWORD *hitMap;
    UWORD width;
    UWORD height;
    UWORD x;
    UWORD y;
    const SummaryPoint *point;

    hitMap = TargetScoring_GetHitMap100(&width, &height);
    if (!rp || !hitMap) {
        return;
    }

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (hitMap[(y * width) + x] != 0) {
                point = &gSummaryMap100ToTarget050[y][x];

                if (point->x != SUMMARY_POINT_INVALID && point->y != SUMMARY_POINT_INVALID) {
                    DrawSummaryHitMark(rp, (WORD)(targetX + point->x), (WORD)(targetY + point->y),
                                       targetX, targetY, SUMMARY_TARGET050_W, SUMMARY_TARGET050_H);
                }
            }
        }
    }
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
    typedef struct SummaryStep {
        const char *distanceText;
        const AmacsBob *bob;
        WORD x;
        WORD y;
        BOOL showTotals;
    } SummaryStep;

    struct RastPort *rp;
    struct Screen *scr;
    struct TextFont *font = NULL;
    AmacsBob scoringETypeBob;
    AmacsBob target050Bob;
    BOOL scoringETypeLoaded = FALSE;
    BOOL target050Loaded = FALSE;
    SummaryStep steps[6];
    UWORD stepIndex = 0;
    char line[32];
    UWORD score;
    UWORD acc;
    WaitResult waitResult;
    BOOL summaryDbufEnabled = FALSE;

    memset(&scoringETypeBob, 0, sizeof(scoringETypeBob));
    memset(&target050Bob, 0, sizeof(target050Bob));

    if (!Gfx_CrossFadeToImage(SUMMARY_FILE, rangePalette, 32, SummaryPaletteRGB4, 32)) {
        return FALSE;
    }

    scr = Gfx_GetScreen();
    rp = Gfx_GetDrawRastPort();

    if (!scr || !rp) {
        return FALSE;
    }

    if (Bob_LoadRawAndMask(&target050Bob, SUMMARY_TARGET050_RAW, SUMMARY_TARGET050_MASK,
                           SUMMARY_TARGET050_W, SUMMARY_TARGET050_H, 5)) {
        target050Loaded = TRUE;
    }

    if (Bob_LoadRawAndMask(&scoringETypeBob, SUMMARY_SCORING_ETYPE_RAW, SUMMARY_SCORING_ETYPE_MASK,
                           SUMMARY_SCORING_ETYPE_W, SUMMARY_SCORING_ETYPE_H, 5)) {
        scoringETypeLoaded = TRUE;
    }

    font = OpenFont(&gSummaryFontAttr);

    score = summary ? summary->score : 0;
    acc = summary ? summary->accuracy : 0;

    if (Gfx_EnableDoubleBuffering()) {
        summaryDbufEnabled = TRUE;
    }

    steps[0].distanceText = "50 Meter";
    steps[0].bob = target050Loaded ? &target050Bob : NULL;
    steps[0].x = SUMMARY_TARGET050_X;
    steps[0].y = SUMMARY_TARGET050_Y;
    steps[0].showTotals = FALSE;

    steps[1].distanceText = "100 Meter";
    steps[1].bob = target050Loaded ? &target050Bob : NULL;
    steps[1].x = SUMMARY_TARGET050_X;
    steps[1].y = SUMMARY_TARGET050_Y;
    steps[1].showTotals = FALSE;

    steps[2].distanceText = "150 Meter";
    steps[2].bob = scoringETypeLoaded ? &scoringETypeBob : NULL;
    steps[2].x = SUMMARY_SCORING_ETYPE_X;
    steps[2].y = SUMMARY_SCORING_ETYPE_Y;
    steps[2].showTotals = FALSE;

    steps[3].distanceText = "200 Meter";
    steps[3].bob = scoringETypeLoaded ? &scoringETypeBob : NULL;
    steps[3].x = SUMMARY_SCORING_ETYPE_X;
    steps[3].y = SUMMARY_SCORING_ETYPE_Y;
    steps[3].showTotals = FALSE;

    steps[4].distanceText = "250 Meter";
    steps[4].bob = scoringETypeLoaded ? &scoringETypeBob : NULL;
    steps[4].x = SUMMARY_SCORING_ETYPE_X;
    steps[4].y = SUMMARY_SCORING_ETYPE_Y;
    steps[4].showTotals = FALSE;

    steps[5].distanceText = "300 Meter";
    steps[5].bob = scoringETypeLoaded ? &scoringETypeBob : NULL;
    steps[5].x = SUMMARY_SCORING_ETYPE_X;
    steps[5].y = SUMMARY_SCORING_ETYPE_Y;
    steps[5].showTotals = TRUE;

    for (;;) {
        rp = Gfx_GetDrawRastPort();
        if (!rp) {
            if (summaryDbufEnabled) {
                Gfx_DisableDoubleBuffering();
                summaryDbufEnabled = FALSE;
            }
            if (font)
                CloseFont(font);
            if (target050Loaded)
                Bob_Free(&target050Bob);
            if (scoringETypeLoaded)
                Bob_Free(&scoringETypeBob);
            return FALSE;
        }

        if (!LoadRawImageToRastPort(SUMMARY_FILE, rp, LO_WIDTH, LO_HEIGHT)) {
            if (summaryDbufEnabled) {
                Gfx_DisableDoubleBuffering();
                summaryDbufEnabled = FALSE;
            }
            if (font)
                CloseFont(font);
            if (target050Loaded)
                Bob_Free(&target050Bob);
            if (scoringETypeLoaded)
                Bob_Free(&scoringETypeBob);
            return FALSE;
        }

        if (steps[stepIndex].bob) {
            Bob_DrawMaskedToRastPort(steps[stepIndex].bob, rp, steps[stepIndex].x, steps[stepIndex].y);
        }

        if (stepIndex == 0 && steps[stepIndex].bob) {
            DrawSummaryHitMarks050(rp, steps[stepIndex].x, steps[stepIndex].y);
        }

        if (stepIndex == 1 && steps[stepIndex].bob) {
            DrawSummaryHitMarks100(rp, steps[stepIndex].x, steps[stepIndex].y);
        }

        DrawCenteredTextWithShadowMain(rp, font, SUMMARY_DISTANCE_Y, SUMMARY_DISTANCE_PEN,
                                       SUMMARY_SHADOW_PEN, steps[stepIndex].distanceText);

        if (steps[stepIndex].showTotals) {
            BuildSummaryScoreLine(line, score);
            DrawCenteredTextWithShadowMain(rp, font, SUMMARY_SCORE_Y, 31, 24, line);

            BuildSummaryAccuracyLine(line, acc);
            DrawCenteredTextWithShadowMain(rp, font, SUMMARY_ACCURACY_Y, 31, 24, line);
        }

        if (summaryDbufEnabled) {
            Gfx_SwapBuffers();
        }

        waitResult = WaitForAdvanceOnly();
        if (waitResult == WAIT_ESC) {
            if (summaryDbufEnabled) {
                Gfx_DisableDoubleBuffering();
                summaryDbufEnabled = FALSE;
            }
            if (font)
                CloseFont(font);
            if (target050Loaded)
                Bob_Free(&target050Bob);
            if (scoringETypeLoaded)
                Bob_Free(&scoringETypeBob);
            return FALSE;
        }

        if (stepIndex >= 5) {
            break;
        }

        stepIndex++;
    }

    if (summaryDbufEnabled) {
        Gfx_DisableDoubleBuffering();
        summaryDbufEnabled = FALSE;
    }

    if (font)
        CloseFont(font);

    if (target050Loaded) {
        Bob_Free(&target050Bob);
    }

    if (scoringETypeLoaded) {
        Bob_Free(&scoringETypeBob);
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
