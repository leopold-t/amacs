#include <exec/types.h>
#include <graphics/gfx.h>
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
#include "imageHandler.h"
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
static const UWORD SummaryPaletteRGB4[32] = {
    0x0000, 0x0005, 0x000C, 0x010D, 0x0119, 0x010B, 0x0113, 0x0C00, 0x099C, 0x0557, 0x055A,
    0x0D61, 0x065D, 0x0DC1, 0x01A0, 0x088E, 0x0DC1, 0x0999, 0x099C, 0x0A9E, 0x0CCC, 0x0CCE,
    0x01F0, 0x0666, 0x0222, 0x03C2, 0x088E, 0x065D, 0x0005, 0x0119, 0x010B, 0x0EEE};

#define SUMMARY_TEXT_PEN 25
#define SUMMARY_SHADOW_PEN 24
#define SUMMARY_BACKGROUND_PEN 3
#define SUMMARY_TITLE_Y 18
#define SUMMARY_TABLE_HEADER_Y 40
#define SUMMARY_TABLE_FIRST_ROW_Y 56
#define SUMMARY_TABLE_ROW_STEP_Y 14
#define SUMMARY_TABLE_LABEL_X 104
#define SUMMARY_TABLE_HITS_X 216
#define SUMMARY_SCORE_LEFT_X 18
#define SUMMARY_SCORE_RIGHT_X 166
#define SUMMARY_HIT_SCORE_Y 163
#define SUMMARY_TIME_BONUS_Y 179
#define SUMMARY_TOTAL_SCORE_Y 195
#define SUMMARY_ACCURACY_Y 163
#define SUMMARY_RANK_Y 179
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

#define SUMMARY_SCORING_FTYPE_RAW "gfx/ScoringFType.raw"
#define SUMMARY_SCORING_FTYPE_MASK "gfx/ScoringFType.mask"
#define SUMMARY_SCORING_FTYPE_W 74
#define SUMMARY_SCORING_FTYPE_H 36
#define SUMMARY_SCORING_FTYPE_X 123
#define SUMMARY_SCORING_FTYPE_BOTTOM_Y 147
#define SUMMARY_SCORING_FTYPE_Y (SUMMARY_SCORING_FTYPE_BOTTOM_Y - SUMMARY_SCORING_FTYPE_H)
#define SUMMARY_DISTANCE_Y 55
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

static BOOL ShowTitleScorePlaceholderScreen(void);

typedef struct SummaryPoint {
    UBYTE x;
    UBYTE y;
} SummaryPoint;

#define SUMMARY_POINT_INVALID 255

static const SummaryPoint gSummaryMap050ToScoringFType[23][48] = {
    /* y = 0 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {32, 0},
     {33, 0},    {35, 0},    {36, 0},    {37, 0},    {38, 0},    {40, 0},    {41, 0},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 1 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {28, 2},    {30, 2},    {31, 2},
     {33, 2},    {34, 2},    {36, 2},    {37, 2},    {39, 2},    {40, 2},    {42, 2},
     {43, 2},    {45, 2},    {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 2 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {27, 3},    {28, 3},    {30, 3},    {31, 3},
     {33, 3},    {34, 3},    {36, 3},    {37, 3},    {39, 3},    {40, 3},    {42, 3},
     {43, 3},    {45, 3},    {46, 3},    {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 3 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {25, 5},    {27, 5},    {28, 5},    {30, 5},    {31, 5},
     {33, 5},    {34, 5},    {36, 5},    {37, 5},    {39, 5},    {40, 5},    {42, 5},
     {43, 5},    {45, 5},    {46, 5},    {48, 5},    {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 4 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {24, 6},    {25, 6},    {27, 6},    {28, 6},    {30, 6},    {31, 6},
     {33, 6},    {34, 6},    {36, 6},    {37, 6},    {39, 6},    {40, 6},    {42, 6},
     {43, 6},    {45, 6},    {46, 6},    {48, 6},    {49, 6},    {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 5 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {23, 8},    {25, 8},    {26, 8},    {28, 8},    {29, 8},    {31, 8},
     {33, 8},    {34, 8},    {36, 8},    {37, 8},    {39, 8},    {40, 8},    {42, 8},
     {44, 8},    {45, 8},    {47, 8},    {48, 8},    {50, 8},    {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 6 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {22, 10},   {24, 10},   {25, 10},   {27, 10},   {28, 10},   {30, 10},   {31, 10},
     {33, 10},   {34, 10},   {36, 10},   {37, 10},   {39, 10},   {40, 10},   {42, 10},
     {43, 10},   {45, 10},   {46, 10},   {48, 10},   {49, 10},   {51, 10},   {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 7 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {22, 11},   {24, 11},   {25, 11},   {27, 11},   {28, 11},   {30, 11},   {31, 11},
     {33, 11},   {34, 11},   {36, 11},   {37, 11},   {39, 11},   {40, 11},   {42, 11},
     {43, 11},   {45, 11},   {46, 11},   {48, 11},   {49, 11},   {51, 11},   {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 8 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {21, 13},   {23, 13},   {24, 13},   {26, 13},   {28, 13},   {29, 13},   {31, 13},
     {32, 13},   {34, 13},   {36, 13},   {37, 13},   {39, 13},   {41, 13},   {42, 13},
     {44, 13},   {45, 13},   {47, 13},   {49, 13},   {50, 13},   {52, 13},   {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 9 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {21, 14},
     {22, 14},   {24, 14},   {25, 14},   {27, 14},   {28, 14},   {30, 14},   {31, 14},
     {33, 14},   {34, 14},   {36, 14},   {37, 14},   {39, 14},   {40, 14},   {42, 14},
     {43, 14},   {45, 14},   {46, 14},   {48, 14},   {49, 14},   {51, 14},   {52, 14},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 10 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {20, 16},
     {22, 16},   {23, 16},   {25, 16},   {26, 16},   {28, 16},   {29, 16},   {31, 16},
     {33, 16},   {34, 16},   {36, 16},   {37, 16},   {39, 16},   {40, 16},   {42, 16},
     {44, 16},   {45, 16},   {47, 16},   {48, 16},   {50, 16},   {51, 16},   {53, 16},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 11 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {18, 18},   {20, 18},
     {21, 18},   {23, 18},   {24, 18},   {26, 18},   {28, 18},   {29, 18},   {31, 18},
     {32, 18},   {34, 18},   {36, 18},   {37, 18},   {39, 18},   {41, 18},   {42, 18},
     {44, 18},   {45, 18},   {47, 18},   {49, 18},   {50, 18},   {52, 18},   {53, 18},
     {55, 18},   {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 12 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {18, 19},   {20, 19},   {21, 19},
     {23, 19},   {24, 19},   {26, 19},   {27, 19},   {29, 19},   {30, 19},   {32, 19},
     {33, 19},   {35, 19},   {36, 19},   {38, 19},   {39, 19},   {41, 19},   {42, 19},
     {44, 19},   {45, 19},   {47, 19},   {48, 19},   {50, 19},   {51, 19},   {53, 19},
     {54, 19},   {56, 19},   {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 13 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {16, 21},   {18, 21},   {19, 21},   {21, 21},
     {22, 21},   {24, 21},   {25, 21},   {27, 21},   {28, 21},   {30, 21},   {31, 21},
     {33, 21},   {34, 21},   {36, 21},   {37, 21},   {39, 21},   {40, 21},   {42, 21},
     {43, 21},   {45, 21},   {46, 21},   {48, 21},   {49, 21},   {51, 21},   {52, 21},
     {54, 21},   {55, 21},   {57, 21},   {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 14 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {15, 22},   {16, 22},   {18, 22},   {19, 22},   {21, 22},   {22, 22},
     {24, 22},   {25, 22},   {27, 22},   {28, 22},   {30, 22},   {31, 22},   {33, 22},
     {34, 22},   {36, 22},   {37, 22},   {38, 22},   {40, 22},   {41, 22},   {43, 22},
     {44, 22},   {46, 22},   {47, 22},   {49, 22},   {50, 22},   {52, 22},   {53, 22},
     {55, 22},   {56, 22},   {58, 22},   {59, 22},   {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 15 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {12, 24},
     {14, 24},   {15, 24},   {17, 24},   {18, 24},   {20, 24},   {21, 24},   {23, 24},
     {24, 24},   {26, 24},   {27, 24},   {29, 24},   {30, 24},   {32, 24},   {33, 24},
     {35, 24},   {36, 24},   {38, 24},   {39, 24},   {41, 24},   {42, 24},   {44, 24},
     {45, 24},   {47, 24},   {48, 24},   {50, 24},   {51, 24},   {53, 24},   {54, 24},
     {56, 24},   {57, 24},   {59, 24},   {60, 24},   {62, 24},   {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 16 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {10, 25},   {11, 25},  {13, 25},
     {14, 25},   {16, 25},   {17, 25},   {19, 25},   {20, 25},   {22, 25},  {23, 25},
     {25, 25},   {26, 25},   {28, 25},   {29, 25},   {30, 25},   {32, 25},  {33, 25},
     {35, 25},   {36, 25},   {38, 25},   {39, 25},   {41, 25},   {42, 25},  {44, 25},
     {45, 25},   {46, 25},   {48, 25},   {49, 25},   {51, 25},   {52, 25},  {54, 25},
     {55, 25},   {57, 25},   {58, 25},   {60, 25},   {61, 25},   {63, 25},  {64, 25},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 17 */
    {{255, 255}, {255, 255}, {6, 27},  {8, 27},  {9, 27},    {11, 27},   {12, 27},   {14, 27},
     {15, 27},   {17, 27},   {18, 27}, {20, 27}, {21, 27},   {23, 27},   {24, 27},   {26, 27},
     {27, 27},   {29, 27},   {30, 27}, {32, 27}, {33, 27},   {35, 27},   {36, 27},   {38, 27},
     {39, 27},   {41, 27},   {42, 27}, {44, 27}, {45, 27},   {47, 27},   {48, 27},   {50, 27},
     {51, 27},   {53, 27},   {54, 27}, {56, 27}, {57, 27},   {59, 27},   {60, 27},   {62, 27},
     {63, 27},   {65, 27},   {66, 27}, {68, 27}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 18 */
    {{255, 255}, {3, 29},  {5, 29},  {6, 29},  {8, 29},  {9, 29},  {11, 29},   {12, 29},
     {14, 29},   {15, 29}, {17, 29}, {18, 29}, {20, 29}, {22, 29}, {23, 29},   {25, 29},
     {26, 29},   {28, 29}, {29, 29}, {31, 29}, {32, 29}, {34, 29}, {35, 29},   {37, 29},
     {39, 29},   {40, 29}, {42, 29}, {43, 29}, {45, 29}, {46, 29}, {48, 29},   {49, 29},
     {51, 29},   {52, 29}, {54, 29}, {56, 29}, {57, 29}, {59, 29}, {60, 29},   {62, 29},
     {63, 29},   {65, 29}, {66, 29}, {68, 29}, {69, 29}, {71, 29}, {255, 255}, {255, 255}},

    /* y = 19 */
    {{2, 30},  {4, 30},  {5, 30},  {6, 30},  {8, 30},  {10, 30}, {11, 30}, {12, 30},
     {14, 30}, {16, 30}, {17, 30}, {18, 30}, {20, 30}, {22, 30}, {23, 30}, {24, 30},
     {26, 30}, {28, 30}, {29, 30}, {30, 30}, {32, 30}, {34, 30}, {35, 30}, {36, 30},
     {38, 30}, {40, 30}, {41, 30}, {42, 30}, {44, 30}, {46, 30}, {47, 30}, {48, 30},
     {50, 30}, {52, 30}, {53, 30}, {54, 30}, {56, 30}, {58, 30}, {59, 30}, {60, 30},
     {62, 30}, {64, 30}, {65, 30}, {66, 30}, {68, 30}, {70, 30}, {71, 30}, {255, 255}},

    /* y = 20 */
    {{1, 32},  {3, 32},  {4, 32},  {6, 32},  {7, 32},  {9, 32},  {10, 32}, {12, 32},
     {13, 32}, {15, 32}, {16, 32}, {18, 32}, {19, 32}, {21, 32}, {22, 32}, {24, 32},
     {25, 32}, {27, 32}, {28, 32}, {30, 32}, {31, 32}, {33, 32}, {34, 32}, {36, 32},
     {37, 32}, {39, 32}, {40, 32}, {42, 32}, {43, 32}, {45, 32}, {46, 32}, {48, 32},
     {49, 32}, {51, 32}, {52, 32}, {54, 32}, {55, 32}, {57, 32}, {58, 32}, {60, 32},
     {61, 32}, {63, 32}, {64, 32}, {66, 32}, {67, 32}, {69, 32}, {70, 32}, {72, 32}},

    /* y = 21 */
    {{0, 33},  {2, 33},  {3, 33},  {5, 33},  {6, 33},  {8, 33},  {9, 33},  {11, 33},
     {12, 33}, {14, 33}, {16, 33}, {17, 33}, {19, 33}, {20, 33}, {22, 33}, {23, 33},
     {25, 33}, {26, 33}, {28, 33}, {30, 33}, {31, 33}, {33, 33}, {34, 33}, {36, 33},
     {37, 33}, {39, 33}, {40, 33}, {42, 33}, {43, 33}, {45, 33}, {47, 33}, {48, 33},
     {50, 33}, {51, 33}, {53, 33}, {54, 33}, {56, 33}, {57, 33}, {59, 33}, {61, 33},
     {62, 33}, {64, 33}, {65, 33}, {67, 33}, {68, 33}, {70, 33}, {71, 33}, {73, 33}},

    /* y = 22 */
    {{0, 35},  {2, 35},  {3, 35},  {5, 35},  {6, 35},  {8, 35},  {9, 35},  {11, 35},
     {12, 35}, {14, 35}, {16, 35}, {17, 35}, {19, 35}, {20, 35}, {22, 35}, {23, 35},
     {25, 35}, {26, 35}, {28, 35}, {30, 35}, {31, 35}, {33, 35}, {34, 35}, {36, 35},
     {37, 35}, {39, 35}, {40, 35}, {42, 35}, {43, 35}, {45, 35}, {47, 35}, {48, 35},
     {50, 35}, {51, 35}, {53, 35}, {54, 35}, {56, 35}, {57, 35}, {59, 35}, {61, 35},
     {62, 35}, {64, 35}, {65, 35}, {67, 35}, {68, 35}, {70, 35}, {71, 35}, {73, 35}}

};

static const SummaryPoint gSummaryMap100ToScoringFType[11][24] = {
    /* y = 0 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {32, 0},    {33, 0},    {35, 0},    {36, 0},
     {37, 0},    {38, 0},    {40, 0},    {41, 0},    {255, 255}, {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 1 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {255, 255}, {26, 4},    {28, 4},    {31, 4},    {33, 4},    {36, 4},
     {38, 4},    {41, 4},    {43, 4},    {46, 4},    {48, 4},    {255, 255},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 2 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {23, 7},    {25, 7},    {28, 7},    {30, 7},    {33, 7},    {35, 7},
     {38, 7},    {40, 7},    {43, 7},    {45, 7},    {48, 7},    {50, 7},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 3 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {22, 10},   {25, 10},   {27, 10},   {30, 10},   {33, 10},   {35, 10},
     {38, 10},   {40, 10},   {43, 10},   {46, 10},   {48, 10},   {51, 10},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 4 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255},
     {21, 14},   {24, 14},   {27, 14},   {29, 14},   {32, 14},   {35, 14},
     {38, 14},   {41, 14},   {44, 14},   {46, 14},   {49, 14},   {52, 14},
     {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 5 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}, {18, 18},
     {21, 18},   {24, 18},   {27, 18},   {29, 18},   {32, 18},   {35, 18},
     {38, 18},   {41, 18},   {44, 18},   {46, 18},   {49, 18},   {52, 18},
     {55, 18},   {255, 255}, {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 6 */
    {{255, 255}, {255, 255}, {255, 255}, {255, 255}, {16, 21},   {19, 21},
     {21, 21},   {24, 21},   {27, 21},   {30, 21},   {32, 21},   {35, 21},
     {38, 21},   {41, 21},   {43, 21},   {46, 21},   {49, 21},   {52, 21},
     {54, 21},   {57, 21},   {255, 255}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 7 */
    {{255, 255}, {255, 255}, {12, 24}, {15, 24}, {17, 24}, {20, 24}, {23, 24},   {25, 24},
     {28, 24},   {30, 24},   {33, 24}, {36, 24}, {38, 24}, {41, 24}, {44, 24},   {46, 24},
     {49, 24},   {51, 24},   {54, 24}, {57, 24}, {59, 24}, {62, 24}, {255, 255}, {255, 255}},

    /* y = 8 */
    {{255, 255}, {4, 28},  {7, 28},  {10, 28}, {13, 28}, {16, 28}, {19, 28}, {23, 28},
     {26, 28},   {29, 28}, {32, 28}, {35, 28}, {38, 28}, {41, 28}, {44, 28}, {47, 28},
     {50, 28},   {54, 28}, {57, 28}, {60, 28}, {63, 28}, {66, 28}, {69, 28}, {255, 255}},

    /* y = 9 */
    {{1, 32},  {4, 32},  {7, 32},  {10, 32}, {13, 32}, {16, 32}, {20, 32}, {23, 32},
     {26, 32}, {29, 32}, {32, 32}, {35, 32}, {38, 32}, {41, 32}, {44, 32}, {47, 32},
     {50, 32}, {53, 32}, {57, 32}, {60, 32}, {63, 32}, {66, 32}, {69, 32}, {72, 32}},

    /* y = 10 */
    {{0, 35},  {3, 35},  {6, 35},  {10, 35}, {13, 35}, {16, 35}, {19, 35}, {22, 35},
     {25, 35}, {29, 35}, {32, 35}, {35, 35}, {38, 35}, {41, 35}, {44, 35}, {48, 35},
     {51, 35}, {54, 35}, {57, 35}, {60, 35}, {63, 35}, {67, 35}, {70, 35}, {73, 35}}

};

static const SummaryPoint gSummaryMap150ToScoringEType[T150_H][T150_W] = {
    /* y = 0 */
    {{255, 255},
     {255, 255},
     {255, 255},
     {15, 0},
     {19, 0},
     {22, 0},
     {255, 255},
     {255, 255},
     {255, 255}},

    /* y = 1 */
    {{255, 255}, {255, 255}, {10, 4}, {14, 4}, {19, 4}, {23, 4}, {27, 4}, {255, 255}, {255, 255}},

    /* y = 2 */
    {{255, 255}, {255, 255}, {10, 8}, {14, 8}, {19, 8}, {23, 8}, {27, 8}, {255, 255}, {255, 255}},

    /* y = 3 */
    {{255, 255},
     {255, 255},
     {10, 13},
     {14, 13},
     {19, 13},
     {23, 13},
     {27, 13},
     {255, 255},
     {255, 255}},

    /* y = 4 */
    {{255, 255}, {9, 17}, {12, 17}, {15, 17}, {19, 17}, {22, 17}, {25, 17}, {28, 17}, {255, 255}},

    /* y = 5 */
    {{3, 21}, {7, 21}, {11, 21}, {15, 21}, {19, 21}, {22, 21}, {26, 21}, {30, 21}, {34, 21}},

    /* y = 6 */
    {{0, 25}, {5, 25}, {9, 25}, {14, 25}, {19, 25}, {23, 25}, {28, 25}, {32, 25}, {37, 25}},

    /* y = 7 */
    {{0, 29}, {5, 29}, {9, 29}, {14, 29}, {19, 29}, {23, 29}, {28, 29}, {32, 29}, {37, 29}},

    /* y = 8 */
    {{0, 34}, {5, 34}, {9, 34}, {14, 34}, {19, 34}, {23, 34}, {28, 34}, {32, 34}, {37, 34}},

    /* y = 9 */
    {{0, 38}, {5, 38}, {9, 38}, {14, 38}, {19, 38}, {23, 38}, {28, 38}, {32, 38}, {37, 38}},

    /* y = 10 */
    {{0, 42}, {5, 42}, {9, 42}, {14, 42}, {19, 42}, {23, 42}, {28, 42}, {32, 42}, {37, 42}},

    /* y = 11 */
    {{0, 46}, {5, 46}, {9, 46}, {14, 46}, {19, 46}, {23, 46}, {28, 46}, {32, 46}, {37, 46}},

    /* y = 12 */
    {{0, 50}, {5, 50}, {9, 50}, {14, 50}, {19, 50}, {23, 50}, {28, 50}, {32, 50}, {37, 50}},

    /* y = 13 */
    {{0, 54}, {5, 54}, {9, 54}, {14, 54}, {19, 54}, {23, 54}, {28, 54}, {32, 54}, {37, 54}},

    /* y = 14 */
    {{0, 59}, {5, 59}, {9, 59}, {14, 59}, {19, 59}, {23, 59}, {28, 59}, {32, 59}, {37, 59}},

    /* y = 15 */
    {{0, 63}, {5, 63}, {9, 63}, {14, 63}, {19, 63}, {23, 63}, {28, 63}, {32, 63}, {37, 63}},

    /* y = 16 */
    {{0, 67}, {5, 67}, {9, 67}, {14, 67}, {19, 67}, {23, 67}, {28, 67}, {32, 67}, {37, 67}},

};

static const SummaryPoint gSummaryMap200ToScoringEType[T200_H][T200_W] = {
    /* y = 0 */
    {{255, 255}, {255, 255}, {255, 255}, {15, 0}, {22, 0}, {255, 255}, {255, 255}, {255, 255}},

    /* y = 1 */
    {{255, 255}, {255, 255}, {10, 5}, {16, 5}, {21, 5}, {27, 5}, {255, 255}, {255, 255}},

    /* y = 2 */
    {{255, 255}, {255, 255}, {10, 10}, {16, 10}, {21, 10}, {27, 10}, {255, 255}, {255, 255}},

    /* y = 3 */
    {{255, 255}, {10, 14}, {13, 14}, {17, 14}, {20, 14}, {24, 14}, {27, 14}, {255, 255}},

    /* y = 4 */
    {{9, 19}, {12, 19}, {14, 19}, {17, 19}, {20, 19}, {23, 19}, {25, 19}, {28, 19}},

    /* y = 5 */
    {{0, 24}, {5, 24}, {11, 24}, {16, 24}, {21, 24}, {26, 24}, {32, 24}, {37, 24}},

    /* y = 6 */
    {{0, 29}, {5, 29}, {11, 29}, {16, 29}, {21, 29}, {26, 29}, {32, 29}, {37, 29}},

    /* y = 7 */
    {{0, 34}, {5, 34}, {11, 34}, {16, 34}, {21, 34}, {26, 34}, {32, 34}, {37, 34}},

    /* y = 8 */
    {{0, 38}, {5, 38}, {11, 38}, {16, 38}, {21, 38}, {26, 38}, {32, 38}, {37, 38}},

    /* y = 9 */
    {{0, 43}, {5, 43}, {11, 43}, {16, 43}, {21, 43}, {26, 43}, {32, 43}, {37, 43}},

    /* y = 10 */
    {{0, 48}, {5, 48}, {11, 48}, {16, 48}, {21, 48}, {26, 48}, {32, 48}, {37, 48}},

    /* y = 11 */
    {{0, 53}, {5, 53}, {11, 53}, {16, 53}, {21, 53}, {26, 53}, {32, 53}, {37, 53}},

    /* y = 12 */
    {{0, 57}, {5, 57}, {11, 57}, {16, 57}, {21, 57}, {26, 57}, {32, 57}, {37, 57}},

    /* y = 13 */
    {{0, 62}, {5, 62}, {11, 62}, {16, 62}, {21, 62}, {26, 62}, {32, 62}, {37, 62}},

    /* y = 14 */
    {{0, 67}, {5, 67}, {11, 67}, {16, 67}, {21, 67}, {26, 67}, {32, 67}, {37, 67}},

};

static const SummaryPoint gSummaryMap250ToScoringEType[T250_H][T250_W] = {
    /* y = 0 */
    {{255, 255}, {255, 255}, {15, 0}, {19, 0}, {22, 0}, {255, 255}, {255, 255}},

    /* y = 1 */
    {{255, 255}, {255, 255}, {10, 5}, {19, 5}, {27, 5}, {255, 255}, {255, 255}},

    /* y = 2 */
    {{255, 255}, {255, 255}, {10, 10}, {19, 10}, {27, 10}, {255, 255}, {255, 255}},

    /* y = 3 */
    {{255, 255}, {10, 15}, {14, 15}, {19, 15}, {23, 15}, {27, 15}, {255, 255}},

    /* y = 4 */
    {{3, 21}, {8, 21}, {13, 21}, {19, 21}, {24, 21}, {29, 21}, {34, 21}},

    /* y = 5 */
    {{0, 26}, {6, 26}, {12, 26}, {19, 26}, {25, 26}, {31, 26}, {37, 26}},

    /* y = 6 */
    {{0, 31}, {6, 31}, {12, 31}, {19, 31}, {25, 31}, {31, 31}, {37, 31}},

    /* y = 7 */
    {{0, 36}, {6, 36}, {12, 36}, {19, 36}, {25, 36}, {31, 36}, {37, 36}},

    /* y = 8 */
    {{0, 41}, {6, 41}, {12, 41}, {19, 41}, {25, 41}, {31, 41}, {37, 41}},

    /* y = 9 */
    {{0, 46}, {6, 46}, {12, 46}, {19, 46}, {25, 46}, {31, 46}, {37, 46}},

    /* y = 10 */
    {{0, 52}, {6, 52}, {12, 52}, {19, 52}, {25, 52}, {31, 52}, {37, 52}},

    /* y = 11 */
    {{0, 57}, {6, 57}, {12, 57}, {19, 57}, {25, 57}, {31, 57}, {37, 57}},

    /* y = 12 */
    {{0, 62}, {6, 62}, {12, 62}, {19, 62}, {25, 62}, {31, 62}, {37, 62}},

    /* y = 13 */
    {{0, 67}, {6, 67}, {12, 67}, {19, 67}, {25, 67}, {31, 67}, {37, 67}},

};

static const SummaryPoint gSummaryMap300ToScoringEType[T300_H][T300_W] = {
    /* y = 0 */
    {{255, 255}, {15, 0}, {19, 0}, {22, 0}, {255, 255}},

    /* y = 1 */
    {{255, 255}, {10, 7}, {19, 7}, {27, 7}, {255, 255}},

    /* y = 2 */
    {{10, 15}, {14, 15}, {19, 15}, {23, 15}, {27, 15}},

    /* y = 3 */
    {{2, 22}, {10, 22}, {19, 22}, {27, 22}, {35, 22}},

    /* y = 4 */
    {{0, 30}, {9, 30}, {19, 30}, {28, 30}, {37, 30}},

    /* y = 5 */
    {{0, 37}, {9, 37}, {19, 37}, {28, 37}, {37, 37}},

    /* y = 6 */
    {{0, 45}, {9, 45}, {19, 45}, {28, 45}, {37, 45}},

    /* y = 7 */
    {{0, 52}, {9, 52}, {19, 52}, {28, 52}, {37, 52}},

    /* y = 8 */
    {{0, 60}, {9, 60}, {19, 60}, {28, 60}, {37, 60}},

    /* y = 9 */
    {{0, 67}, {9, 67}, {19, 67}, {28, 67}, {37, 67}},

};

static void DrainWindowMessages(void);
static void PollAdvanceAndEsc(BOOL *outAdvance, BOOL *outEsc);
static WaitResult WaitForAdvanceNoTimeout(void);

static const struct TextAttr gSummaryFontAttr = {"topaz.font", 8, FS_NORMAL, FPF_ROMFONT};

#define HISCORE_ENTRY_COUNT 10
#define HISCORE_NAME_LEN 20
#define HISCORE_TEXT_PEN 19
#define HISCORE_SHADOW_PEN 0
#define HISCORE_TITLE_Y 35
#define HISCORE_FIRST_ENTRY_Y 62
#define HISCORE_ENTRY_STEP_Y 14
#define PULL_TRIGGER_POSITION_Y 214

typedef struct HiScoreEntry {
    char name[HISCORE_NAME_LEN + 1];
    UWORD score;
} HiScoreEntry;

static const HiScoreEntry gDefaultHiScores[HISCORE_ENTRY_COUNT] = {
    {"PFC BILL RIZER", 1024},     {"1LT SOLID SNAKE", 901},    {"1LT SONYA BLADE", 855},
    {"SGT ERNEST G. BILKO", 733}, {"1LT SNAKE PLISSKEN", 653}, {"GEN SPECIFIC", 512},
    {"PFC LANCE BEAN", 476},      {"MAJ JACKSON BRIGGS", 369}, {"SGT SLAUGHTER", 256},
    {"PVT PUBLIC", 112}};

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
    static const char prefix[] = "HIT SCORE: ";
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

static void BuildSummaryTimeBonusLine(char *buf, UWORD bonus) {
    static const char prefix[] = "TIME BONUS: ";
    UWORD i = 0;
    UWORD pos = 0;

    if (!buf) {
        return;
    }

    while (prefix[i] != '\0') {
        buf[pos++] = prefix[i++];
    }

    buf[pos] = '\0';
    AppendUnsignedMain(buf, pos, bonus);
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

static void BuildSummaryTotalScoreLine(char *buf, UWORD totalScore) {
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
    AppendUnsignedMain(buf, pos, totalScore);
}

static const char *GetSummaryRankText(UWORD accuracy) {
    if (accuracy >= 90) {
        return "EXPERT";
    }

    if (accuracy >= 75) {
        return "SHARPSHOOTER";
    }

    if (accuracy >= 56) {
        return "MARKSMAN";
    }

    return "UNQUALIFIED";
}

static void BuildSummaryRankLine(char *buf, UWORD accuracy) {
    static const char prefix[] = "RANK: ";
    const char *rank = GetSummaryRankText(accuracy);
    UWORD i = 0;
    UWORD pos = 0;

    if (!buf) {
        return;
    }

    while (prefix[i] != '\0') {
        buf[pos++] = prefix[i++];
    }

    i = 0;
    while (rank[i] != '\0') {
        buf[pos++] = rank[i++];
    }

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

static void DrawRightAlignedTextWithShadowMain(struct RastPort *rp, struct TextFont *font,
                                               WORD rightX, WORD y, UWORD pen, UWORD shadowPenValue,
                                               const char *text) {
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
    x = (WORD)(rightX - width);
    DrawTextWithShadowExMain(rp, font, x, y, pen, shadowPenValue, text, len);
}

static UWORD CountHitMapMain(const UWORD *map, UWORD width, UWORD height) {
    ULONG total = 0;
    ULONG cells;
    ULONG i;

    if (!map || width == 0 || height == 0) {
        return 0;
    }

    cells = (ULONG)width * (ULONG)height;
    for (i = 0; i < cells; i++) {
        total += map[i];
        if (total > 65535) {
            return 65535;
        }
    }

    return (UWORD)total;
}

static UWORD GetSummaryHitCountMain(UWORD distance) {
    const UWORD *map = NULL;
    UWORD width = 0;
    UWORD height = 0;

    switch (distance) {
        case 50:
            map = TargetScoring_GetHitMap050(&width, &height);
            break;
        case 100:
            map = TargetScoring_GetHitMap100(&width, &height);
            break;
        case 150:
            map = TargetScoring_GetHitMap150(&width, &height);
            break;
        case 200:
            map = TargetScoring_GetHitMap200(&width, &height);
            break;
        case 250:
            map = TargetScoring_GetHitMap250(&width, &height);
            break;
        case 300:
            map = TargetScoring_GetHitMap300(&width, &height);
            break;
        default:
            break;
    }

    return CountHitMapMain(map, width, height);
}

static UWORD AppendPaddedUnsignedMain(char *buf, UWORD pos, UWORD value, UWORD width) {
    char digits[5];
    UWORD count = 0;
    UWORD zeroCount;

    if (!buf) {
        return pos;
    }

    if (value == 0) {
        digits[count++] = '0';
    } else {
        while (value > 0 && count < 5) {
            digits[count++] = (char)('0' + (value % 10));
            value = (UWORD)(value / 10);
        }
    }

    zeroCount = (count < width) ? (UWORD)(width - count) : 0;
    while (zeroCount > 0) {
        buf[pos++] = '0';
        zeroCount--;
    }

    while (count > 0) {
        buf[pos++] = digits[--count];
    }

    buf[pos] = '\0';
    return pos;
}

static UWORD AppendHiScoreValueMain(char *buf, UWORD pos, UWORD value) {
    char digits[5];
    UWORD count = 0;
    UWORD minDigits = 3;
    UWORD fieldWidth = 5;
    UWORD padCount;
    UWORD zeroCount;

    if (!buf) {
        return pos;
    }

    if (value == 0) {
        digits[count++] = '0';
    } else {
        while (value > 0 && count < 5) {
            digits[count++] = (char)('0' + (value % 10));
            value = (UWORD)(value / 10);
        }
    }

    zeroCount = (count < minDigits) ? (UWORD)(minDigits - count) : 0;
    padCount = ((count + zeroCount) < fieldWidth) ? (UWORD)(fieldWidth - count - zeroCount) : 0;

    while (padCount > 0) {
        buf[pos++] = ' ';
        padCount--;
    }

    while (zeroCount > 0) {
        buf[pos++] = '0';
        zeroCount--;
    }

    while (count > 0) {
        buf[pos++] = digits[--count];
    }

    buf[pos] = '\0';
    return pos;
}

static void BuildHiScoreLine(char *buf, UWORD rank, const HiScoreEntry *entry) {
    UWORD pos = 0;
    UWORD i;

    if (!buf || !entry) {
        return;
    }

    pos = AppendPaddedUnsignedMain(buf, pos, rank, 2);
    buf[pos++] = ' ';
    buf[pos++] = ' ';

    for (i = 0; i < HISCORE_NAME_LEN; i++) {
        if (entry->name[i] != '\0') {
            buf[pos++] = entry->name[i];
        } else {
            buf[pos++] = ' ';
        }
    }

    buf[pos++] = ' ';
    buf[pos++] = ' ';
    pos = AppendHiScoreValueMain(buf, pos, entry->score);
    buf[pos] = '\0';
}

static void DrawHiScoreEntries(struct RastPort *rp, struct TextFont *font) {
    UWORD i;
    char line[34];

    if (!rp || !font) {
        return;
    }

    for (i = 0; i < HISCORE_ENTRY_COUNT; i++) {
        BuildHiScoreLine(line, (UWORD)(i + 1), &gDefaultHiScores[i]);
        DrawCenteredTextWithShadowMain(rp, font,
                                       (WORD)(HISCORE_FIRST_ENTRY_Y + i * HISCORE_ENTRY_STEP_Y),
                                       HISCORE_TEXT_PEN, HISCORE_SHADOW_PEN, line);
    }
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
    const SummaryPoint *point;

    hitMap = TargetScoring_GetHitMap050(&width, &height);
    if (!rp || !hitMap) {
        return;
    }

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (hitMap[(y * width) + x] != 0) {
                point = &gSummaryMap050ToScoringFType[y][x];

                if (point->x != SUMMARY_POINT_INVALID && point->y != SUMMARY_POINT_INVALID) {
                    DrawSummaryHitMark(rp, (WORD)(targetX + point->x), (WORD)(targetY + point->y),
                                       targetX, targetY, SUMMARY_SCORING_FTYPE_W,
                                       SUMMARY_SCORING_FTYPE_H);
                }
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
                point = &gSummaryMap100ToScoringFType[y][x];

                if (point->x != SUMMARY_POINT_INVALID && point->y != SUMMARY_POINT_INVALID) {
                    DrawSummaryHitMark(rp, (WORD)(targetX + point->x), (WORD)(targetY + point->y),
                                       targetX, targetY, SUMMARY_SCORING_FTYPE_W,
                                       SUMMARY_SCORING_FTYPE_H);
                }
            }
        }
    }
}

static void DrawSummaryHitMarksMapped(struct RastPort *rp, WORD targetX, WORD targetY,
                                      const UWORD *hitMap, UWORD width, UWORD height,
                                      const SummaryPoint *map) {
    UWORD x;
    UWORD y;
    const SummaryPoint *point;

    if (!rp || !hitMap || !map) {
        return;
    }

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (hitMap[(y * width) + x] != 0) {
                point = &map[(y * width) + x];

                if (point->x != SUMMARY_POINT_INVALID && point->y != SUMMARY_POINT_INVALID) {
                    DrawSummaryHitMark(rp, (WORD)(targetX + point->x), (WORD)(targetY + point->y),
                                       targetX, targetY, SUMMARY_SCORING_ETYPE_W,
                                       SUMMARY_SCORING_ETYPE_H);
                }
            }
        }
    }
}

static void DrawSummaryHitMarks150(struct RastPort *rp, WORD targetX, WORD targetY) {
    UWORD width;
    UWORD height;
    const UWORD *hitMap = TargetScoring_GetHitMap150(&width, &height);

    DrawSummaryHitMarksMapped(rp, targetX, targetY, hitMap, width, height,
                              (const SummaryPoint *)gSummaryMap150ToScoringEType);
}

static void DrawSummaryHitMarks200(struct RastPort *rp, WORD targetX, WORD targetY) {
    UWORD width;
    UWORD height;
    const UWORD *hitMap = TargetScoring_GetHitMap200(&width, &height);

    DrawSummaryHitMarksMapped(rp, targetX, targetY, hitMap, width, height,
                              (const SummaryPoint *)gSummaryMap200ToScoringEType);
}

static void DrawSummaryHitMarks250(struct RastPort *rp, WORD targetX, WORD targetY) {
    UWORD width;
    UWORD height;
    const UWORD *hitMap = TargetScoring_GetHitMap250(&width, &height);

    DrawSummaryHitMarksMapped(rp, targetX, targetY, hitMap, width, height,
                              (const SummaryPoint *)gSummaryMap250ToScoringEType);
}

static void DrawSummaryHitMarks300(struct RastPort *rp, WORD targetX, WORD targetY) {
    UWORD width;
    UWORD height;
    const UWORD *hitMap = TargetScoring_GetHitMap300(&width, &height);

    DrawSummaryHitMarksMapped(rp, targetX, targetY, hitMap, width, height,
                              (const SummaryPoint *)gSummaryMap300ToScoringEType);
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

static BOOL InitSummaryBackBuffer(struct BitMap *bm, struct RastPort *rp, UWORD width, UWORD height,
                                  UWORD depth) {
    UWORD p;

    if (!bm || !rp || width == 0 || height == 0 || depth == 0) {
        return FALSE;
    }

    memset(bm, 0, sizeof(*bm));
    InitBitMap(bm, depth, width, height);

    for (p = 0; p < depth; p++) {
        bm->Planes[p] = AllocRaster(width, height);
        if (!bm->Planes[p]) {
            UWORD q;
            for (q = 0; q < p; q++) {
                if (bm->Planes[q]) {
                    FreeRaster(bm->Planes[q], width, height);
                    bm->Planes[q] = NULL;
                }
            }
            return FALSE;
        }
    }

    InitRastPort(rp);
    rp->BitMap = bm;
    SetRast(rp, 0);
    WaitBlit();

    return TRUE;
}

static void FreeSummaryBackBuffer(struct BitMap *bm, UWORD width, UWORD height) {
    UWORD p;

    if (!bm) {
        return;
    }

    for (p = 0; p < bm->Depth; p++) {
        if (bm->Planes[p]) {
            FreeRaster(bm->Planes[p], width, height);
            bm->Planes[p] = NULL;
        }
    }
}

static BOOL ShowSummaryScreen(const RangeSummaryData *summary) {
    typedef struct SummaryStep {
        const char *distanceText;
        const AmacsBob *bob;
        WORD x;
        WORD y;
    } SummaryStep;

    struct RastPort *screenRP;
    struct Screen *scr;
    struct TextFont *font = NULL;
    struct BitMap summaryBM;
    struct RastPort summaryRP;
    UWORD summaryDepth = 0;
    BOOL summaryBufferReady = FALSE;
    AmacsBob scoringETypeBob;
    AmacsBob scoringFTypeBob;
    BOOL scoringETypeLoaded = FALSE;
    BOOL scoringFTypeLoaded = FALSE;
    BOOL summaryVisible = FALSE;
    SummaryStep steps[6];
    UWORD stepIndex = 0;
    char line[32];
    UWORD score;
    UWORD acc;
    UWORD timeBonus;
    UWORD totalScore;
    UWORD hits050;
    UWORD hits100;
    UWORD hits150;
    UWORD hits200;
    UWORD hits250;
    UWORD hits300;
    UWORD hitsTotal;
    WORD tableY;
    WaitResult waitResult;

    memset(&summaryBM, 0, sizeof(summaryBM));
    memset(&summaryRP, 0, sizeof(summaryRP));
    memset(&scoringETypeBob, 0, sizeof(scoringETypeBob));
    memset(&scoringFTypeBob, 0, sizeof(scoringFTypeBob));

    Gfx_FadeOutCurrentScreenToBlack(rangePalette, 32);

    scr = Gfx_GetScreen();

    if (!scr || !scr->RastPort.BitMap) {
        return FALSE;
    }

    screenRP = &scr->RastPort;
    summaryDepth = (UWORD)scr->RastPort.BitMap->Depth;

    /*
     * Summary is static between key presses, so avoid system ScreenBuffer DBuf here.
     * Build each full Summary page in a private bitmap and copy it to the visible
     * screen in one blit. This keeps page changes flicker-free without touching
     * ChangeScreenBuffer/SafeMessage, which is risky at end-of-round transitions.
     */
    if (!InitSummaryBackBuffer(&summaryBM, &summaryRP, LO_WIDTH, LO_HEIGHT, summaryDepth)) {
        return FALSE;
    }
    summaryBufferReady = TRUE;

    if (Bob_LoadRawAndMask(&scoringFTypeBob, SUMMARY_SCORING_FTYPE_RAW, SUMMARY_SCORING_FTYPE_MASK,
                           SUMMARY_SCORING_FTYPE_W, SUMMARY_SCORING_FTYPE_H, 5)) {
        scoringFTypeLoaded = TRUE;
    }

    if (Bob_LoadRawAndMask(&scoringETypeBob, SUMMARY_SCORING_ETYPE_RAW, SUMMARY_SCORING_ETYPE_MASK,
                           SUMMARY_SCORING_ETYPE_W, SUMMARY_SCORING_ETYPE_H, 5)) {
        scoringETypeLoaded = TRUE;
    }

    font = OpenFont(&gSummaryFontAttr);

    score = summary ? summary->score : 0;
    acc = summary ? summary->accuracy : 0;
    timeBonus = summary ? summary->timeBonus : 0;
    totalScore = (UWORD)(score + timeBonus);

    hits050 = GetSummaryHitCountMain(50);
    hits100 = GetSummaryHitCountMain(100);
    hits150 = GetSummaryHitCountMain(150);
    hits200 = GetSummaryHitCountMain(200);
    hits250 = GetSummaryHitCountMain(250);
    hits300 = GetSummaryHitCountMain(300);
    hitsTotal = (UWORD)(hits050 + hits100 + hits150 + hits200 + hits250 + hits300);

    steps[0].distanceText = "50 Meter";
    steps[0].bob = scoringFTypeLoaded ? &scoringFTypeBob : NULL;
    steps[0].x = SUMMARY_SCORING_FTYPE_X;
    steps[0].y = SUMMARY_SCORING_FTYPE_Y;

    steps[1].distanceText = "100 Meter";
    steps[1].bob = scoringFTypeLoaded ? &scoringFTypeBob : NULL;
    steps[1].x = SUMMARY_SCORING_FTYPE_X;
    steps[1].y = SUMMARY_SCORING_FTYPE_Y;

    steps[2].distanceText = "150 Meter";
    steps[2].bob = scoringETypeLoaded ? &scoringETypeBob : NULL;
    steps[2].x = SUMMARY_SCORING_ETYPE_X;
    steps[2].y = SUMMARY_SCORING_ETYPE_Y;

    steps[3].distanceText = "200 Meter";
    steps[3].bob = scoringETypeLoaded ? &scoringETypeBob : NULL;
    steps[3].x = SUMMARY_SCORING_ETYPE_X;
    steps[3].y = SUMMARY_SCORING_ETYPE_Y;

    steps[4].distanceText = "250 Meter";
    steps[4].bob = scoringETypeLoaded ? &scoringETypeBob : NULL;
    steps[4].x = SUMMARY_SCORING_ETYPE_X;
    steps[4].y = SUMMARY_SCORING_ETYPE_Y;

    steps[5].distanceText = "300 Meter";
    steps[5].bob = scoringETypeLoaded ? &scoringETypeBob : NULL;
    steps[5].x = SUMMARY_SCORING_ETYPE_X;
    steps[5].y = SUMMARY_SCORING_ETYPE_Y;

    for (;;) {
        SetRast(&summaryRP, SUMMARY_BACKGROUND_PEN);
        WaitBlit();

        if (stepIndex < 6) {
            if (steps[stepIndex].bob) {
                Bob_DrawMaskedToRastPort(steps[stepIndex].bob, &summaryRP, steps[stepIndex].x,
                                         steps[stepIndex].y);
            }

            if (stepIndex == 0 && steps[stepIndex].bob) {
                DrawSummaryHitMarks050(&summaryRP, steps[stepIndex].x, steps[stepIndex].y);
            }

            if (stepIndex == 1 && steps[stepIndex].bob) {
                DrawSummaryHitMarks100(&summaryRP, steps[stepIndex].x, steps[stepIndex].y);
            }

            if (stepIndex == 2 && steps[stepIndex].bob) {
                DrawSummaryHitMarks150(&summaryRP, steps[stepIndex].x, steps[stepIndex].y);
            }

            if (stepIndex == 3 && steps[stepIndex].bob) {
                DrawSummaryHitMarks200(&summaryRP, steps[stepIndex].x, steps[stepIndex].y);
            }

            if (stepIndex == 4 && steps[stepIndex].bob) {
                DrawSummaryHitMarks250(&summaryRP, steps[stepIndex].x, steps[stepIndex].y);
            }

            if (stepIndex == 5 && steps[stepIndex].bob) {
                DrawSummaryHitMarks300(&summaryRP, steps[stepIndex].x, steps[stepIndex].y);
            }

            DrawCenteredTextWithShadowMain(&summaryRP, font, SUMMARY_DISTANCE_Y,
                                           SUMMARY_DISTANCE_PEN, SUMMARY_SHADOW_PEN,
                                           steps[stepIndex].distanceText);
        } else {
            DrawCenteredTextWithShadowMain(&summaryRP, font, SUMMARY_TITLE_Y, 31, 24, "SUMMARY");
            DrawRightAlignedTextWithShadowMain(&summaryRP, font, SUMMARY_TABLE_HITS_X,
                                               SUMMARY_TABLE_HEADER_Y, 31, 24, "HITS");

            tableY = SUMMARY_TABLE_FIRST_ROW_Y;
            DrawTextWithShadowExMain(&summaryRP, font, SUMMARY_TABLE_LABEL_X, tableY, 31, 24,
                                     " 50 M", 5);
            line[0] = '\0';
            AppendUnsignedMain(line, 0, hits050);
            DrawRightAlignedTextWithShadowMain(&summaryRP, font, SUMMARY_TABLE_HITS_X, tableY, 31,
                                               24, line);

            tableY = (WORD)(tableY + SUMMARY_TABLE_ROW_STEP_Y);
            DrawTextWithShadowExMain(&summaryRP, font, SUMMARY_TABLE_LABEL_X, tableY, 31, 24,
                                     "100 M", 5);
            line[0] = '\0';
            AppendUnsignedMain(line, 0, hits100);
            DrawRightAlignedTextWithShadowMain(&summaryRP, font, SUMMARY_TABLE_HITS_X, tableY, 31,
                                               24, line);

            tableY = (WORD)(tableY + SUMMARY_TABLE_ROW_STEP_Y);
            DrawTextWithShadowExMain(&summaryRP, font, SUMMARY_TABLE_LABEL_X, tableY, 31, 24,
                                     "150 M", 5);
            line[0] = '\0';
            AppendUnsignedMain(line, 0, hits150);
            DrawRightAlignedTextWithShadowMain(&summaryRP, font, SUMMARY_TABLE_HITS_X, tableY, 31,
                                               24, line);

            tableY = (WORD)(tableY + SUMMARY_TABLE_ROW_STEP_Y);
            DrawTextWithShadowExMain(&summaryRP, font, SUMMARY_TABLE_LABEL_X, tableY, 31, 24,
                                     "200 M", 5);
            line[0] = '\0';
            AppendUnsignedMain(line, 0, hits200);
            DrawRightAlignedTextWithShadowMain(&summaryRP, font, SUMMARY_TABLE_HITS_X, tableY, 31,
                                               24, line);

            tableY = (WORD)(tableY + SUMMARY_TABLE_ROW_STEP_Y);
            DrawTextWithShadowExMain(&summaryRP, font, SUMMARY_TABLE_LABEL_X, tableY, 31, 24,
                                     "250 M", 5);
            line[0] = '\0';
            AppendUnsignedMain(line, 0, hits250);
            DrawRightAlignedTextWithShadowMain(&summaryRP, font, SUMMARY_TABLE_HITS_X, tableY, 31,
                                               24, line);

            tableY = (WORD)(tableY + SUMMARY_TABLE_ROW_STEP_Y);
            DrawTextWithShadowExMain(&summaryRP, font, SUMMARY_TABLE_LABEL_X, tableY, 31, 24,
                                     "300 M", 5);
            line[0] = '\0';
            AppendUnsignedMain(line, 0, hits300);
            DrawRightAlignedTextWithShadowMain(&summaryRP, font, SUMMARY_TABLE_HITS_X, tableY, 31,
                                               24, line);

            tableY = (WORD)(tableY + SUMMARY_TABLE_ROW_STEP_Y + 3);
            DrawTextWithShadowExMain(&summaryRP, font, SUMMARY_TABLE_LABEL_X, tableY,
                                     SUMMARY_DISTANCE_PEN, SUMMARY_SHADOW_PEN, "TOTALS", 6);
            line[0] = '\0';
            AppendUnsignedMain(line, 0, hitsTotal);
            DrawRightAlignedTextWithShadowMain(&summaryRP, font, SUMMARY_TABLE_HITS_X, tableY,
                                               SUMMARY_DISTANCE_PEN, SUMMARY_SHADOW_PEN, line);

            BuildSummaryScoreLine(line, score);
            DrawTextWithShadowExMain(&summaryRP, font, SUMMARY_SCORE_LEFT_X, SUMMARY_HIT_SCORE_Y,
                                     31, 24, line, (UWORD)strlen(line));

            BuildSummaryTimeBonusLine(line, timeBonus);
            DrawTextWithShadowExMain(&summaryRP, font, SUMMARY_SCORE_LEFT_X, SUMMARY_TIME_BONUS_Y,
                                     31, 24, line, (UWORD)strlen(line));

            BuildSummaryTotalScoreLine(line, totalScore);
            DrawTextWithShadowExMain(&summaryRP, font, SUMMARY_SCORE_LEFT_X, SUMMARY_TOTAL_SCORE_Y,
                                     SUMMARY_DISTANCE_PEN, SUMMARY_SHADOW_PEN, line,
                                     (UWORD)strlen(line));

            BuildSummaryAccuracyLine(line, acc);
            DrawTextWithShadowExMain(&summaryRP, font, SUMMARY_SCORE_RIGHT_X, SUMMARY_ACCURACY_Y,
                                     31, 24, line, (UWORD)strlen(line));

            BuildSummaryRankLine(line, acc);
            DrawTextWithShadowExMain(&summaryRP, font, SUMMARY_SCORE_RIGHT_X, SUMMARY_RANK_Y, 31,
                                     24, line, (UWORD)strlen(line));
        }

        DrawCenteredTextWithShadowMain(&summaryRP, font, PULL_TRIGGER_POSITION_Y,
                                       SUMMARY_DISTANCE_PEN, SUMMARY_SHADOW_PEN,
                                       "PULL TRIGGER TO CONTINUE");

        WaitBlit();
        WaitTOF();
        BltBitMap(&summaryBM, 0, 0, screenRP->BitMap, 0, 0, LO_WIDTH, LO_HEIGHT, 0xC0, 0xFF, NULL);
        WaitBlit();

        if (!summaryVisible) {
            Gfx_FadeInCurrentScreenFromBlack(SummaryPaletteRGB4, 32);
            summaryVisible = TRUE;
        }

        waitResult = WaitForAdvanceOnly();
        if (waitResult == WAIT_ESC) {
            if (font)
                CloseFont(font);
            if (scoringFTypeLoaded)
                Bob_Free(&scoringFTypeBob);
            if (scoringETypeLoaded)
                Bob_Free(&scoringETypeBob);
            if (summaryBufferReady)
                FreeSummaryBackBuffer(&summaryBM, LO_WIDTH, LO_HEIGHT);
            return FALSE;
        }

        if (stepIndex >= 6) {
            break;
        }

        stepIndex++;
    }

    if (font)
        CloseFont(font);

    if (scoringFTypeLoaded) {
        Bob_Free(&scoringFTypeBob);
    }

    if (scoringETypeLoaded) {
        Bob_Free(&scoringETypeBob);
    }

    if (summaryBufferReady) {
        FreeSummaryBackBuffer(&summaryBM, LO_WIDTH, LO_HEIGHT);
    }

    if (!ShowTitleScorePlaceholderScreen()) {
        return FALSE;
    }

    return TRUE;
}

static BOOL ShowTitleScorePlaceholderScreen(void) {
    struct Screen *scr;
    struct RastPort *rp;
    struct TextFont *font = NULL;
    WaitResult waitResult;

    /*
     * Fade to black first and compose the whole hi-score placeholder while the
     * palette is black. This avoids a visible flash of the plain title screen
     * before the rectangle and labels are drawn.
     */
    Gfx_FadeOutCurrentScreenToBlack(SummaryPaletteRGB4, 32);

    scr = Gfx_GetScreen();
    if (!scr || !scr->RastPort.BitMap) {
        return FALSE;
    }

    rp = &scr->RastPort;

    if (!LoadRawImageToRastPort(TITLE_FILE, rp, LO_WIDTH, LO_HEIGHT)) {
        return FALSE;
    }

    /* Hi-score placeholder area: 280x208 px, top-left at (20,24). */
    SetAPen(rp, 10);
    RectFill(rp, 20, 24, 299, 231);

    font = OpenFont(&gSummaryFontAttr);

    if (font) {
        DrawCenteredTextWithShadowMain(rp, font, HISCORE_TITLE_Y, HISCORE_TEXT_PEN,
                                       HISCORE_SHADOW_PEN, "HIGH SCORES");
        DrawHiScoreEntries(rp, font);
        DrawCenteredTextWithShadowMain(rp, font, PULL_TRIGGER_POSITION_Y, HISCORE_TEXT_PEN,
                                       HISCORE_SHADOW_PEN, "PULL TRIGGER TO CONTINUE");
        CloseFont(font);
    }

    WaitBlit();
    Gfx_FadeInCurrentScreenFromBlack(titlePalette, 32);

    /* Require a fresh press on the placeholder screen. */
    while (IsJoystickFirePressed()) {
        WaitTOF();
    }
    DrainWindowMessages();

    waitResult = WaitForAdvanceNoTimeout();
    if (waitResult == WAIT_ESC) {
        return FALSE;
    }

    if (!LoadRawImageToRastPort(TITLE_FILE, rp, LO_WIDTH, LO_HEIGHT)) {
        return FALSE;
    }

    WaitBlit();
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
