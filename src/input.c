#include <exec/libraries.h>
#include <exec/types.h>

#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>

#ifndef AMACS_JOY_HARDWARE_ONLY
#include <libraries/lowlevel.h>
#include <proto/exec.h>
#include <proto/lowlevel.h>
#endif

#include "input.h"
#include "gfx.h"

/* ------------------------------------------------------------------ */
/* Joystick (controller port 2)                                        */
/*                                                                     */
/* lowlevel.library is OPTIONAL.  Input_Init() tries to open it; if    */
/* it is present, ReadJoyPort(1) is used (USB/Poseidon joysticks, CD32 */
/* pads and device auto-detection keep working).  If it is missing,    */
/* the joystick is read directly from hardware instead.  Port 2 is the */
/* joystick port; port 1 carries the mouse.  The fallback performs the */
/* same register-level read lowlevel.library does for a standard      */
/* digital joystick:                                                   */
/*                                                                     */
/*   JOY1DAT ($DFF00C)  direction counters of port 2                   */
/*       Right = bit 1                                                 */
/*       Left  = bit 9                                                 */
/*       Down  = bit 1 XOR bit 0                                       */
/*       Up    = bit 9 XOR bit 8                                       */
/*   CIAA PRA ($BFE001) bit 7 = port 2 fire button (active low)        */
/*                                                                     */
/* Both registers are read-only here: nothing is written, so the OS    */
/* and the mouse on port 1 are not disturbed.                          */
/*                                                                     */
/* Build with -DAMACS_JOY_HARDWARE_ONLY to drop every reference to     */
/* lowlevel.library (headers included) and always use the hardware     */
/* path.                                                               */
/* ------------------------------------------------------------------ */
#ifndef AMACS_JOY_HARDWARE_ONLY
/* Our own base symbol keeps the C runtime/libamiga from treating
 * lowlevel.library as a mandatory startup dependency; it stays NULL
 * unless Input_Init() manages to open the library. */
struct Library *LowLevelBase = NULL;
#endif

#define JOY2_JOY1DAT_ADDR 0x00DFF00CUL
#define JOY2_CIAA_PRA_ADDR 0x00BFE001UL
#define JOY2_CIAA_FIRE_BIT 7

#define JOY_BIT_UP 0x01
#define JOY_BIT_DOWN 0x02
#define JOY_BIT_LEFT 0x04
#define JOY_BIT_RIGHT 0x08
#define JOY_BIT_FIRE 0x10

/* Pure decoder (no hardware access) - kept separate so it can be unit tested. */
static UBYTE DecodeJoyPort2(UWORD joy1dat, UBYTE ciaaPra) {
    UBYTE r = 0;

    if (joy1dat & 0x0200) {
        r |= JOY_BIT_LEFT;
    }
    if (joy1dat & 0x0002) {
        r |= JOY_BIT_RIGHT;
    }
    if (((joy1dat >> 9) ^ (joy1dat >> 8)) & 0x0001) {
        r |= JOY_BIT_UP;
    }
    if (((joy1dat >> 1) ^ joy1dat) & 0x0001) {
        r |= JOY_BIT_DOWN;
    }
    if (!(ciaaPra & (1 << JOY2_CIAA_FIRE_BIT))) {
        r |= JOY_BIT_FIRE;
    }

    return r;
}

static UBYTE ReadJoyPort2Hardware(void) {
    volatile UWORD *joy1dat = (volatile UWORD *)JOY2_JOY1DAT_ADDR;
    volatile UBYTE *ciaaPra = (volatile UBYTE *)JOY2_CIAA_PRA_ADDR;

    return DecodeJoyPort2(*joy1dat, *ciaaPra);
}

#ifndef AMACS_JOY_HARDWARE_ONLY
static UBYTE ReadJoyPort2LowLevel(void) {
    ULONG p = ReadJoyPort(1);
    UBYTE r = 0;

    if (p & JPF_JOY_UP) {
        r |= JOY_BIT_UP;
    }
    if (p & JPF_JOY_DOWN) {
        r |= JOY_BIT_DOWN;
    }
    if (p & JPF_JOY_LEFT) {
        r |= JOY_BIT_LEFT;
    }
    if (p & JPF_JOY_RIGHT) {
        r |= JOY_BIT_RIGHT;
    }
    if (p & JPF_BUTTON_RED) {
        r |= JOY_BIT_FIRE;
    }

    return r;
}
#endif

/* Returns a JOY_BIT_* mask for port 2, through lowlevel.library when it
 * was opened and straight from the hardware otherwise. */
static UBYTE ReadJoyPort2(void) {
#ifndef AMACS_JOY_HARDWARE_ONLY
    if (LowLevelBase) {
        return ReadJoyPort2LowLevel();
    }
#endif

    return ReadJoyPort2Hardware();
}

static UBYTE keyDown[256];
static UBYTE keyPressed[256];
static BOOL joyFirePressedEdge = FALSE;
static BOOL keyboardFirePressedEdge = FALSE;
static BOOL mouseFirePressedEdge = FALSE;
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
/* Amiga cursor keys (raw key codes). */
#define RAWKEY_CURSOR_UP 0x4C
#define RAWKEY_CURSOR_DOWN 0x4D
#define RAWKEY_CURSOR_RIGHT 0x4E
#define RAWKEY_CURSOR_LEFT 0x4F
#define RAWKEY_SPACE 0x40
#define RAWKEY_NUMPAD_ENTER 0x43
#define RAWKEY_RETURN 0x44

static BOOL IsAmigaQualifier(UWORD qualifier) {
    return (qualifier & (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)) ? TRUE : FALSE;
}

BOOL Input_Init(void) {
    int i;

#ifndef AMACS_JOY_HARDWARE_ONLY
    /* Optional: a failed open is not an error, ReadJoyPort2() simply
     * falls back to the hardware path. */
    LowLevelBase = OpenLibrary("lowlevel.library", 0);
#endif

    for (i = 0; i < 256; i++) {
        keyDown[i] = 0;
        keyPressed[i] = 0;
    }

    joyFirePressedEdge = FALSE;
    keyboardFirePressedEdge = FALSE;
    mouseFirePressedEdge = FALSE;
    quitPressedEdge = FALSE;
    joyFireDown = FALSE;
    mouseFireDown = FALSE;
    mouseDeltaX = 0;
    mouseDeltaY = 0;

    return TRUE;
}

void Input_Shutdown(void) {
#ifndef AMACS_JOY_HARDWARE_ONLY
    if (LowLevelBase) {
        CloseLibrary(LowLevelBase);
        LowLevelBase = NULL;
    }
#endif
}

BOOL IsJoystickFirePressed(void) {
    UBYTE p = ReadJoyPort2();
    return (p & JOY_BIT_FIRE) ? TRUE : FALSE;
}

BOOL Input_JoyLeft(void) { return (ReadJoyPort2() & JOY_BIT_LEFT) ? TRUE : FALSE; }
BOOL Input_JoyRight(void) { return (ReadJoyPort2() & JOY_BIT_RIGHT) ? TRUE : FALSE; }
BOOL Input_JoyUp(void) { return (ReadJoyPort2() & JOY_BIT_UP) ? TRUE : FALSE; }
BOOL Input_JoyDown(void) { return (ReadJoyPort2() & JOY_BIT_DOWN) ? TRUE : FALSE; }

BOOL Input_KeyboardLeft(void) { return (keyDown[RAWKEY_A] || keyDown[RAWKEY_CURSOR_LEFT]) ? TRUE : FALSE; }
BOOL Input_KeyboardRight(void) { return (keyDown[RAWKEY_D] || keyDown[RAWKEY_CURSOR_RIGHT]) ? TRUE : FALSE; }
BOOL Input_KeyboardUp(void) { return (keyDown[RAWKEY_W] || keyDown[RAWKEY_CURSOR_UP]) ? TRUE : FALSE; }
BOOL Input_KeyboardDown(void) { return (keyDown[RAWKEY_S] || keyDown[RAWKEY_CURSOR_DOWN]) ? TRUE : FALSE; }

BOOL Input_Left(void) { return (Input_JoyLeft() || Input_KeyboardLeft()) ? TRUE : FALSE; }
BOOL Input_Right(void) { return (Input_JoyRight() || Input_KeyboardRight()) ? TRUE : FALSE; }
BOOL Input_Up(void) { return (Input_JoyUp() || Input_KeyboardUp()) ? TRUE : FALSE; }
BOOL Input_Down(void) { return (Input_JoyDown() || Input_KeyboardDown()) ? TRUE : FALSE; }

void Input_PollWindow(struct Window *win) {
    struct IntuiMessage *msg;
    BOOL joyNow;

    joyNow = IsJoystickFirePressed();

    if (joyNow && !joyFireDown) {
        joyFirePressedEdge = TRUE;
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
                    keyboardFirePressedEdge = TRUE;
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
                    mouseFirePressedEdge = TRUE;
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

BOOL Input_JoyFirePressed(void) {
    if (joyFirePressedEdge) { joyFirePressedEdge = FALSE; return TRUE; }
    return FALSE;
}

BOOL Input_KeyboardFirePressed(void) {
    if (keyboardFirePressedEdge) { keyboardFirePressedEdge = FALSE; return TRUE; }
    return FALSE;
}

BOOL Input_MouseFirePressed(void) {
    if (mouseFirePressedEdge) { mouseFirePressedEdge = FALSE; return TRUE; }
    return FALSE;
}

BOOL Input_JoyFireDown(void) { return joyFireDown; }
BOOL Input_KeyboardFireDown(void) {
    return (keyDown[RAWKEY_SPACE] || keyDown[RAWKEY_RETURN] ||
            keyDown[RAWKEY_NUMPAD_ENTER]) ? TRUE : FALSE;
}
BOOL Input_MouseFireDown(void) { return mouseFireDown; }

BOOL Input_FirePressed(void) {
    BOOL pressed = (joyFirePressedEdge || keyboardFirePressedEdge || mouseFirePressedEdge) ? TRUE : FALSE;
    joyFirePressedEdge = FALSE;
    keyboardFirePressedEdge = FALSE;
    mouseFirePressedEdge = FALSE;
    return pressed;
}

BOOL Input_IsFireDown(void) {
    return (Input_JoyFireDown() || Input_KeyboardFireDown() || Input_MouseFireDown()) ? TRUE : FALSE;
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
    joyFirePressedEdge = FALSE;
    keyboardFirePressedEdge = FALSE;
    mouseFirePressedEdge = FALSE;
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
