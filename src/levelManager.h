#ifndef LEVEL_MANAGER_H
#define LEVEL_MANAGER_H

#include <exec/types.h>
#include "rangeHandler.h"

extern BOOL IS_NEW_GAME_SESSION;

typedef enum LevelManagerMode {
    LEVEL_MANAGER_MODE_DEMO = 0,
    LEVEL_MANAGER_MODE_LEVEL1
} LevelManagerMode;

void LevelManager_Init(void);
void LevelManager_Shutdown(void);
void LevelManager_SetMode(LevelManagerMode mode);
LevelManagerMode LevelManager_GetMode(void);
BOOL LevelManager_RunCurrent(BOOL useDBuf, RangeSummaryData *outSummary);

#endif
