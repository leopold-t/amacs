#ifndef TARGETS_HANDLER_H
#define TARGETS_HANDLER_H

#include <exec/types.h>
#include <graphics/rastport.h>

/*
 * TargetsHandler
 * -------------
 * Owns target BOB resources and per-slot animation state.
 *
 * Current behavior (demo mode):
 * - One target type (50m): Target050.raw + Target050.mask (48x23, depth 5)
 * - Five predefined slots along the ground line.
 * - Automatic cycle:
 *     slot 0 rises from the ground,
 *     stays visible for ~5 seconds,
 *     instantly disappears,
 *     next slot rises, ...
 *
 * IMPORTANT:
 * - This module does NOT read IDCMP messages.
 * - The range loop should call:
 *     TargetsHandler_Init() once on entry,
 *     TargetsHandler_Tick() once per frame,
 *     TargetsHandler_Draw(rp) each frame (after background restore,
 *     before drawing the optic).
 *
 * Optional:
 * - TargetsHandler_ToggleSlot(slot) can be used later for manual tests.
 */

BOOL TargetsHandler_Init(void);
void TargetsHandler_Shutdown(void);

/* Optional manual override (for later): jump to slot and restart timer */
void TargetsHandler_ToggleSlot(UWORD slot);

/* Advance animation + demo-cycle timer (call once per frame) */
void TargetsHandler_Tick(void);

/* Draw current target(s) (call each frame, after restoring background) */
void TargetsHandler_Draw(struct RastPort *rp);

#endif