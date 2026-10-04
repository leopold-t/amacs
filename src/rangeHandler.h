#ifndef RANGEHANDLER_H
#define RANGEHANDLER_H

#include <exec/types.h>

typedef enum RangeControlMode {
    RANGE_CONTROL_JOYSTICK = 0,
    RANGE_CONTROL_KEYBOARD,
    RANGE_CONTROL_MOUSE
} RangeControlMode;

RangeControlMode Range_GetPrimaryControl(void);
void Range_SetPrimaryControl(RangeControlMode mode);

typedef struct RangeSummaryData {
    UWORD score;
    UWORD accuracy;
    UWORD totalTime;
    UWORD timeBonus;
    BOOL summaryLastShotHit;
    UBYTE summaryLastShotScore;
    BOOL abortedToTitle;
} RangeSummaryData;

/* Runs the range loop with a movable front sight.
 * useDBuf:
 *  - TRUE  => draw to back buffer via Gfx_GetDrawRastPort() and swap
 *  - FALSE => draw directly to screen RastPort (no DBuf)
 */
BOOL RunRangeWithFrontSight(BOOL useDBuf, RangeSummaryData *outSummary);

#endif /* RANGEHANDLER_H */