#include "levelManager.h"
#include "rangeHandler.h"

#include <intuition/intuition.h>

BOOL IS_NEW_GAME_SESSION = TRUE;

typedef struct LevelManagerState {
    LevelManagerMode mode;
} LevelManagerState;

static LevelManagerState gLevelManager = {LEVEL_MANAGER_MODE_DEMO};

void LevelManager_Init(void) {
    gLevelManager.mode = LEVEL_MANAGER_MODE_DEMO;
}

void LevelManager_Shutdown(void) {}

void LevelManager_SetMode(LevelManagerMode mode) {
    gLevelManager.mode = mode;
}

LevelManagerMode LevelManager_GetMode(void) {
    return gLevelManager.mode;
}

BOOL LevelManager_RunCurrent(BOOL useDBuf, RangeSummaryData *outSummary) {
    switch (gLevelManager.mode) {
        case LEVEL_MANAGER_MODE_LEVEL1:
        case LEVEL_MANAGER_MODE_DEMO:
        default:
            return RunRangeWithFrontSight(useDBuf, outSummary);
    }

    return FALSE;
}
