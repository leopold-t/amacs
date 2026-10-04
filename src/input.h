#ifndef INPUT_H
#define INPUT_H

#include <exec/types.h>

struct Window;

BOOL Input_Init(void);
void Input_Shutdown(void);

BOOL IsJoystickFirePressed(void);
BOOL Input_Left(void);
BOOL Input_Right(void);
BOOL Input_Up(void);
BOOL Input_Down(void);
void Input_ResetState(void);

void Input_PollWindow(struct Window *win);

BOOL Input_KeyPressed(UBYTE rawCode);
BOOL Input_QuitPressed(void);
BOOL Input_FirePressed(void);
BOOL Input_IsFireDown(void);

/* Device-specific controls used by Firing Range exclusive control modes. */
BOOL Input_JoyLeft(void);
BOOL Input_JoyRight(void);
BOOL Input_JoyUp(void);
BOOL Input_JoyDown(void);
BOOL Input_KeyboardLeft(void);
BOOL Input_KeyboardRight(void);
BOOL Input_KeyboardUp(void);
BOOL Input_KeyboardDown(void);
BOOL Input_JoyFirePressed(void);
BOOL Input_JoyFireDown(void);
BOOL Input_KeyboardFirePressed(void);
BOOL Input_KeyboardFireDown(void);
BOOL Input_MouseFirePressed(void);
BOOL Input_MouseFireDown(void);
void Input_GetMouseDelta(WORD *dx, WORD *dy);
void Input_PeekMouseDelta(WORD *dx, WORD *dy);

#endif