#ifndef TARGET_SCORING_H
#define TARGET_SCORING_H

#include <exec/types.h>

/*
 * Score values:
 * 0 = Miss
 * 1 = Poor
 * 2 = Below Avg
 * 3 = Average
 * 4 = Good
 * 5 = Excellent
 */

#define SCORE_MISS 0
#define SCORE_POOR 1
#define SCORE_BELOW_AVG 2
#define SCORE_AVERAGE 3
#define SCORE_GOOD 4
#define SCORE_EXCELLENT 5

/*
 * Returns score value (0–5) based on:
 * - target distance
 * - local hit coordinates within target bitmap
 */
UBYTE TargetScoring_GetScore(UWORD distance, WORD localX, WORD localY);

#endif /* TARGET_SCORING_H */