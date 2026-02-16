#ifndef INPUT_H
#define INPUT_H

#include <exec/types.h>

BOOL Input_Init(void);
void Input_Shutdown(void);

BOOL IsJoystickFirePressed(void);

/* Direction helpers */
BOOL Input_Left(void);
BOOL Input_Right(void);
BOOL Input_Up(void);
BOOL Input_Down(void);

#endif