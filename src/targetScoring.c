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

typedef struct {
    UWORD distance;
    BYTE offsetY;
} ZeroOffset;

/* BZO 300 m (Default, M16A2) */
static const ZeroOffset gBZO300[] = {{50, +1},  {100, +2}, {150, +3},
                                     {200, +4}, {250, +1}, {300, 0}};

/* BZO 250 m (To be optional, M16A1) */
static const ZeroOffset gBZO250[] = {{50, +1}, {100, +1}, {150, +1}, {200, 0}, {250, 0}, {300, -3}};

#define ZERO_OFFSET_COUNT(table) (sizeof(table) / sizeof((table)[0]))
#define ACTIVE_BZO_METERS 300

static const ZeroOffset *GetActiveZeroTable(UWORD *outCount) {
    switch (ACTIVE_BZO_METERS) {
        case 250:
            *outCount = (UWORD)ZERO_OFFSET_COUNT(gBZO250);
            return gBZO250;

        case 300:
        default:
            *outCount = (UWORD)ZERO_OFFSET_COUNT(gBZO300);
            return gBZO300;
    }
}

static BYTE GetZeroOffset(const ZeroOffset *table, UWORD count, UWORD distance) {
    UWORD i;

    for (i = 0; i < count; i++) {
        if (table[i].distance == distance) {
            return table[i].offsetY;
        }
    }

    return 0;
}

BYTE TargetScoring_GetZeroOffset(UWORD distance) {
    UWORD count;
    const ZeroOffset *table = GetActiveZeroTable(&count);

    return GetZeroOffset(table, count, distance);
}

static UBYTE GetScoreFromMap(const UBYTE *map, UWORD width, WORD x, WORD y) {
    return map[y * width + x];
}

static UWORD gTotalScore = 0;
static UWORD gScore050 = 0;
static UWORD gScore100 = 0;
static UWORD gScore150 = 0;
static UWORD gScore200 = 0;
static UWORD gScore250 = 0;
static UWORD gScore300 = 0;

static void ClearHitMap(UWORD *map, UWORD width, UWORD height) {
    UWORD i;
    UWORD total = (UWORD)(width * height);

    for (i = 0; i < total; i++) {
        map[i] = 0;
    }
}

static void AddScoreSaturated(UWORD *value, UBYTE score) {
    if (!value) {
        return;
    }

    if (*value <= (65535 - score)) {
        *value = (UWORD)(*value + score);
    } else {
        *value = 65535;
    }
}

static UBYTE RegisterScore(UWORD *hitMap, UWORD width, WORD localX, WORD localY,
                           const UBYTE *scoreMap, UWORD *distanceScore) {
    UBYTE score = GetScoreFromMap(scoreMap, width, localX, localY);
    hitMap[(localY * width) + localX]++;

    AddScoreSaturated(&gTotalScore, score);
    AddScoreSaturated(distanceScore, score);

    return score;
}

UBYTE TargetScoring_GetScore(UWORD distance, WORD localX, WORD localY) {
    switch (distance) {
        case 300:
            if (localX < 0 || localX >= T300_W || localY < 0 || localY >= T300_H) {
                return SCORE_MISS;
            }
            return RegisterScore((UWORD *)gHitMap300, T300_W, localX, localY,
                                 (const UBYTE *)gScoreMap300, &gScore300);

        case 250:
            if (localX < 0 || localX >= T250_W || localY < 0 || localY >= T250_H) {
                return SCORE_MISS;
            }
            return RegisterScore((UWORD *)gHitMap250, T250_W, localX, localY,
                                 (const UBYTE *)gScoreMap250, &gScore250);

        case 200:
            if (localX < 0 || localX >= T200_W || localY < 0 || localY >= T200_H) {
                return SCORE_MISS;
            }
            return RegisterScore((UWORD *)gHitMap200, T200_W, localX, localY,
                                 (const UBYTE *)gScoreMap200, &gScore200);

        case 150:
            if (localX < 0 || localX >= T150_W || localY < 0 || localY >= T150_H) {
                return SCORE_MISS;
            }
            return RegisterScore((UWORD *)gHitMap150, T150_W, localX, localY,
                                 (const UBYTE *)gScoreMap150, &gScore150);

        case 100:
            if (localX < 0 || localX >= T100_W || localY < 0 || localY >= T100_H) {
                return SCORE_MISS;
            }
            return RegisterScore((UWORD *)gHitMap100, T100_W, localX, localY,
                                 (const UBYTE *)gScoreMap100, &gScore100);

        case 50:
            if (localX < 0 || localX >= T050_W || localY < 0 || localY >= T050_H) {
                return SCORE_MISS;
            }
            return RegisterScore((UWORD *)gHitMap050, T050_W, localX, localY,
                                 (const UBYTE *)gScoreMap050, &gScore050);

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
    gScore050 = 0;
    gScore100 = 0;
    gScore150 = 0;
    gScore200 = 0;
    gScore250 = 0;
    gScore300 = 0;
}

UWORD TargetScoring_GetTotalScore(void) {
    return gTotalScore;
}

UWORD TargetScoring_GetPerformance(UWORD distance) {
    switch (distance) {
        case 50:
            return gScore050;
        case 100:
            return gScore100;
        case 150:
            return gScore150;
        case 200:
            return gScore200;
        case 250:
            return gScore250;
        case 300:
            return gScore300;
        default:
            break;
    }

    return 0;
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
