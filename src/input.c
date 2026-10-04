#include <exec/libraries.h>
#include <exec/types.h>

#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>

#include <libraries/lowlevel.h>
#include <proto/exec.h>
#include <proto/lowlevel.h>

/* Keep lowlevel.library optional until Input_Init() succeeds.
   Providing our own base symbol prevents the C runtime/libamiga
   from treating lowlevel.library as a mandatory startup dependency. */
struct Library *LowLevelBase = NULL;

#include "input.h"
#include "gfx.h"

static ULONG ReadJoyPort2(void) {
    if (!LowLevelBase) {
        return 0;
    }

    return ReadJoyPort(1);
}

static UBYTE keyDown[256];
static UBYTE keyPressed[256];
static BOOL firePressedEdge = FALSE;
static BOOL quitPressedEdge = FALSE;
static BOOL joyFireDown = FALSE;
static BOOL mouseFireDown = FALSE;
static LONG mouseDeltaX = 0;
static LONG mouseDeltaY = 0;

#ifndef IEQUALIFIER_LCOMMAND
#define IEQUALIFIER_LCOMMAND 0x0080
#endif
#ifndef IEQUALIFIER_RCOMMAND
#define IEQUALIFIER_RCOMMAND 0x0800
#endif

#define RAWKEY_F1 0x50
#define RAWKEY_F2 0x51
#define RAWKEY_F3 0x52
#define RAWKEY_Q 0x10
#define RAWKEY_W 0x11
#define RAWKEY_P 0x19
#define RAWKEY_A 0x20
#define RAWKEY_S 0x21
#define RAWKEY_D 0x22
#define RAWKEY_SPACE 0x40
#define RAWKEY_NUMPAD_ENTER 0x43
#define RAWKEY_RETURN 0x44

static BOOL IsAmigaQualifier(UWORD qualifier) {
    return (qualifier & (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)) ? TRUE : FALSE;
}

BOOL Input_Init(void) {
    int i;

    LowLevelBase = OpenLibrary("lowlevel.library", 0);

    for (i = 0; i < 256; i++) {
        keyDown[i] = 0;
        keyPressed[i] = 0;
    }

    firePressedEdge = FALSE;
    quitPressedEdge = FALSE;
    joyFireDown = FALSE;
    mouseFireDown = FALSE;
    mouseDeltaX = 0;
    mouseDeltaY = 0;

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
    return ((p & JPF_JOY_LEFT) || keyDown[RAWKEY_A]) ? TRUE : FALSE;
}

BOOL Input_Right(void) {
    ULONG p = ReadJoyPort2();
    return ((p & JPF_JOY_RIGHT) || keyDown[RAWKEY_D]) ? TRUE : FALSE;
}

BOOL Input_Up(void) {
    ULONG p = ReadJoyPort2();
    return ((p & JPF_JOY_UP) || keyDown[RAWKEY_W]) ? TRUE : FALSE;
}

BOOL Input_Down(void) {
    ULONG p = ReadJoyPort2();
    return ((p & JPF_JOY_DOWN) || keyDown[RAWKEY_S]) ? TRUE : FALSE;
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
                if (code == RAWKEY_Q && IsAmigaQualifier(msg->Qualifier)) {
                    quitPressedEdge = TRUE;
                }

                /* Space, Return and numeric keypad Enter are keyboard triggers.
                 * Keep joystick Fire and LMB active as parallel controls until
                 * the Settings menu makes the devices mutually exclusive. */
                if ((code == RAWKEY_SPACE || code == RAWKEY_RETURN ||
                     code == RAWKEY_NUMPAD_ENTER) && !keyDown[code]) {
                    firePressedEdge = TRUE;
                }

                if (code == RAWKEY_F1 && !keyDown[code]) {
                    Gfx_ToggleNightVision();
                } else if (code == RAWKEY_F2 && !keyDown[code]) {
                    Gfx_ToggleRedRoom();
                } else if (code == RAWKEY_F3 && !keyDown[code]) {
                    Gfx_ToggleAmber();
                }

                if (!keyDown[code]) {
                    keyPressed[code] = 1;
                }

                keyDown[code] = 1;
            }
        }

        if (msg->Class == IDCMP_VANILLAKEY) {
            UBYTE c = (UBYTE)(msg->Code & 0xFF);

            if ((c == 'q' || c == 'Q') && IsAmigaQualifier(msg->Qualifier)) {
                quitPressedEdge = TRUE;
            } else if (c == 'p' || c == 'P') {
                /* Some Intuition configurations deliver printable keys as
                 * VANILLAKEY only. Mirror P into the raw-key edge table so
                 * gameplay pause keeps working through Input_KeyPressed().
                 */
                keyPressed[RAWKEY_P] = 1;
            }
        }

        if (msg->Class == IDCMP_MOUSEMOVE) {
            /* With IDCMP_DELTAMOVE enabled, MouseX/MouseY are relative
             * movement values rather than absolute pointer coordinates. */
            mouseDeltaX += (LONG)msg->MouseX;
            mouseDeltaY += (LONG)msg->MouseY;
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

BOOL Input_QuitPressed(void) {
    if (quitPressedEdge) {
        quitPressedEdge = FALSE;
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
    return (joyFireDown || mouseFireDown || keyDown[RAWKEY_SPACE] ||
            keyDown[RAWKEY_RETURN] || keyDown[RAWKEY_NUMPAD_ENTER]) ? TRUE : FALSE;
}

void Input_PeekMouseDelta(WORD *dx, WORD *dy) {
    LONG x = mouseDeltaX;
    LONG y = mouseDeltaY;

    /* Clamp only to the WORD API range; do not consume the accumulated
     * movement.  The Range reload gesture can inspect it before the aiming
     * code consumes the same delta later in the frame. */
    if (x < -32768L) x = -32768L;
    if (x > 32767L) x = 32767L;
    if (y < -32768L) y = -32768L;
    if (y > 32767L) y = 32767L;

    if (dx) *dx = (WORD)x;
    if (dy) *dy = (WORD)y;
}

void Input_GetMouseDelta(WORD *dx, WORD *dy) {
    LONG x = mouseDeltaX;
    LONG y = mouseDeltaY;

    /* Clamp only to the WORD API range; normal per-frame deltas are tiny. */
    if (x < -32768L) x = -32768L;
    if (x > 32767L) x = 32767L;
    if (y < -32768L) y = -32768L;
    if (y > 32767L) y = 32767L;

    if (dx) *dx = (WORD)x;
    if (dy) *dy = (WORD)y;

    mouseDeltaX = 0;
    mouseDeltaY = 0;
}

void Input_ResetState(void) {
    UWORD i;
    firePressedEdge = FALSE;
    quitPressedEdge = FALSE;
    joyFireDown = FALSE;
    mouseFireDown = FALSE;
    mouseDeltaX = 0;
    mouseDeltaY = 0;

    for (i = 0; i < 256; i++) {
        keyDown[i] = FALSE;
        keyPressed[i] = FALSE;
    }
}
