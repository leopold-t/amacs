#include "soundHandler.h"
#include "assets.h"

#include <devices/audio.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>

extern VOID BeginIO(struct IORequest *);

typedef struct Sample {
    BYTE *data;
    ULONG length;
} Sample;

typedef struct AudioVoice {
    struct MsgPort *port;
    struct IOAudio *io;
    BOOL playing;
    UBYTE channelMap[1];
} AudioVoice;

#define SOUND_11KHZ_PERIOD 321

#define SHOT_VOLUME 64
#define SHOT_CYCLES 1

#define HIT_VOLUME 64
#define HIT_CYCLES 1

#define AUDIO_CH_SHOT 1
#define AUDIO_CH_HIT 2

static AudioVoice gShotVoice = {NULL, NULL, FALSE, {AUDIO_CH_SHOT}};
static AudioVoice gHitVoice = {NULL, NULL, FALSE, {AUDIO_CH_HIT}};

static Sample gShot = {NULL, 0};
static Sample gHit = {NULL, 0};

static BOOL gSoundInited = FALSE;
static BOOL gHitPending = FALSE;
static BOOL gSoundPaused = FALSE;
static struct DateStamp gHitDueStamp;
static struct DateStamp gPauseStamp;
static SoundError gLastError = SOUND_OK;

static void ResetState(void) {
    gSoundInited = FALSE;
    gHitPending = FALSE;
    gSoundPaused = FALSE;
    gShotVoice.playing = FALSE;
    gHitVoice.playing = FALSE;
    gHitDueStamp.ds_Days = 0;
    gHitDueStamp.ds_Minute = 0;
    gHitDueStamp.ds_Tick = 0;
    gPauseStamp.ds_Days = 0;
    gPauseStamp.ds_Minute = 0;
    gPauseStamp.ds_Tick = 0;
}

static void FreeSample(Sample *sample) {
    if (!sample || !sample->data) {
        return;
    }

    FreeMem(sample->data, sample->length);
    sample->data = NULL;
    sample->length = 0;
}

static void DeleteVoiceIO(AudioVoice *voice) {
    if (!voice) {
        return;
    }

    if (voice->io) {
        DeleteIORequest((struct IORequest *)voice->io);
        voice->io = NULL;
    }

    if (voice->port) {
        while (GetMsg(voice->port)) {
        }
        DeleteMsgPort(voice->port);
        voice->port = NULL;
    }

    voice->playing = FALSE;
}

static void CloseVoice(AudioVoice *voice) {
    if (!voice) {
        return;
    }

    if (voice->io) {
        CloseDevice((struct IORequest *)voice->io);
    }

    DeleteVoiceIO(voice);
}

static void StopVoice(AudioVoice *voice) {
    if (!voice || !voice->io || !voice->playing) {
        return;
    }

    AbortIO((struct IORequest *)voice->io);
    WaitIO((struct IORequest *)voice->io);
    voice->playing = FALSE;
}

static void ReapVoice(AudioVoice *voice) {
    if (!voice || !voice->io || !voice->playing) {
        return;
    }

    if (CheckIO((struct IORequest *)voice->io)) {
        WaitIO((struct IORequest *)voice->io);
        voice->playing = FALSE;
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

static BOOL InitVoice(AudioVoice *voice) {
    if (!voice) {
        gLastError = SOUND_ERR_IOREQ;
        return FALSE;
    }

    voice->port = CreateMsgPort();
    if (!voice->port) {
        gLastError = SOUND_ERR_PORT;
        return FALSE;
    }

    voice->io = (struct IOAudio *)CreateIORequest(voice->port, sizeof(struct IOAudio));
    if (!voice->io) {
        gLastError = SOUND_ERR_IOREQ;
        DeleteVoiceIO(voice);
        return FALSE;
    }

    voice->io->ioa_Request.io_Message.mn_Node.ln_Pri = 0;
    voice->io->ioa_Request.io_Command = ADCMD_ALLOCATE;
    voice->io->ioa_Request.io_Flags = ADIOF_NOWAIT;
    voice->io->ioa_Data = voice->channelMap;
    voice->io->ioa_Length = sizeof(voice->channelMap);
    voice->io->ioa_AllocKey = 0;

    if (OpenDevice(AUDIONAME, 0L, (struct IORequest *)voice->io, 0L) != 0) {
        gLastError = SOUND_ERR_OPENDEVICE;
        DeleteVoiceIO(voice);
        return FALSE;
    }

    voice->playing = FALSE;
    return TRUE;
}

static void StartVoiceSample(AudioVoice *voice, const Sample *sample, UWORD period, UBYTE volume,
                             UBYTE cycles) {
    if (!gSoundInited || !voice || !voice->io || !sample || !sample->data || sample->length == 0) {
        return;
    }

    voice->io->ioa_Request.io_Command = CMD_WRITE;
    voice->io->ioa_Request.io_Flags = ADIOF_PERVOL;
    voice->io->ioa_Data = (UBYTE *)sample->data;
    voice->io->ioa_Length = sample->length;
    voice->io->ioa_Period = period;
    voice->io->ioa_Volume = volume;
    voice->io->ioa_Cycles = cycles;

    BeginIO((struct IORequest *)voice->io);

    if (voice->io->ioa_Request.io_Flags & IOF_QUICK) {
        voice->playing = FALSE;
        return;
    }

    voice->playing = TRUE;
}

static void AddTicksToDateStamp(struct DateStamp *stamp, UWORD ticks) {
    LONG totalTicks;

    if (!stamp) {
        return;
    }

    totalTicks = (LONG)stamp->ds_Tick + (LONG)ticks;
    stamp->ds_Minute += totalTicks / 3000;
    stamp->ds_Tick = totalTicks % 3000;
}

static LONG CompareDateStamp(const struct DateStamp *a, const struct DateStamp *b) {
    if (a->ds_Days != b->ds_Days) {
        return (LONG)a->ds_Days - (LONG)b->ds_Days;
    }

    if (a->ds_Minute != b->ds_Minute) {
        return (LONG)a->ds_Minute - (LONG)b->ds_Minute;
    }

    return (LONG)a->ds_Tick - (LONG)b->ds_Tick;
}

static void AddDateStampDelta(struct DateStamp *stamp, const struct DateStamp *delta) {
    LONG days;
    LONG minutes;
    LONG ticks;

    if (!stamp || !delta) {
        return;
    }

    days = (LONG)stamp->ds_Days + (LONG)delta->ds_Days;
    minutes = (LONG)stamp->ds_Minute + (LONG)delta->ds_Minute;
    ticks = (LONG)stamp->ds_Tick + (LONG)delta->ds_Tick;

    minutes += ticks / 3000;
    ticks %= 3000;

    days += minutes / (24L * 60L);
    minutes %= (24L * 60L);

    stamp->ds_Days = days;
    stamp->ds_Minute = minutes;
    stamp->ds_Tick = ticks;
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

    if (!LoadSample(TARGET_HIT_FILE, &gHit)) {
        FreeSample(&gShot);
        return FALSE;
    }

    if (!InitVoice(&gShotVoice)) {
        FreeSample(&gShot);
        FreeSample(&gHit);
        return FALSE;
    }

    if (!InitVoice(&gHitVoice)) {
        CloseVoice(&gShotVoice);
        FreeSample(&gShot);
        FreeSample(&gHit);
        return FALSE;
    }

    gSoundInited = TRUE;
    gLastError = SOUND_OK;
    return TRUE;
}

void Sound_Shutdown(void) {
    StopVoice(&gShotVoice);
    StopVoice(&gHitVoice);
    CloseVoice(&gShotVoice);
    CloseVoice(&gHitVoice);
    FreeSample(&gShot);
    FreeSample(&gHit);
    ResetState();
    gLastError = SOUND_OK;
}

void Sound_Update(void) {
    struct DateStamp now;

    if (gSoundPaused) {
        return;
    }

    ReapVoice(&gShotVoice);
    ReapVoice(&gHitVoice);

    if (!gHitPending) {
        return;
    }

    DateStamp(&now);
    if (CompareDateStamp(&now, &gHitDueStamp) < 0) {
        return;
    }

    if (gHitVoice.playing) {
        StopVoice(&gHitVoice);
    }

    gHitPending = FALSE;
    StartVoiceSample(&gHitVoice, &gHit, SOUND_11KHZ_PERIOD, HIT_VOLUME, HIT_CYCLES);
}

void Sound_SetPaused(BOOL paused) {
    struct DateStamp now;
    struct DateStamp delta;

    if (!gSoundInited || paused == gSoundPaused) {
        return;
    }

    if (paused) {
        DateStamp(&gPauseStamp);
        gSoundPaused = TRUE;
        return;
    }

    DateStamp(&now);
    delta.ds_Days = now.ds_Days - gPauseStamp.ds_Days;
    delta.ds_Minute = now.ds_Minute - gPauseStamp.ds_Minute;
    delta.ds_Tick = now.ds_Tick - gPauseStamp.ds_Tick;

    while (delta.ds_Tick < 0) {
        delta.ds_Tick += 3000;
        delta.ds_Minute--;
    }

    while (delta.ds_Minute < 0) {
        delta.ds_Minute += 24L * 60L;
        delta.ds_Days--;
    }

    if (gHitPending) {
        AddDateStampDelta(&gHitDueStamp, &delta);
    }

    gSoundPaused = FALSE;
}

void Sound_PlayShot(void) {
    if (!gSoundInited || !gShotVoice.io || !gShot.data || gShot.length == 0) {
        return;
    }

    ReapVoice(&gShotVoice);
    if (gShotVoice.playing) {
        return;
    }

    StartVoiceSample(&gShotVoice, &gShot, SOUND_11KHZ_PERIOD, SHOT_VOLUME, SHOT_CYCLES);
}

void Sound_PlayHit(UWORD delayTicks) {
    struct DateStamp now;

    if (!gSoundInited || !gHitVoice.io || !gHit.data || gHit.length == 0) {
        return;
    }

    ReapVoice(&gHitVoice);

    if (gHitVoice.playing) {
        StopVoice(&gHitVoice);
    }

    DateStamp(&now);
    gHitDueStamp = now;
    AddTicksToDateStamp(&gHitDueStamp, delayTicks);
    gHitPending = TRUE;

    if (delayTicks == 0) {
        gHitPending = FALSE;
        StartVoiceSample(&gHitVoice, &gHit, SOUND_11KHZ_PERIOD, HIT_VOLUME, HIT_CYCLES);
    }
}
