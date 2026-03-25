#include "soundHandler.h"
#include "assets.h"

#include <devices/audio.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>

extern VOID BeginIO(struct IORequest *);

#define SHOT_PERIOD 161
#define SHOT_VOLUME 64
#define SHOT_CYCLES 1

static struct MsgPort *gAudioPort = NULL;
static struct IOAudio *gAudioIO = NULL;

static BYTE *gShotData = NULL;
static ULONG gShotLength = 0;

static BOOL gSoundInited = FALSE;
static BOOL gShotPlaying = FALSE;
static SoundError gLastError = SOUND_OK;

static UBYTE gChannelMap[] = {15};

static void ResetState(void) {
    gSoundInited = FALSE;
    gShotPlaying = FALSE;
}

static void FreeSample(void) {
    if (gShotData) {
        FreeMem(gShotData, gShotLength);
        gShotData = NULL;
        gShotLength = 0;
    }
}

static void DeleteAudioIO(void) {
    if (gAudioIO) {
        DeleteIORequest((struct IORequest *)gAudioIO);
        gAudioIO = NULL;
    }

    if (gAudioPort) {
        while (GetMsg(gAudioPort)) {
        }
        DeleteMsgPort(gAudioPort);
        gAudioPort = NULL;
    }
}

static void CloseAudio(void) {
    if (gAudioIO) {
        CloseDevice((struct IORequest *)gAudioIO);
    }

    DeleteAudioIO();
}

static void StopPlayback(void) {
    if (!gAudioIO || !gShotPlaying) {
        return;
    }

    AbortIO((struct IORequest *)gAudioIO);
    WaitIO((struct IORequest *)gAudioIO);
    gShotPlaying = FALSE;
}

static void ReapPlayback(void) {
    if (!gAudioIO || !gShotPlaying) {
        return;
    }

    if (CheckIO((struct IORequest *)gAudioIO)) {
        WaitIO((struct IORequest *)gAudioIO);
        gShotPlaying = FALSE;
    }
}

static BOOL LoadRawSample(const char *path, BYTE **outData, ULONG *outLen) {
    BPTR fh;
    LONG size;
    BYTE *buf;

    *outData = NULL;
    *outLen = 0;

    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh) {
        gLastError = SOUND_ERR_OPENFILE;
        return FALSE;
    }

    if (Seek(fh, 0, OFFSET_END) < 0) {
        Close(fh);
        gLastError = SOUND_ERR_FILESIZE;
        return FALSE;
    }

    size = Seek(fh, 0, OFFSET_CURRENT);
    if (size <= 1) {
        Close(fh);
        gLastError = SOUND_ERR_FILESIZE;
        return FALSE;
    }

    if (Seek(fh, 0, OFFSET_BEGINNING) < 0) {
        Close(fh);
        gLastError = SOUND_ERR_FILESIZE;
        return FALSE;
    }

    if (size & 1) {
        size--;
    }

    if (size <= 1) {
        Close(fh);
        gLastError = SOUND_ERR_FILESIZE;
        return FALSE;
    }

    buf = (BYTE *)AllocMem((ULONG)size, MEMF_CHIP);
    if (!buf) {
        Close(fh);
        gLastError = SOUND_ERR_ALLOCMEM;
        return FALSE;
    }

    if (Read(fh, buf, size) != size) {
        FreeMem(buf, (ULONG)size);
        Close(fh);
        gLastError = SOUND_ERR_READFILE;
        return FALSE;
    }

    Close(fh);

    *outData = buf;
    *outLen = (ULONG)size;
    return TRUE;
}

SoundError Sound_GetLastError(void) {
    return gLastError;
}

BOOL Sound_Init(void) {
    if (gSoundInited) {
        gLastError = SOUND_OK;
        return TRUE;
    }

    ResetState();
    gLastError = SOUND_OK;

    if (!LoadRawSample(SHOT_FILE, &gShotData, &gShotLength)) {
        return FALSE;
    }

    gAudioPort = CreateMsgPort();
    if (!gAudioPort) {
        gLastError = SOUND_ERR_PORT;
        FreeSample();
        return FALSE;
    }

    gAudioIO = (struct IOAudio *)CreateIORequest(gAudioPort, sizeof(struct IOAudio));
    if (!gAudioIO) {
        gLastError = SOUND_ERR_IOREQ;
        DeleteAudioIO();
        FreeSample();
        return FALSE;
    }

    gAudioIO->ioa_Request.io_Message.mn_Node.ln_Pri = 0;
    gAudioIO->ioa_Request.io_Command = ADCMD_ALLOCATE;
    gAudioIO->ioa_Request.io_Flags = ADIOF_NOWAIT;
    gAudioIO->ioa_Data = gChannelMap;
    gAudioIO->ioa_Length = sizeof(gChannelMap);
    gAudioIO->ioa_AllocKey = 0;

    if (OpenDevice(AUDIONAME, 0L, (struct IORequest *)gAudioIO, 0L) != 0) {
        gLastError = SOUND_ERR_OPENDEVICE;
        DeleteAudioIO();
        FreeSample();
        return FALSE;
    }

    gSoundInited = TRUE;
    gShotPlaying = FALSE;
    gLastError = SOUND_OK;
    return TRUE;
}

void Sound_Shutdown(void) {
    StopPlayback();
    CloseAudio();
    FreeSample();
    ResetState();
    gLastError = SOUND_OK;
}

void Sound_Update(void) {
    ReapPlayback();
}

void Sound_PlayShot(void) {
    if (!gSoundInited || !gAudioIO || !gShotData || gShotLength == 0) {
        return;
    }

    ReapPlayback();
    if (gShotPlaying) {
        return;
    }

    gAudioIO->ioa_Request.io_Command = CMD_WRITE;
    gAudioIO->ioa_Request.io_Flags = ADIOF_PERVOL;
    gAudioIO->ioa_Data = (UBYTE *)gShotData;
    gAudioIO->ioa_Length = gShotLength;
    gAudioIO->ioa_Period = SHOT_PERIOD;
    gAudioIO->ioa_Volume = SHOT_VOLUME;
    gAudioIO->ioa_Cycles = SHOT_CYCLES;

    BeginIO((struct IORequest *)gAudioIO);

    if (gAudioIO->ioa_Request.io_Flags & IOF_QUICK) {
        gShotPlaying = FALSE;
        return;
    }

    gShotPlaying = TRUE;
}