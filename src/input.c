#include <exec/types.h>
#include <exec/libraries.h>

#include <proto/exec.h>
#include <proto/lowlevel.h>
#include <libraries/lowlevel.h>   /* JPF_* masks */

#include "input.h"

/*
 * Joystick handling using lowlevel.library (ReadJoyPort).
 *
 * IMPORTANT:
 * - We do NOT link with -llowlevel (often not present as liblowlevel.a).
 * - proto/lowlevel.h uses direct library calls via LowLevelBase.
 * - LowLevelBase is declared by proto/lowlevel.h, so we must not redeclare it.
 */

BOOL Input_Init(void)
{
    LowLevelBase = OpenLibrary("lowlevel.library", 0);
    return (LowLevelBase != NULL);
}

void Input_Shutdown(void)
{
    if (LowLevelBase) {
        CloseLibrary(LowLevelBase);
        LowLevelBase = NULL;
    }
}

BOOL IsJoystickFirePressed(void)
{
    ULONG p0, p1;

    if (!LowLevelBase)
        return FALSE;

    /* Read both ports: 0 = port 1, 1 = port 2 (depending on setup/emulator) */
    p0 = ReadJoyPort(0);
    p1 = ReadJoyPort(1);

    /* Standard fire button mask */
    if ((p0 & JPF_BUTTON_RED) || (p1 & JPF_BUTTON_RED))
        return TRUE;

    return FALSE;
}
