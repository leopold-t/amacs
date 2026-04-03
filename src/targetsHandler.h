#ifndef TARGETS_HANDLER_H
#define TARGETS_HANDLER_H

#include <exec/types.h>
#include <graphics/rastport.h>

typedef enum TargetDistance {
    TARGET_DISTANCE_050 = 50,
    TARGET_DISTANCE_100 = 100,
    TARGET_DISTANCE_150 = 150,
    TARGET_DISTANCE_200 = 200,
    TARGET_DISTANCE_250 = 250,
    TARGET_DISTANCE_300 = 300
} TargetDistance;

typedef struct TargetInfo {
    BOOL loaded;
    BOOL visible;
    BOOL hit;
    UWORD distance;
    UWORD slotIndex;
    UWORD slotCount;
    WORD x;
    WORD y;
    WORD width;
    WORD height;
    UWORD hitDelayTicks;
} TargetInfo;

BOOL TargetsHandler_Init(void);
void TargetsHandler_Shutdown(void);
void TargetsHandler_Reset(void);
void TargetsHandler_ToggleSlot(UWORD slot);
void TargetsHandler_Tick(void);
void TargetsHandler_Draw(struct RastPort *rp);
void TargetsHandler_SetPaused(BOOL paused);
BOOL TargetsHandler_CheckHit(WORD x, WORD y, UWORD *hitDelayTicks, UBYTE *hitScore);
UWORD TargetsHandler_GetSlotCount(TargetDistance distance);
BOOL TargetsHandler_SelectSlot(TargetDistance distance, UWORD slotIndex);
BOOL TargetsHandler_GetTargetInfo(TargetDistance distance, TargetInfo *outInfo);

#endif
