#include <exec/libraries.h>
#include <exec/types.h>

#include <libraries/lowlevel.h> /* JPF_* masks */
#include <proto/exec.h>
#include <proto/lowlevel.h>

#include "input.h"

/*
 * Joystick handling using lowlevel.library (ReadJoyPort).
 *
 * IMPORTANT:
 * - We do NOT link with -llowlevel (often not present as liblowlevel.a).
 * - proto/lowlevel.h uses direct library calls via LowLevelBase.
 * - LowLevelBase is declared by proto/lowlevel.h, so we must not redeclare it.
 */

static ULONG ReadJoyPort2(void) {
    if (!LowLevelBase) {
        return 0;
    }
    /* Port 2 = index 1 in ReadJoyPort() */
    return ReadJoyPort(1);
}

BOOL Input_Init(void) {
    LowLevelBase = OpenLibrary("lowlevel.library", 0);
    return (LowLevelBase != NULL);
}

void Input_Shutdown(void) {
    if (LowLevelBase) {
        CloseLibrary(LowLevelBase);
        LowLevelBase = NULL;
    }
}

BOOL IsJoystickFirePressed(void) {
    ULONG p = ReadJoyPort2();
    if (p & JPF_BUTTON_RED) {
        return TRUE;
    }
    return FALSE;
}

BOOL Input_Left(void) {
    ULONG p = ReadJoyPort2();
    return (p & JPF_JOY_LEFT) ? TRUE : FALSE;
}

BOOL Input_Right(void) {
    ULONG p = ReadJoyPort2();
    return (p & JPF_JOY_RIGHT) ? TRUE : FALSE;
}

BOOL Input_Up(void) {
    ULONG p = ReadJoyPort2();
    return (p & JPF_JOY_UP) ? TRUE : FALSE;
}

BOOL Input_Down(void) {
    ULONG p = ReadJoyPort2();
    return (p & JPF_JOY_DOWN) ? TRUE : FALSE;
}