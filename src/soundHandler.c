#include "soundHandler.h"
#include "assets.h"

#include <devices/audio.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <string.h>

/* Error codes */
#define SOUND_OK 0
#define SOUND_ERR_DEVICE 1
#define SOUND_ERR_SPEECH 2
#define SOUND_ERR_SAMPLE 3
#define SOUND_ERR_VOICE 4

/* Global flags */
static BOOL gAudioDeviceOpened = FALSE;
static struct IOAudio gAudioIO = {0};

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
#define SOUND_16KHZ_PERIOD 222
#define SOUND_22KHZ_PERIOD 161

#define SHOT_VOLUME 64
#define SHOT_CYCLES 1

#define HIT_VOLUME 64
#define HIT_CYCLES 1
#define LOOP_FOREVER_CYCLES 0

#define AUDIO_CH_0_RIGHT_SHOT 1
#define AUDIO_CH_1_LEFT_HIT 2
#define AUDIO_CH_2_LEFT_TITLE 4 // TODO: Rename this channel from TITLE to AMBIENT
#define AUDIO_CH_3_RIGHT_SPEECH 8

static AudioVoice gShotVoice = {NULL, NULL, FALSE, {AUDIO_CH_0_RIGHT_SHOT}};
static AudioVoice gHitVoice = {NULL, NULL, FALSE, {AUDIO_CH_1_LEFT_HIT}};
static AudioVoice gTitleVoice = {NULL, NULL, FALSE, {AUDIO_CH_2_LEFT_TITLE}};
static AudioVoice gSpeechVoice = {NULL, NULL, FALSE, {AUDIO_CH_3_RIGHT_SPEECH}};
static AudioVoice gDrumsVoice = {NULL, NULL, FALSE, {AUDIO_CH_0_RIGHT_SHOT}};

static Sample gShot = {NULL, 0};
static Sample gHit = {NULL, 0};
static Sample gTitleMusic = {NULL, 0};
static Sample gSpeechLoop = {NULL, 0};
static Sample gHiScoreFanfare = {NULL, 0};
static Sample gNarratorPrepareToFire = {NULL, 0};
static Sample gSpeechHit = {NULL, 0};
static Sample gSpeechExcellent = {NULL, 0};
static Sample gSpeechSuperb = {NULL, 0};
static Sample gSpeechWellDone = {NULL, 0};
static Sample gSpeechUnacceptable = {NULL, 0};

static BOOL gSoundInited = FALSE;
static BOOL gTitleMusicInited = FALSE;
static BOOL gSpeechLoopInited = FALSE;
static BOOL gHiScoreFanfareInited = FALSE;
static BOOL gTitleMusicAvailable = FALSE;
static BOOL gSpeechLoopAvailable = FALSE;
static BOOL gHiScoreFanfareAvailable = FALSE;
static BOOL gNarratorPrepareToFireInited = FALSE;
static BOOL gSpeechHitInited = FALSE;
static BOOL gSpeechHitAvailable = FALSE;
static BOOL gSpeechExcellentInited = FALSE;
static BOOL gSpeechSuperbInited = FALSE;
static BOOL gSpeechWellDoneInited = FALSE;
static BOOL gSpeechUnacceptableInited = FALSE;
static BOOL gSpeechVoiceInited = FALSE;
static BOOL gDrumsVoiceInited = FALSE;
static BOOL gSpeechLoopEnabled = FALSE;
static BOOL gHitPending = FALSE;
static UBYTE gHitPendingVolume = HIT_VOLUME;
static BOOL gSoundPaused = FALSE;
static struct DateStamp gHitDueStamp;
static struct DateStamp gPauseStamp;
static SoundError gLastError = SOUND_OK;

static void ResetState(void) {
    gSoundInited = FALSE;
    gHitPending = FALSE;
    gHitPendingVolume = HIT_VOLUME;
    gSoundPaused = FALSE;
    gShotVoice.playing = FALSE;
    gHitVoice.playing = FALSE;
    gDrumsVoice.playing = FALSE;
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

static BOOL EnsureSpeechVoice(void) {
    if (gSpeechVoiceInited) {
        return TRUE;
    }

    if (!InitVoice(&gSpeechVoice)) {
        return FALSE;
    }

    gSpeechVoiceInited = TRUE;
    return TRUE;
}

static BOOL EnsureDrumsVoice(void) {
    if (gDrumsVoiceInited) {
        return TRUE;
    }

    if (!InitVoice(&gDrumsVoice)) {
        return FALSE;
    }

    gDrumsVoiceInited = TRUE;
    return TRUE;
}

static void StartVoiceSample(AudioVoice *voice, const Sample *sample, UWORD period, UBYTE volume,
                             UBYTE cycles) {
    if (!voice || !voice->io || !sample || !sample->data || sample->length == 0) {
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

BOOL Sound_InitTitleMusic(void) {
    if (gTitleMusicInited) {
        gLastError = SOUND_OK;
        return TRUE;
    }

    gLastError = SOUND_OK;
    gTitleMusicAvailable = FALSE;

    if (!LoadSample(TITLE_MUSIC_FILE, &gTitleMusic)) {
        /* Optional asset: missing title music must not block the game. */
        gTitleMusicInited = TRUE;
        gLastError = SOUND_OK;
        return TRUE;
    }

    /* Keep the sample in memory, but allocate the Paula channel only while playing. */
    gTitleMusicAvailable = TRUE;
    gTitleMusicInited = TRUE;
    gLastError = SOUND_OK;
    return TRUE;
}

void Sound_PlayTitleMusic(void) {
    if (!gTitleMusicInited) {
        if (!Sound_InitTitleMusic()) {
            return;
        }
    }

    if (!gTitleMusicAvailable || !gTitleMusic.data || gTitleMusic.length == 0) {
        return;
    }

    if (!gTitleVoice.io) {
        if (!InitVoice(&gTitleVoice)) {
            return;
        }
    }

    ReapVoice(&gTitleVoice);

    if (gTitleVoice.playing) {
        StopVoice(&gTitleVoice);
    }

    StartVoiceSample(&gTitleVoice, &gTitleMusic, SOUND_11KHZ_PERIOD, 64, 1);
}

void Sound_StopTitleMusic(BOOL fadeOut) {
    (void)fadeOut;

    if (!gTitleMusicInited) {
        return;
    }

    ReapVoice(&gTitleVoice);

    if (gTitleVoice.playing) {
        StopVoice(&gTitleVoice);
    }

    /*
     * Title music is optional and only needed while the title screen is active.
     * Release both the Paula channel and the CHIP sample before entering later
     * screens/range code, so a present title track cannot reduce runtime CHIP
     * memory or leave CH2 in a stale state when other optional tracks are absent.
     */
    if (gTitleVoice.io) {
        CloseVoice(&gTitleVoice);
    }

    FreeSample(&gTitleMusic);
    gTitleMusicAvailable = FALSE;
    gTitleMusicInited = FALSE;
    gLastError = SOUND_OK;
}

void Sound_ShutdownTitleMusic(void) {
    Sound_StopTitleMusic(FALSE);
}

BOOL Sound_IsTitleMusicPlaying(void) {
    if (!gTitleMusicInited || !gTitleMusicAvailable || !gTitleVoice.io) {
        return FALSE;
    }

    ReapVoice(&gTitleVoice);
    return gTitleVoice.playing;
}

BOOL Sound_InitSpeechLoop(void) {
    if (gSpeechLoopInited) {
        gLastError = SOUND_OK;
        return TRUE;
    }

    gLastError = SOUND_OK;
    gSpeechLoopAvailable = FALSE;

    if (!LoadSample(DRUMS_LOOP_FILE, &gSpeechLoop)) {
        /* Optional asset: missing drums must not block the game. */
        gSpeechLoopInited = TRUE;
        gSpeechLoopEnabled = FALSE;
        gLastError = SOUND_OK;
        return TRUE;
    }

    /* Optional loop: allocate CH0 only when playback is actually requested. */
    gSpeechLoopAvailable = TRUE;
    gSpeechLoopInited = TRUE;
    gSpeechLoopEnabled = FALSE;
    gLastError = SOUND_OK;
    return TRUE;
}

void Sound_PlaySpeechLoop(void) {
    if (!gSpeechLoopInited) {
        if (!Sound_InitSpeechLoop()) {
            return;
        }
    }

    if (!gSpeechLoopAvailable || !gSpeechLoop.data || gSpeechLoop.length == 0) {
        gSpeechLoopEnabled = FALSE;
        return;
    }

    if (!EnsureDrumsVoice()) {
        return;
    }

    ReapVoice(&gDrumsVoice);

    gSpeechLoopEnabled = TRUE;

    if (!gDrumsVoice.playing) {
        StartVoiceSample(&gDrumsVoice, &gSpeechLoop, SOUND_11KHZ_PERIOD, 48, LOOP_FOREVER_CYCLES);
    }
}

void Sound_StopSpeechLoop(BOOL fadeOut) {
    (void)fadeOut;

    gSpeechLoopEnabled = FALSE;

    if (gDrumsVoiceInited) {
        ReapVoice(&gDrumsVoice);

        if (gDrumsVoice.playing) {
            StopVoice(&gDrumsVoice);
        }

        /*
         * DRUMS_LOOP_FILE is loaded once after leaving the title screen and
         * kept in CHIP RAM.  Only the Paula channel is released here so CH0
         * can be reused by shot samples on the firing range.
         */
        CloseVoice(&gDrumsVoice);
        gDrumsVoiceInited = FALSE;
    }

    gLastError = SOUND_OK;
}

void Sound_ShutdownSpeechLoop(void) {
    Sound_StopSpeechLoop(FALSE);

    if (gSpeechLoopInited) {
        FreeSample(&gSpeechLoop);
        gSpeechLoopAvailable = FALSE;
        gSpeechLoopInited = FALSE;
    }

    gLastError = SOUND_OK;
}

BOOL Sound_IsSpeechLoopPlaying(void) {
    if (!gSpeechLoopInited || !gSpeechLoopAvailable || !gDrumsVoiceInited) {
        return FALSE;
    }

    ReapVoice(&gDrumsVoice);
    return gDrumsVoice.playing;
}

BOOL Sound_InitHiScoreFanfare(void) {
    if (gHiScoreFanfareInited) {
        gLastError = SOUND_OK;
        return TRUE;
    }

    gLastError = SOUND_OK;
    gHiScoreFanfareAvailable = FALSE;

    if (!LoadSample(HISCORE_FANFARE_FILE, &gHiScoreFanfare)) {
        /* Optional asset: missing hi-score fanfare must not block the game. */
        gHiScoreFanfareInited = TRUE;
        gLastError = SOUND_OK;
        return TRUE;
    }

    /* Optional fanfare: allocate CH3 only when playback is actually requested. */
    gHiScoreFanfareAvailable = TRUE;
    gHiScoreFanfareInited = TRUE;
    gLastError = SOUND_OK;
    return TRUE;
}

void Sound_PlayHiScoreFanfare(void) {
    if (!gHiScoreFanfareInited) {
        if (!Sound_InitHiScoreFanfare()) {
            return;
        }
    }

    if (!gHiScoreFanfareAvailable || !gHiScoreFanfare.data || gHiScoreFanfare.length == 0) {
        return;
    }

    gSpeechLoopEnabled = FALSE;

    if (!EnsureSpeechVoice()) {
        return;
    }

    ReapVoice(&gSpeechVoice);

    if (gSpeechVoice.playing) {
        StopVoice(&gSpeechVoice);
    }

    StartVoiceSample(&gSpeechVoice, &gHiScoreFanfare, SOUND_11KHZ_PERIOD, 64, 1);
}

void Sound_StopHiScoreFanfare(BOOL fadeOut) {
    (void)fadeOut;

    if (gSpeechVoiceInited) {
        ReapVoice(&gSpeechVoice);

        if (gSpeechVoice.playing) {
            StopVoice(&gSpeechVoice);
        }
    }

    /* Fanfare is optional transition audio. Free the sample after use. */
    if (gHiScoreFanfareInited) {
        FreeSample(&gHiScoreFanfare);
        gHiScoreFanfareAvailable = FALSE;
        gHiScoreFanfareInited = FALSE;
    }

    gLastError = SOUND_OK;
}

void Sound_ShutdownHiScoreFanfare(void) {
    Sound_StopHiScoreFanfare(FALSE);
}

BOOL Sound_IsHiScoreFanfarePlaying(void) {
    if (!gHiScoreFanfareInited || !gHiScoreFanfareAvailable || !gSpeechVoiceInited) {
        return FALSE;
    }

    ReapVoice(&gSpeechVoice);
    return gSpeechVoice.playing;
}

BOOL Sound_InitNarratorPrepareToFire(void) {
    if (gNarratorPrepareToFireInited) {
        gLastError = SOUND_OK;
        return TRUE;
    }

    gLastError = SOUND_OK;

    if (!LoadSample(NARRATOR_PREPARE_TO_FIRE_FILE, &gNarratorPrepareToFire)) {
        /* Optional narrator cue: remember the attempt to avoid floppy I/O on Fire. */
        gNarratorPrepareToFireInited = TRUE;
        gLastError = SOUND_OK;
        return TRUE;
    }

    if (!EnsureSpeechVoice()) {
        FreeSample(&gNarratorPrepareToFire);
        gNarratorPrepareToFireInited = TRUE;
        gLastError = SOUND_OK;
        return TRUE;
    }

    gNarratorPrepareToFireInited = TRUE;
    gLastError = SOUND_OK;
    return TRUE;
}

void Sound_PlayNarratorPrepareToFire(void) {
    if (!gNarratorPrepareToFireInited) {
        if (!Sound_InitNarratorPrepareToFire()) {
            return;
        }
    }

    if (!gNarratorPrepareToFire.data || gNarratorPrepareToFire.length == 0) {
        return;
    }

    gSpeechLoopEnabled = FALSE;

    if (!EnsureSpeechVoice()) {
        return;
    }

    ReapVoice(&gSpeechVoice);

    if (gSpeechVoice.playing) {
        StopVoice(&gSpeechVoice);
    }

    StartVoiceSample(&gSpeechVoice, &gNarratorPrepareToFire, SOUND_22KHZ_PERIOD, 64, 1);
}

void Sound_StopNarratorPrepareToFire(BOOL fadeOut) {
    (void)fadeOut;

    if (!gNarratorPrepareToFireInited) {
        return;
    }

    ReapVoice(&gSpeechVoice);

    if (gSpeechVoice.playing) {
        StopVoice(&gSpeechVoice);
    }
}

void Sound_ShutdownNarratorPrepareToFire(void) {
    if (!gNarratorPrepareToFireInited) {
        return;
    }

    Sound_StopNarratorPrepareToFire(FALSE);
    FreeSample(&gNarratorPrepareToFire);
    gNarratorPrepareToFireInited = FALSE;

    if ((!gSpeechLoopInited || !gSpeechLoopAvailable) &&
        (!gHiScoreFanfareInited || !gHiScoreFanfareAvailable) &&
        (!gSpeechHitInited || !gSpeechHitAvailable) && !gSpeechExcellentInited &&
        !gSpeechSuperbInited && !gSpeechWellDoneInited && !gSpeechUnacceptableInited &&
        gSpeechVoiceInited) {
        CloseVoice(&gSpeechVoice);
        gSpeechVoiceInited = FALSE;
    }

    gLastError = SOUND_OK;
}

BOOL Sound_IsNarratorPrepareToFirePlaying(void) {
    if (!gNarratorPrepareToFireInited) {
        return FALSE;
    }

    ReapVoice(&gSpeechVoice);
    return gSpeechVoice.playing;
}

BOOL Sound_InitSpeechHit(void) {
    if (gSpeechHitInited) {
        gLastError = SOUND_OK;
        return TRUE;
    }

    gLastError = SOUND_OK;
    gSpeechHitAvailable = FALSE;

    /*
     * Speech_Hit is a gameplay cue.  It must be preloaded before entering the
     * firing range, just like Shot/TargetHit, so the first target hit never
     * performs floppy I/O during active gameplay.
     */
    if (!LoadSample(SPEECH_HIT_FILE, &gSpeechHit)) {
        /* Optional speech cue: remember the attempt to avoid retrying on hits. */
        gSpeechHitInited = TRUE;
        gLastError = SOUND_OK;
        return TRUE;
    }

    if (!gSpeechVoiceInited) {
        if (!InitVoice(&gSpeechVoice)) {
            FreeSample(&gSpeechHit);
            gSpeechHitInited = TRUE;
            gLastError = SOUND_OK;
            return TRUE;
        }

        gSpeechVoiceInited = TRUE;
    }

    gSpeechHitAvailable = TRUE;
    gSpeechHitInited = TRUE;
    gLastError = SOUND_OK;
    return TRUE;
}

void Sound_PlaySpeechHit(void) {
    if (!gSpeechHitInited) {
        if (!Sound_InitSpeechHit()) {
            return;
        }
    }

    if (!gSpeechHitAvailable || !gSpeechHit.data || gSpeechHit.length == 0 || !gSpeechVoiceInited) {
        return;
    }

    ReapVoice(&gSpeechVoice);

    if (gSpeechVoice.playing) {
        StopVoice(&gSpeechVoice);
    }

    StartVoiceSample(&gSpeechVoice, &gSpeechHit, SOUND_22KHZ_PERIOD, 64, 1);
}

void Sound_StopSpeechHit(BOOL fadeOut) {
    (void)fadeOut;

    if (!gSpeechHitInited) {
        return;
    }

    ReapVoice(&gSpeechVoice);

    if (gSpeechVoice.playing) {
        StopVoice(&gSpeechVoice);
    }
}

void Sound_ShutdownSpeechHit(void) {
    if (!gSpeechHitInited) {
        return;
    }

    Sound_StopSpeechHit(FALSE);
    FreeSample(&gSpeechHit);
    gSpeechHitAvailable = FALSE;
    gSpeechHitInited = FALSE;
}

BOOL Sound_InitSpeechExcellent(void) {
    if (gSpeechExcellentInited) {
        gLastError = SOUND_OK;
        return TRUE;
    }

    gLastError = SOUND_OK;

    if (!LoadSample(SPEECH_EXCELLENT_FILE, &gSpeechExcellent)) {
        return FALSE;
    }

    if (!EnsureSpeechVoice()) {
        FreeSample(&gSpeechExcellent);
        return FALSE;
    }

    gSpeechExcellentInited = TRUE;
    return TRUE;
}

void Sound_PlaySpeechExcellent(void) {
    if (!gSpeechExcellentInited) {
        if (!Sound_InitSpeechExcellent()) {
            return;
        }
    }

    if (!EnsureSpeechVoice()) {
        return;
    }

    ReapVoice(&gSpeechVoice);

    if (gSpeechVoice.playing) {
        StopVoice(&gSpeechVoice);
    }

    StartVoiceSample(&gSpeechVoice, &gSpeechExcellent, SOUND_22KHZ_PERIOD, 64, 1);
}

void Sound_StopSpeechExcellent(BOOL fadeOut) {
    (void)fadeOut;

    if (!gSpeechExcellentInited) {
        return;
    }

    ReapVoice(&gSpeechVoice);

    if (gSpeechVoice.playing) {
        StopVoice(&gSpeechVoice);
    }
}

void Sound_ShutdownSpeechExcellent(void) {
    if (!gSpeechExcellentInited) {
        return;
    }

    Sound_StopSpeechExcellent(FALSE);
    FreeSample(&gSpeechExcellent);
    gSpeechExcellentInited = FALSE;
}

BOOL Sound_InitSpeechSuperb(void) {
    if (gSpeechSuperbInited) {
        gLastError = SOUND_OK;
        return TRUE;
    }

    gLastError = SOUND_OK;

    if (!LoadSample(SPEECH_SUPERB_FILE, &gSpeechSuperb)) {
        return FALSE;
    }

    if (!EnsureSpeechVoice()) {
        FreeSample(&gSpeechSuperb);
        return FALSE;
    }

    gSpeechSuperbInited = TRUE;
    return TRUE;
}

void Sound_PlaySpeechSuperb(void) {
    if (!gSpeechSuperbInited) {
        if (!Sound_InitSpeechSuperb()) {
            return;
        }
    }

    if (!EnsureSpeechVoice()) {
        return;
    }

    ReapVoice(&gSpeechVoice);

    if (gSpeechVoice.playing) {
        StopVoice(&gSpeechVoice);
    }

    StartVoiceSample(&gSpeechVoice, &gSpeechSuperb, SOUND_22KHZ_PERIOD, 64, 1);
}

void Sound_StopSpeechSuperb(BOOL fadeOut) {
    (void)fadeOut;

    if (!gSpeechSuperbInited) {
        return;
    }

    ReapVoice(&gSpeechVoice);

    if (gSpeechVoice.playing) {
        StopVoice(&gSpeechVoice);
    }
}

void Sound_ShutdownSpeechSuperb(void) {
    if (!gSpeechSuperbInited) {
        return;
    }

    Sound_StopSpeechSuperb(FALSE);
    FreeSample(&gSpeechSuperb);
    gSpeechSuperbInited = FALSE;
}

BOOL Sound_InitSpeechWellDone(void) {
    if (gSpeechWellDoneInited) {
        gLastError = SOUND_OK;
        return TRUE;
    }

    gLastError = SOUND_OK;

    if (!LoadSample(SPEECH_WELL_DONE_FILE, &gSpeechWellDone)) {
        return FALSE;
    }

    if (!EnsureSpeechVoice()) {
        FreeSample(&gSpeechWellDone);
        return FALSE;
    }

    gSpeechWellDoneInited = TRUE;
    return TRUE;
}

void Sound_PlaySpeechWellDone(void) {
    if (!gSpeechWellDoneInited) {
        if (!Sound_InitSpeechWellDone()) {
            return;
        }
    }

    if (!EnsureSpeechVoice()) {
        return;
    }

    ReapVoice(&gSpeechVoice);

    if (gSpeechVoice.playing) {
        StopVoice(&gSpeechVoice);
    }

    StartVoiceSample(&gSpeechVoice, &gSpeechWellDone, SOUND_22KHZ_PERIOD, 64, 1);
}

void Sound_StopSpeechWellDone(BOOL fadeOut) {
    (void)fadeOut;

    if (!gSpeechWellDoneInited) {
        return;
    }

    ReapVoice(&gSpeechVoice);

    if (gSpeechVoice.playing) {
        StopVoice(&gSpeechVoice);
    }
}

void Sound_ShutdownSpeechWellDone(void) {
    if (!gSpeechWellDoneInited) {
        return;
    }

    Sound_StopSpeechWellDone(FALSE);
    FreeSample(&gSpeechWellDone);
    gSpeechWellDoneInited = FALSE;
}

BOOL Sound_InitSpeechUnacceptable(void) {
    if (gSpeechUnacceptableInited) {
        gLastError = SOUND_OK;
        return TRUE;
    }

    gLastError = SOUND_OK;

    if (!LoadSample(SPEECH_UNACCEPTABLE_FILE, &gSpeechUnacceptable)) {
        return FALSE;
    }

    if (!EnsureSpeechVoice()) {
        FreeSample(&gSpeechUnacceptable);
        return FALSE;
    }

    gSpeechUnacceptableInited = TRUE;
    return TRUE;
}

void Sound_PlaySpeechUnacceptable(void) {
    if (!gSpeechUnacceptableInited) {
        if (!Sound_InitSpeechUnacceptable()) {
            return;
        }
    }

    if (!EnsureSpeechVoice()) {
        return;
    }

    ReapVoice(&gSpeechVoice);

    if (gSpeechVoice.playing) {
        StopVoice(&gSpeechVoice);
    }

    StartVoiceSample(&gSpeechVoice, &gSpeechUnacceptable, SOUND_22KHZ_PERIOD, 64, 1);
}

void Sound_StopSpeechUnacceptable(BOOL fadeOut) {
    (void)fadeOut;

    if (!gSpeechUnacceptableInited) {
        return;
    }

    ReapVoice(&gSpeechVoice);

    if (gSpeechVoice.playing) {
        StopVoice(&gSpeechVoice);
    }
}

void Sound_ShutdownSpeechUnacceptable(void) {
    if (!gSpeechUnacceptableInited) {
        return;
    }

    Sound_StopSpeechUnacceptable(FALSE);
    FreeSample(&gSpeechUnacceptable);
    gSpeechUnacceptableInited = FALSE;
}

SoundError Sound_GetLastError(void) {
    return gLastError;
}

BOOL Sound_Init(void) {
    if (gSoundInited) {
        return TRUE;
    }

    ResetState();

    /* Open audio.device if it is not already open */
    if (!gAudioDeviceOpened) {
        if (OpenDevice((STRPTR) "audio.device", 0, (struct IORequest *)&gAudioIO, 0) != 0) {
            gLastError = SOUND_ERR_DEVICE;
            return FALSE;
        }
        gAudioDeviceOpened = TRUE;
    }

    /* Initialize common samples */
    if (!LoadSample(SHOT_FILE, &gShot) || !LoadSample(TARGET_HIT_FILE, &gHit)) {
        return FALSE;
    }

    if (!InitVoice(&gShotVoice) || !InitVoice(&gHitVoice)) {
        return FALSE;
    }

    /* Initialize narrator */
    if (!gSpeechVoiceInited) {
        if (!InitVoice(&gSpeechVoice)) {
            gLastError = SOUND_ERR_SPEECH;
            /* Do not return FALSE here - the remaining audio can still work */
        } else {
            gSpeechVoiceInited = TRUE;
        }
    }

    gSoundInited = TRUE;
    gLastError = SOUND_OK;
    return TRUE;
}

void Sound_Shutdown(void) {
    StopVoice(&gShotVoice);
    StopVoice(&gHitVoice);
    StopVoice(&gDrumsVoice);
    StopVoice(&gSpeechVoice);
    StopVoice(&gTitleVoice);

    CloseVoice(&gShotVoice);
    CloseVoice(&gHitVoice);

    if (gDrumsVoiceInited) {
        CloseVoice(&gDrumsVoice);
        gDrumsVoiceInited = FALSE;
    }

    FreeSample(&gShot);
    FreeSample(&gHit);

    /* We leave the audio.device open during a normal shutdown */
    ResetState();
    gLastError = SOUND_OK;
}

void Sound_Update(void) {
    struct DateStamp now;

    if (gSpeechLoopInited && gSpeechLoopAvailable && gDrumsVoiceInited) {
        ReapVoice(&gDrumsVoice);

        if (!gSoundPaused && gSpeechLoopEnabled && !gDrumsVoice.playing) {
            StartVoiceSample(&gDrumsVoice, &gSpeechLoop, SOUND_11KHZ_PERIOD, 48, LOOP_FOREVER_CYCLES);
        }
    }

    if (gSpeechVoiceInited) {
    }

    if (gSoundPaused) {
        return;
    }

    if (!gSoundInited) {
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
    StartVoiceSample(&gHitVoice, &gHit, SOUND_11KHZ_PERIOD, gHitPendingVolume, HIT_CYCLES);
    Sound_PlaySpeechHit();
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

void Sound_PlayHit(UWORD delayTicks, UBYTE volume) {
    struct DateStamp now;

    if (volume > HIT_VOLUME) {
        volume = HIT_VOLUME;
    }

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
    gHitPendingVolume = volume;
    gHitPending = TRUE;

    if (delayTicks == 0) {
        gHitPending = FALSE;
        StartVoiceSample(&gHitVoice, &gHit, SOUND_11KHZ_PERIOD, volume, HIT_CYCLES);
        Sound_PlaySpeechHit();
    }
}

void Sound_FullShutdown(void) {
    /* Stop and close regular voices */
    Sound_Shutdown();

    /* Narrator cleanup */
    if (gSpeechVoiceInited) {
        StopVoice(&gSpeechVoice);
        CloseVoice(&gSpeechVoice);
        gSpeechVoiceInited = FALSE;
    }

    /* Fully close audio.device */
    if (gAudioDeviceOpened) {
        CloseDevice((struct IORequest *)&gAudioIO);
        gAudioDeviceOpened = FALSE;
    }

    /* Forced reset of data structures */
    memset(&gSpeechVoice, 0, sizeof(gSpeechVoice));
    memset(&gAudioIO, 0, sizeof(gAudioIO));

    ResetState();

    /* Give the system a moment to reclaim audio channels */
    Delay(5); // ~0.1 seconds
}