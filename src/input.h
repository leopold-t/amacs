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
BOOL Input_FirePressed(void);
BOOL Input_IsFireDown(void);

#endif