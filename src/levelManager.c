#include "levelManager.h"

BOOL IS_NEW_GAME_SESSION = TRUE;


#include "rangeHandler.h"

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

BOOL LevelManager_RunCurrent(BOOL useDBuf) {
    switch (gLevelManager.mode) {
        case LEVEL_MANAGER_MODE_LEVEL1:
        case LEVEL_MANAGER_MODE_DEMO:
        default:
            return RunRangeWithFrontSight(useDBuf);
    }

    return FALSE;
}
