#include "levelManager.h"
#include "rangeHandler.h"

#include <intuition/intuition.h>

BOOL IS_NEW_GAME_SESSION = TRUE;

typedef struct LevelManagerState {
    LevelManagerMode mode;
} LevelManagerState;

static LevelManagerState gLevelManager = {LEVEL_MANAGER_MODE_DEMO};

static const LevelConfig gDemoConfig = {10, 10, 5, 2, FALSE, TRUE, "TEN SHOT CHALLENGE"};
static const LevelConfig gLevel1Config = {18, 12, 30, 1, TRUE, TRUE, "LEVEL 1"};
static const LevelConfig gLevel2Config = {18, 12, 30, 1, TRUE, TRUE, "LEVEL 2"};
static const LevelConfig gLevel3Config = {24, 18, 30, 1, TRUE, FALSE, "LEVEL 3"};
/* One 30-round magazine, as Levels 1-3. */
static const LevelConfig gLevel4Config = {24, 16, 30, 1, TRUE, FALSE, "LEVEL 4"};
static const LevelConfig gLevel5Config = {40, 30, 20, 1, FALSE, FALSE, "LEVEL 5"};

const LevelConfig *LevelManager_GetConfig(void) {
    return (gLevelManager.mode == LEVEL_MANAGER_MODE_LEVEL5) ? &gLevel5Config :
           (gLevelManager.mode == LEVEL_MANAGER_MODE_LEVEL4) ? &gLevel4Config :
           (gLevelManager.mode == LEVEL_MANAGER_MODE_LEVEL3) ? &gLevel3Config :
           (gLevelManager.mode == LEVEL_MANAGER_MODE_LEVEL2) ? &gLevel2Config :
           (gLevelManager.mode == LEVEL_MANAGER_MODE_LEVEL1) ? &gLevel1Config : &gDemoConfig;
}


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
        case LEVEL_MANAGER_MODE_LEVEL2:
        case LEVEL_MANAGER_MODE_LEVEL3:
        case LEVEL_MANAGER_MODE_LEVEL4:
        case LEVEL_MANAGER_MODE_LEVEL5:
        case LEVEL_MANAGER_MODE_DEMO:
        default:
            return RunRangeWithFrontSight(useDBuf, outSummary);
    }

    return FALSE;
}
