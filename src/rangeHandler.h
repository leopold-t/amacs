#ifndef RANGEHANDLER_H
#define RANGEHANDLER_H

#include <exec/types.h>

typedef enum RangeControlMode {
    RANGE_CONTROL_JOYSTICK = 0,
    RANGE_CONTROL_KEYBOARD,
    RANGE_CONTROL_MOUSE,
    RANGE_CONTROL_LINKED,
    RANGE_CONTROL_GYRO /* Air/gyro mouse: direct movement, no simulated sway or breathing */
} RangeControlMode;

RangeControlMode Range_GetPrimaryControl(void);
void Range_SetPrimaryControl(RangeControlMode mode);
typedef enum RangePositionMode {
    RANGE_POSITION_AS_TRAINED = 0,
    RANGE_POSITION_SUPPORTED,
    RANGE_POSITION_UNSUPPORTED
} RangePositionMode;
RangePositionMode Range_GetPositionMode(void);
void Range_SetPositionMode(RangePositionMode mode);
const char *Range_PositionModeName(RangePositionMode mode);
BOOL Range_GetSupportedPosition(void);
void Range_SetSupportedPosition(BOOL supported);
UWORD Range_GetMouseSensitivity(void);
void Range_SetMouseSensitivity(UWORD percent);
/* GYRO MOUSE has its own, wider gain range (25-400 %, step 25, default 200 %). */
#define RANGE_GYRO_SENSITIVITY_MIN 25
#define RANGE_GYRO_SENSITIVITY_MAX 400
#define RANGE_GYRO_SENSITIVITY_STEP 25
UWORD Range_GetGyroSensitivity(void);
void Range_SetGyroSensitivity(UWORD percent);

typedef enum RangeWindDirection {
    RANGE_WIND_FULL_LEFT = 0,
    RANGE_WIND_HALF_LEFT,
    RANGE_WIND_FULL_RIGHT,
    RANGE_WIND_HALF_RIGHT
} RangeWindDirection;

UWORD Range_GetWindSpeed(void);
void Range_SetWindSpeed(UWORD mph);
RangeWindDirection Range_GetWindDirection(void);
void Range_SetWindDirection(RangeWindDirection direction);

typedef struct RangeSummaryData {
    UWORD score;
    UWORD accuracy;
    UWORD totalTime;
    UWORD timeBonus;
    UWORD penalties; /* Tactical target-priority errors (none in Level 1). */
    BOOL summaryLastShotHit;
    UBYTE summaryLastShotScore;
    UBYTE debugJumpLevel; /* Test-only direct range jump, 0 if unused. */
    BOOL abortedToTitle;
    BOOL levelQualified;
    BOOL forceLevelPass; /* Testing shortcut: bypass qualification, never add points. */
} RangeSummaryData;

/* Runs the range loop with a movable front sight.
 * useDBuf:
 *  - TRUE  => draw to back buffer via Gfx_GetDrawRastPort() and swap
 *  - FALSE => draw directly to screen RastPort (no DBuf)
 */
BOOL RunRangeWithFrontSight(BOOL useDBuf, RangeSummaryData *outSummary);

#endif /* RANGEHANDLER_H */