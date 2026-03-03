#ifndef INPUT_H
#define INPUT_H

#include <exec/types.h>

/* Forward declaration to avoid heavy includes in headers */
struct Window;

/* Joystick init/shutdown */
BOOL Input_Init(void);
void Input_Shutdown(void);

/* Joystick state (port 2) */
BOOL IsJoystickFirePressed(void);
BOOL Input_Left(void);
BOOL Input_Right(void);
BOOL Input_Up(void);
BOOL Input_Down(void);

/*
 * IDCMP (keyboard + mouse) handling
 *
 * Call Input_PollWindow() once per frame in the active gameplay loop
 * (range/sight loop). This consumes window->UserPort messages.
 */
void Input_PollWindow(struct Window *win);

/* "Pressed edge" queries (true once per key-down, then clears) */
BOOL Input_KeyPressed(UBYTE rawCode);

/* Convenience: fire event edge (mouse LMB or joystick fire), clears after read */
BOOL Input_FirePressed(void);

#endif