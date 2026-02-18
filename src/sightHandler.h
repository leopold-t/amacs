#ifndef SIGHT_HANDLER_H
#define SIGHT_HANDLER_H

#include <exec/types.h>

/*
 * Runs the range loop with the moving front sight.
 *
 * useDBuf:
 *  - TRUE  => draw to back buffer + swap (Kick 3.x path)
 *  - FALSE => draw directly to screen RastPort (fallback path)
 *
 * The function exits on ESC or Fire/LMB (same logic as the intro uses).
 */
void RunRangeWithFrontSight(BOOL useDBuf);

#endif /* SIGHT_HANDLER_H */