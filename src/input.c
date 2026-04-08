#include <exec/libraries.h>
#include <exec/types.h>

#include <intuition/intuition.h>
#include <intuition/screens.h>

#include <libraries/lowlevel.h>
#include <proto/exec.h>
#include <proto/lowlevel.h>

#include "input.h"

static ULONG ReadJoyPort2(void) {
    if (!LowLevelBase) {
        return 0;
    }

    return ReadJoyPort(1);
}

static UBYTE keyDown[256];
static UBYTE keyPressed[256];
static BOOL firePressedEdge = FALSE;
static BOOL joyFireDown = FALSE;
static BOOL mouseFireDown = FALSE;

BOOL Input_Init(void) {
    int i;

    LowLevelBase = OpenLibrary("lowlevel.library", 0);

    for (i = 0; i < 256; i++) {
        keyDown[i] = 0;
        keyPressed[i] = 0;
    }

    firePressedEdge = FALSE;
    joyFireDown = FALSE;
    mouseFireDown = FALSE;

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

void Input_PollWindow(struct Window *win) {
    struct IntuiMessage *msg;
    BOOL joyNow;

    joyNow = IsJoystickFirePressed();
    if (joyNow && !joyFireDown) {
        firePressedEdge = TRUE;
    }
    joyFireDown = joyNow;

    if (!win || !win->UserPort) {
        return;
    }

    while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {
        if (msg->Class == IDCMP_RAWKEY) {
            UBYTE code = (UBYTE)msg->Code;

            if (code & 0x80) {
                UBYTE downCode = (UBYTE)(code & 0x7F);
                keyDown[downCode] = 0;
            } else {
                if (!keyDown[code]) {
                    keyPressed[code] = 1;
                }
                keyDown[code] = 1;
            }
        }

        if (msg->Class == IDCMP_MOUSEBUTTONS) {
            if (msg->Code == SELECTDOWN) {
                if (!mouseFireDown) {
                    firePressedEdge = TRUE;
                }
                mouseFireDown = TRUE;
            } else if (msg->Code == SELECTUP) {
                mouseFireDown = FALSE;
            }
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

BOOL Input_IsFireDown(void) {
    return (joyFireDown || mouseFireDown) ? TRUE : FALSE;
}
void Input_ResetState(void) {
    UWORD i;
    firePressedEdge = FALSE;
    joyFireDown = FALSE;
    mouseFireDown = FALSE;
    for (i = 0; i < 256; i++) {
        keyDown[i] = FALSE;
        keyPressed[i] = FALSE;
    }
}
