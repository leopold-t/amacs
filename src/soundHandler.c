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

typedef struct Sample {
    BYTE *data;
    ULONG length;
} Sample;

static struct MsgPort *gAudioPort = NULL;
static struct IOAudio *gAudioIO = NULL;

static Sample gShot = {NULL, 0};

static BOOL gSoundInited = FALSE;
static BOOL gShotPlaying = FALSE;
static SoundError gLastError = SOUND_OK;

static UBYTE gChannelMap[] = {15};

static void ResetState(void) {
    gSoundInited = FALSE;
    gShotPlaying = FALSE;
}

static void FreeSample(Sample *sample) {
    if (!sample || !sample->data) {
        return;
    }

    FreeMem(sample->data, sample->length);
    sample->data = NULL;
    sample->length = 0;
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

static BOOL LoadSample(const char *path, Sample *sample) {
    BPTR fh;
    LONG size;
    BYTE *buf;

    if (!sample) {
        gLastError = SOUND_ERR_READFILE;
        return FALSE;
    }

    sample->data = NULL;
    sample->length = 0;

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

    sample->data = buf;
    sample->length = (ULONG)size;
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

    if (!LoadSample(SHOT_FILE, &gShot)) {
        return FALSE;
    }

    gAudioPort = CreateMsgPort();
    if (!gAudioPort) {
        gLastError = SOUND_ERR_PORT;
        FreeSample(&gShot);
        return FALSE;
    }

    gAudioIO = (struct IOAudio *)CreateIORequest(gAudioPort, sizeof(struct IOAudio));
    if (!gAudioIO) {
        gLastError = SOUND_ERR_IOREQ;
        DeleteAudioIO();
        FreeSample(&gShot);
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
        FreeSample(&gShot);
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
    FreeSample(&gShot);
    ResetState();
    gLastError = SOUND_OK;
}

void Sound_Update(void) {
    ReapPlayback();
}

void Sound_PlayShot(void) {
    if (!gSoundInited || !gAudioIO || !gShot.data || gShot.length == 0) {
        return;
    }

    ReapPlayback();
    if (gShotPlaying) {
        return;
    }

    gAudioIO->ioa_Request.io_Command = CMD_WRITE;
    gAudioIO->ioa_Request.io_Flags = ADIOF_PERVOL;
    gAudioIO->ioa_Data = (UBYTE *)gShot.data;
    gAudioIO->ioa_Length = gShot.length;
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