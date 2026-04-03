#ifndef SOUND_HANDLER_H
#define SOUND_HANDLER_H

#include <exec/types.h>

typedef enum SoundError {
    SOUND_OK = 0,
    SOUND_ERR_PORT,
    SOUND_ERR_IOREQ,
    SOUND_ERR_OPENDEVICE,
    SOUND_ERR_OPENFILE,
    SOUND_ERR_FILESIZE,
    SOUND_ERR_ALLOCMEM,
    SOUND_ERR_READFILE
} SoundError;

BOOL Sound_Init(void);
void Sound_Shutdown(void);
void Sound_Update(void);
void Sound_PlayShot(void);
void Sound_PlayHit(UWORD delayTicks);
void Sound_SetPaused(BOOL paused);

SoundError Sound_GetLastError(void);

#endif
