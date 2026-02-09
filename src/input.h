#ifndef INPUT_H
#define INPUT_H

#include <exec/types.h>

BOOL Input_Init(void);
void Input_Shutdown(void);

BOOL IsJoystickFirePressed(void);

#endif
