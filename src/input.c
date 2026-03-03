#include <exec/libraries.h>
#include <exec/types.h>

#include <intuition/intuition.h>
#include <intuition/screens.h>

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

/* ---------------- IDCMP key/mouse state ---------------- */

static UBYTE keyDown[256];
static UBYTE keyPressed[256]; /* edge: set on key-down transition */
static BOOL firePressedEdge = FALSE;

BOOL Input_Init(void) {
    LowLevelBase = OpenLibrary("lowlevel.library", 0);

    for (int i = 0; i < 256; i++) {
        keyDown[i] = 0;
        keyPressed[i] = 0;
    }
    firePressedEdge = FALSE;

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
    return (p & JPF_BUTTON_RED) ? TRUE : FALSE;
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

/*
 * Consume IDCMP messages and update edge states.
 * Must be called from the active loop (e.g. sightHandler) once per frame.
 */
void Input_PollWindow(struct Window *win) {
    struct IntuiMessage *msg;

    /* Fire edge from joystick (treat as "pressed") */
    if (IsJoystickFirePressed()) {
        firePressedEdge = TRUE;
    }

    if (!win || !win->UserPort) {
        return;
    }

    while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {

        if (msg->Class == IDCMP_RAWKEY) {
            UBYTE code = (UBYTE)msg->Code;

            /* key up events are code | 0x80 */
            if (code & 0x80) {
                UBYTE downCode = (UBYTE)(code & 0x7F);
                keyDown[downCode] = 0;
            } else {
                /* key-down */
                if (!keyDown[code]) {
                    keyPressed[code] = 1; /* rising edge */
                }
                keyDown[code] = 1;
            }
        }

        if (msg->Class == IDCMP_MOUSEBUTTONS && msg->Code == SELECTDOWN) {
            firePressedEdge = TRUE;
        }

        ReplyMsg((struct Message *)msg);
    }
}

BOOL Input_KeyPressed(UBYTE rawCode) {
    if (keyPressed[rawCode]) {
        keyPressed[rawCode] = 0;
        return TRUE;
    }
    return FALSE;
}

BOOL Input_FirePressed(void) {
    if (firePressedEdge) {
        firePressedEdge = FALSE;
        return TRUE;
    }
    return FALSE;
}