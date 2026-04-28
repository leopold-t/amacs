#include "targetScoring.h"
#include "targetsHandler.h"

static UWORD gHitMap050[T050_H][T050_W];
static UWORD gHitMap100[T100_H][T100_W];
static UWORD gHitMap150[T150_H][T150_W];
static UWORD gHitMap200[T200_H][T200_W];
static UWORD gHitMap250[T250_H][T250_W];
static UWORD gHitMap300[T300_H][T300_W];

/*
 * =====================================================================
 * SCORE MAPS
 * Each map uses local target coordinates:
 * [row][column] = [y][x]
 * =====================================================================
 */
static const UBYTE gScoreMap300[T300_H][T300_W] = {
    {0, 1, 1, 1, 0}, {0, 1, 1, 1, 0}, {1, 1, 1, 1, 1}, {1, 2, 2, 2, 1}, {2, 3, 4, 3, 2},
    {2, 4, 5, 4, 2}, {2, 3, 4, 3, 2}, {1, 2, 2, 2, 1}, {1, 1, 1, 1, 1}, {1, 1, 1, 1, 1}};

static const UBYTE gScoreMap250[T250_H][T250_W] = {
    {0, 0, 1, 1, 1, 0, 0}, {0, 0, 1, 1, 1, 0, 0}, {0, 0, 1, 1, 1, 0, 0}, {0, 1, 1, 1, 1, 1, 0},
    {1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1}, {1, 1, 2, 2, 2, 1, 1}, {1, 2, 3, 4, 3, 2, 1},
    {2, 3, 4, 5, 4, 3, 2}, {1, 2, 3, 4, 3, 2, 1}, {1, 1, 2, 2, 2, 1, 1}, {1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1}};

static const UBYTE gScoreMap200[T200_H][T200_W] = {
    {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0},
    {0, 1, 1, 1, 1, 1, 1, 0}, {1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 2, 2, 1, 1, 1},
    {1, 1, 2, 3, 3, 2, 1, 1}, {1, 2, 3, 4, 4, 3, 2, 1}, {1, 2, 3, 5, 5, 3, 2, 1},
    {1, 2, 3, 4, 4, 3, 2, 1}, {1, 1, 2, 3, 3, 2, 1, 1}, {1, 1, 1, 2, 2, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1}};

static const UBYTE gScoreMap150[T150_H][T150_W] = {
    {0, 0, 0, 1, 1, 1, 0, 0, 0}, {0, 0, 1, 1, 1, 1, 1, 0, 0}, {0, 0, 1, 1, 1, 1, 1, 0, 0},
    {0, 0, 1, 1, 1, 1, 1, 0, 0}, {0, 1, 1, 1, 1, 1, 1, 1, 0}, {1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 2, 2, 2, 1, 1, 1}, {1, 1, 2, 3, 4, 3, 2, 1, 1},
    {1, 1, 2, 4, 5, 4, 2, 1, 1}, {1, 1, 2, 3, 4, 3, 2, 1, 1}, {1, 1, 1, 2, 2, 2, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1}};

static const UBYTE gScoreMap100[T100_H][T100_W] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 3, 4, 4, 3, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 3, 5, 5, 3, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0},
    {0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 3, 4, 4, 3, 2, 1, 1, 1, 1, 1, 1, 1, 0, 0},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

static const UBYTE gScoreMap050[T050_H][T050_W] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
     1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2,
     2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
     2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 3, 3, 3,
     3, 3, 3, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 3, 3, 4, 4,
     4, 4, 3, 3, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 3, 4, 4, 5,
     5, 4, 4, 3, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 3, 4, 4, 5,
     5, 4, 4, 3, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 3, 3, 4, 4,
     4, 4, 3, 3, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 3, 3, 3,
     3, 3, 3, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
     2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2,
     2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

static UBYTE GetScoreFromMap(const UBYTE *map, UWORD width, WORD x, WORD y) {
    return map[y * width + x];
}

static UWORD gTotalScore = 0;

static void ClearHitMap(UWORD *map, UWORD width, UWORD height) {
    UWORD i;
    UWORD total = (UWORD)(width * height);

    for (i = 0; i < total; i++) {
        map[i] = 0;
    }
}

static UBYTE RegisterScore(UWORD *hitMap, UWORD width, WORD localX, WORD localY,
                           const UBYTE *scoreMap) {
    UBYTE score = GetScoreFromMap(scoreMap, width, localX, localY);
    hitMap[(localY * width) + localX]++;

    if (gTotalScore <= (65535 - score)) {
        gTotalScore = (UWORD)(gTotalScore + score);
    } else {
        gTotalScore = 65535;
    }

    return score;
}

UBYTE TargetScoring_GetScore(UWORD distance, WORD localX, WORD localY) {
    switch (distance) {
        case 300:
            return RegisterScore((UWORD *)gHitMap300, T300_W, localX, localY,
                                 (const UBYTE *)gScoreMap300);

        case 250:
            return RegisterScore((UWORD *)gHitMap250, T250_W, localX, localY,
                                 (const UBYTE *)gScoreMap250);

        case 200:
            return RegisterScore((UWORD *)gHitMap200, T200_W, localX, localY,
                                 (const UBYTE *)gScoreMap200);

        case 150:
            return RegisterScore((UWORD *)gHitMap150, T150_W, localX, localY,
                                 (const UBYTE *)gScoreMap150);

        case 100:
            return RegisterScore((UWORD *)gHitMap100, T100_W, localX, localY,
                                 (const UBYTE *)gScoreMap100);

        case 50:
            return RegisterScore((UWORD *)gHitMap050, T050_W, localX, localY,
                                 (const UBYTE *)gScoreMap050);

        default:
            break;
    }

    return SCORE_MISS;
}

void TargetScoring_Reset(void) {
    ClearHitMap((UWORD *)gHitMap050, T050_W, T050_H);
    ClearHitMap((UWORD *)gHitMap100, T100_W, T100_H);
    ClearHitMap((UWORD *)gHitMap150, T150_W, T150_H);
    ClearHitMap((UWORD *)gHitMap200, T200_W, T200_H);
    ClearHitMap((UWORD *)gHitMap250, T250_W, T250_H);
    ClearHitMap((UWORD *)gHitMap300, T300_W, T300_H);
    gTotalScore = 0;
}

UWORD TargetScoring_GetTotalScore(void) {
    return gTotalScore;
}

const UWORD *TargetScoring_GetHitMap050(UWORD *outWidth, UWORD *outHeight) {
    if (outWidth) {
        *outWidth = T050_W;
    }

    if (outHeight) {
        *outHeight = T050_H;
    }

    return (const UWORD *)gHitMap050;
}


const UWORD *TargetScoring_GetHitMap100(UWORD *outWidth, UWORD *outHeight) {
    if (outWidth) {
        *outWidth = T100_W;
    }

    if (outHeight) {
        *outHeight = T100_H;
    }

    return (const UWORD *)gHitMap100;
}


const UWORD *TargetScoring_GetHitMap150(UWORD *outWidth, UWORD *outHeight) {
    if (outWidth) {
        *outWidth = T150_W;
    }

    if (outHeight) {
        *outHeight = T150_H;
    }

    return (const UWORD *)gHitMap150;
}


const UWORD *TargetScoring_GetHitMap200(UWORD *outWidth, UWORD *outHeight) {
    if (outWidth) {
        *outWidth = T200_W;
    }

    if (outHeight) {
        *outHeight = T200_H;
    }

    return (const UWORD *)gHitMap200;
}


const UWORD *TargetScoring_GetHitMap250(UWORD *outWidth, UWORD *outHeight) {
    if (outWidth) {
        *outWidth = T250_W;
    }

    if (outHeight) {
        *outHeight = T250_H;
    }

    return (const UWORD *)gHitMap250;
}


const UWORD *TargetScoring_GetHitMap300(UWORD *outWidth, UWORD *outHeight) {
    if (outWidth) {
        *outWidth = T300_W;
    }

    if (outHeight) {
        *outHeight = T300_H;
    }

    return (const UWORD *)gHitMap300;
}
