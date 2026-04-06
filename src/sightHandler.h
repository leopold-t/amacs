#ifndef SIGHTHANDLER_H
#define SIGHTHANDLER_H

#include <exec/types.h>

/* Runs the range loop with a movable front sight.
 * useDBuf:
 *  - TRUE  => draw to back buffer via Gfx_GetDrawRastPort() and swap
 *  - FALSE => draw directly to screen RastPort (no DBuf)
 */
BOOL RunRangeWithFrontSight(BOOL useDBuf);

#endif /* SIGHTHANDLER_H */