#ifndef RANGEHANDLER_H
#define RANGEHANDLER_H

#include <exec/types.h>

typedef struct RangeSummaryData {
    UWORD score;
    UWORD accuracy;
    BOOL summaryLastShotHit;
    UBYTE summaryLastShotScore;
} RangeSummaryData;

/* Runs the range loop with a movable front sight.
 * useDBuf:
 *  - TRUE  => draw to back buffer via Gfx_GetDrawRastPort() and swap
 *  - FALSE => draw directly to screen RastPort (no DBuf)
 */
BOOL RunRangeWithFrontSight(BOOL useDBuf, RangeSummaryData *outSummary);

#endif /* RANGEHANDLER_H */