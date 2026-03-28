#ifndef TARGETS_HANDLER_H
#define TARGETS_HANDLER_H

#include <exec/types.h>
#include <graphics/rastport.h>

BOOL TargetsHandler_Init(void);
void TargetsHandler_Shutdown(void);
void TargetsHandler_ToggleSlot(UWORD slot);
void TargetsHandler_Tick(void);
void TargetsHandler_Draw(struct RastPort *rp);
BOOL TargetsHandler_CheckHit(WORD x, WORD y, UWORD *hitDelayTicks);

#endif
