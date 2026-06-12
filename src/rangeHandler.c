#include "rangeHandler.h"

#include <dos/dos.h>
#include <exec/types.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <string.h>

#include "bob.h"
#include "gfx.h"
#include "input.h"
#include "levelManager.h"
#include "soundHandler.h"
#include "targetScoring.h"
#include "targetsHandler.h"

extern BOOL Input_Left(void);
extern BOOL Input_Right(void);
extern BOOL Input_Up(void);
extern BOOL Input_Down(void);

#define FRONTSIGHT_RAW "gfx/FrontSight.raw"
#define FRONTSIGHT_MASK "gfx/FrontSight.mask"

#define REARSIGHT_RAW "gfx/RearSight.raw"
#define REARSIGHT_MASK "gfx/RearSight.mask"

#define FRONTSIGHT_W 83
#define FRONTSIGHT_H 79

#define REARSIGHT_W 115
#define REARSIGHT_H 115

#define SCR_W 320
#define SCR_H 256

#define OVERSCAN_X 41
#define OVERSCAN_X_EXTRA 16
#define OVERSCAN_X_TOTAL (OVERSCAN_X + OVERSCAN_X_EXTRA)
#define OVERSCAN_Y 16

#define RING_OFFSET_X (-16)
#define RING_OFFSET_Y (-51)

#define OCCL_REL_X (-6)
#define OCCL_REL_Y (REARSIGHT_H)
#define OCCL_W 124
#define OCCL_H 39

#define LEAD_MAX_PX 34
#define LEAD_MAX_FP (LEAD_MAX_PX * 256)

#define LEAD_FOLLOW_DIV 4
#define LEAD_DECAY_NUM 210
#define LEAD_DECAY_DEN 256
#define LEAD_STOP_FP (2 * 256)

#define DOS_TICKS_PER_SEC 50
#define SHOT_COOLDOWN_TICKS DOS_TICKS_PER_SEC

#define RECOIL_PIXELS 4
#define RECOIL_DOWN_TICKS 3
#define RECOIL_UP_TICKS 1
#define RECOIL_TOTAL_TICKS (RECOIL_UP_TICKS + RECOIL_DOWN_TICKS)
#define RECOIL_REAR_DELAY_TICKS 2

#define BREATH_PHASE_REST 0
#define BREATH_PHASE_UP 1
#define BREATH_PHASE_DOWN 2
#define BREATH_REST_TICKS (3 * DOS_TICKS_PER_SEC)
#define BREATH_MOVE_TICKS (2 * DOS_TICKS_PER_SEC)
#define BREATH_AMPLITUDE_PX 8
#define BREATH_LEAD_FP (2 * 256)

#define HAND_SWAY_PHASE_REST 0
#define HAND_SWAY_PHASE_LEFT 1
#define HAND_SWAY_PHASE_LEFT_RETURN 2
#define HAND_SWAY_PHASE_RIGHT 3
#define HAND_SWAY_PHASE_RIGHT_RETURN 4
#define HAND_SWAY_REST_TICKS (DOS_TICKS_PER_SEC / 2)
#define HAND_SWAY_MOVE_TICKS (DOS_TICKS_PER_SEC / 2)
#define HAND_SWAY_AMPLITUDE_PX 3
#define HAND_SWAY_LEAD_FP (3 * 256)

#define RELOAD_STATE_NONE 0
#define RELOAD_STATE_WAIT_PUSH 1
#define RELOAD_STATE_WAIT_PULL 2
#define RELOAD_STATE_FINISHING 3
#define RELOAD_FINISH_TICKS ((DOS_TICKS_PER_SEC * 5) / 2)
#define RELOAD_SPEECH_INITIAL_DELAY_TICKS (3 * DOS_TICKS_PER_SEC)
#define RELOAD_SPEECH_INTERVAL_TICKS (5 * DOS_TICKS_PER_SEC)

#define FRONT_AIM_X 41
#define FRONT_AIM_Y 10

#define RESULT_FLASH_TICKS 3
#define SCORE_FLASH_EXCELLENT_COLOR 9
#define SCORE_FLASH_GOOD_COLOR 10
#define SCORE_FLASH_AVERAGE_COLOR 30
#define SCORE_FLASH_BELOW_AVG_COLOR 29
#define SCORE_FLASH_POOR_COLOR 28
#define SCORE_FLASH_MISS_COLOR 24
#define RESULT_FLASH_THICKNESS 3

#define HUD_TEXT_X 11
#define HUD_TEXT_Y 11
#define HUD_RESULT_Y 24
#define HUD_QUALITY_Y 37
#define HUD_PAUSED_Y HUD_QUALITY_Y
#define HUD_FINAL_SCORE_Y (HUD_PAUSED_Y + 13)
#define HUD_ACCURACY_Y (HUD_FINAL_SCORE_Y + 13)
#define HUD_MAGAZINE_COUNT 2
#define HUD_MAX_POSSIBLE_SCORE (HUD_MAGAZINE_COUNT * HUD_MAGAZINE_SIZE * SCORE_EXCELLENT)
#define HUD_AMMO_Y HUD_TEXT_Y
#define HUD_TEXT_PEN 25
#define HUD_SHADOW_PEN 31
#define HUD_QUALITY_SHADOW_PEN 24
#define HUD_FONT_NAME "topaz.font"
#define HUD_FONT_SIZE 8
#define HUD_SHADOW_OFFSET_X 1
#define HUD_SHADOW_OFFSET_Y 1
#define HUD_MARGIN_RIGHT 11
#define ENDROUND_TITLE_Y 61

// TODO: For testing purposes only, set it to the 30rd in the final release
#define HUD_MAGAZINE_SIZE 5

#define HUD_AMMO_MAX (HUD_MAGAZINE_COUNT * HUD_MAGAZINE_SIZE)
#define HUD_AMMO_BLOCK_CHAR ((char)0xDB)
#define HUD_AMMO_BLOCK_COUNT HUD_MAGAZINE_COUNT

// TODO: For testing purposes and demo version only
#define TIME_BONUS_MAX 300
#define TIME_BONUS_ZERO_TIME 90

static const struct TextAttr gHudFontAttr = {HUD_FONT_NAME, HUD_FONT_SIZE, FS_NORMAL, FPF_ROMFONT};

static UWORD gHitCount = 0;

static void ResetHitCounter(void);
static UWORD CalculateAccuracyPercent(UWORD score);
static ULONG ElapsedSecondsSince(const struct DateStamp *start);
static ULONG ElapsedTicksSince(const struct DateStamp *start);
static void AddTicksToDateStamp(struct DateStamp *stamp, ULONG ticks);
static UWORD CalculateTimeBonus(UWORD totalTime, UWORD accuracyPercent);
static UWORD ScaleSummaryScore(UWORD score);

/* Record containing range state flags
 * TODO: Turn it into global pseudo-object gState */
typedef struct RangeSessionState {
    WORD ringX;
    WORD ringY;
    LONG ax;
    LONG ay;
    LONG vx;
    LONG vy;
    LONG leadX;
    LONG leadY;

    int prevDirX;
    int prevDirY;
    UWORD holdX;
    UWORD holdY;

    BOOL shotCooldownActive;
    BOOL shotNeedsRelease;
    BOOL recoilActive;
    UWORD recoilTick;
    struct DateStamp lastShotStamp;

    WORD rearRecoilHistory[RECOIL_REAR_DELAY_TICKS];
    WORD currentFrontRecoilY;
    WORD currentRearRecoilY;

    BOOL paused;
    BOOL shotTaken;
    BOOL lastShotHit;
    UBYTE lastShotScore;

    UWORD resultFlashTicks;
    UWORD resultFlashColor;

    UWORD ammoCount;
    UBYTE reloadState;
    BOOL reloadNeedsNeutral;
    struct DateStamp reloadSpeechStamp;
    BOOL reloadSpeechStampValid;
    BOOL reloadSpeechFirstPromptPending;
    struct DateStamp reloadFinishStamp;
    BOOL reloadFinishStampValid;
    UWORD sessionScore;

    BOOL showFinalScore;
    BOOL sessionComplete;
    BOOL roundEnding;
    BOOL levelCompleted;

    /* Frozen snapshot of the last shot, used by the end-of-round screen. */
    BOOL finalShotValid;
    BOOL finalShotHitSnap;
    UBYTE finalShotScoreSnap;

    struct DateStamp finalScoreStamp;
    BOOL finalScoreStampValid;
    struct DateStamp roundStartStamp;

    UBYTE breathPhase;
    WORD breathOffsetY;
    LONG breathLeadTargetY;
    struct DateStamp breathPhaseStamp;
    BOOL breathPhaseStampValid;

    UBYTE handSwayPhase;
    WORD handSwayOffsetX;
    WORD handSwayRearOffsetX;
    LONG handSwayLeadTargetX;
    struct DateStamp handSwayPhaseStamp;
    BOOL handSwayPhaseStampValid;
} RangeSessionState;

/* Mandatory function to reset and restore range state */
static void InitRangeSessionState(RangeSessionState *state, BOOL isNewGameSession) {
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(*state));

    state->ringX = (SCR_W - REARSIGHT_W) / 2;
    state->ringY = (SCR_H - REARSIGHT_H) / 2;
    state->ammoCount = HUD_MAGAZINE_COUNT * HUD_MAGAZINE_SIZE;
    state->reloadState = RELOAD_STATE_NONE;
    state->reloadNeedsNeutral = FALSE;
    state->reloadSpeechStampValid = FALSE;
    state->reloadSpeechFirstPromptPending = FALSE;
    state->reloadFinishStampValid = FALSE;
    state->sessionScore = 0;
    state->lastShotScore = SCORE_MISS;
    state->resultFlashColor = SCORE_FLASH_MISS_COLOR;
    state->paused = FALSE;
    state->showFinalScore = FALSE;
    state->sessionComplete = FALSE;
    state->roundEnding = FALSE;
    state->levelCompleted = FALSE;
    state->finalShotValid = FALSE;
    state->finalShotHitSnap = FALSE;
    state->finalShotScoreSnap = SCORE_MISS;
    state->breathPhase = BREATH_PHASE_REST;
    state->breathOffsetY = 0;
    state->breathLeadTargetY = 0;
    state->breathPhaseStampValid = FALSE;
    state->handSwayPhase = HAND_SWAY_PHASE_REST;
    state->handSwayOffsetX = 0;
    state->handSwayRearOffsetX = 0;
    state->handSwayLeadTargetX = 0;
    state->handSwayPhaseStampValid = FALSE;

    Input_ResetState();
    TargetsHandler_Reset();

    if (isNewGameSession) {
        ResetHitCounter();
        TargetScoring_Reset();

        state->lastShotHit = FALSE;
        state->lastShotScore = SCORE_MISS;
        state->resultFlashTicks = 0;
        state->resultFlashColor = SCORE_FLASH_MISS_COLOR;
        state->finalShotValid = FALSE;
        state->finalShotHitSnap = FALSE;
        state->finalShotScoreSnap = SCORE_MISS;
    }

    IS_NEW_GAME_SESSION = FALSE;
}

static void ResetHitCounter(void) {
    gHitCount = 0;
}

static ULONG ElapsedSecondsSince(const struct DateStamp *start) {
    struct DateStamp now;
    LONG totalTicks;

    DateStamp(&now);

    totalTicks = (now.ds_Days - start->ds_Days) * 24 * 60 * 60 * DOS_TICKS_PER_SEC;
    totalTicks += (now.ds_Minute - start->ds_Minute) * 60 * DOS_TICKS_PER_SEC;
    totalTicks += (now.ds_Tick - start->ds_Tick);

    if (totalTicks <= 0) {
        return 0;
    }

    return (ULONG)(totalTicks / DOS_TICKS_PER_SEC);
}

static void StartBreathPhase(RangeSessionState *state, UBYTE phase) {
    state->breathPhase = phase;
    state->breathPhaseStampValid = TRUE;
    DateStamp(&state->breathPhaseStamp);

    if (phase == BREATH_PHASE_REST) {
        state->breathOffsetY = 0;
        state->breathLeadTargetY = 0;
    } else if (phase == BREATH_PHASE_UP) {
        state->breathOffsetY = 0;
        state->breathLeadTargetY = -BREATH_LEAD_FP;
    } else {
        state->breathOffsetY = -BREATH_AMPLITUDE_PX;
        state->breathLeadTargetY = BREATH_LEAD_FP;
    }
}

static WORD UpdateBreathing(RangeSessionState *state, BOOL joystickMoving) {
    ULONG elapsed;
    WORD oldOffset;
    WORD newOffset;

    if (!state) {
        return 0;
    }

    if (joystickMoving) {
        state->breathPhase = BREATH_PHASE_REST;
        state->breathOffsetY = 0;
        state->breathLeadTargetY = 0;
        state->breathPhaseStampValid = FALSE;
        return 0;
    }

    if (!state->breathPhaseStampValid) {
        StartBreathPhase(state, BREATH_PHASE_REST);
        return 0;
    }

    oldOffset = state->breathOffsetY;
    newOffset = oldOffset;
    elapsed = ElapsedTicksSince(&state->breathPhaseStamp);

    if (state->breathPhase == BREATH_PHASE_REST) {
        newOffset = 0;
        state->breathLeadTargetY = 0;

        if (elapsed >= BREATH_REST_TICKS) {
            StartBreathPhase(state, BREATH_PHASE_UP);
            return 0;
        }
    } else if (state->breathPhase == BREATH_PHASE_UP) {
        state->breathLeadTargetY = -BREATH_LEAD_FP;

        if (elapsed >= BREATH_MOVE_TICKS) {
            newOffset = -BREATH_AMPLITUDE_PX;
            state->breathOffsetY = newOffset;
            StartBreathPhase(state, BREATH_PHASE_DOWN);
            return (WORD)(newOffset - oldOffset);
        } else {
            newOffset = (WORD) - (((LONG)elapsed * BREATH_AMPLITUDE_PX) / BREATH_MOVE_TICKS);
        }
    } else {
        state->breathLeadTargetY = BREATH_LEAD_FP;

        if (elapsed >= BREATH_MOVE_TICKS) {
            newOffset = 0;
            state->breathOffsetY = newOffset;
            StartBreathPhase(state, BREATH_PHASE_REST);
            return (WORD)(newOffset - oldOffset);
        } else {
            newOffset = (WORD)(-BREATH_AMPLITUDE_PX +
                               (((LONG)elapsed * BREATH_AMPLITUDE_PX) / BREATH_MOVE_TICKS));
        }
    }

    state->breathOffsetY = newOffset;
    return (WORD)(newOffset - oldOffset);
}

static WORD HandSwayRearOffsetFromSightOffset(WORD sightOffsetX) {
    if (sightOffsetX <= -2) {
        return -1;
    }

    if (sightOffsetX >= 2) {
        return 1;
    }

    return 0;
}

static void StartHandSwayPhase(RangeSessionState *state, UBYTE phase) {
    state->handSwayPhase = phase;
    state->handSwayPhaseStampValid = TRUE;
    DateStamp(&state->handSwayPhaseStamp);

    if (phase == HAND_SWAY_PHASE_REST) {
        state->handSwayOffsetX = 0;
        state->handSwayLeadTargetX = 0;
    } else if (phase == HAND_SWAY_PHASE_LEFT) {
        state->handSwayOffsetX = 0;
        state->handSwayLeadTargetX = -HAND_SWAY_LEAD_FP;
    } else if (phase == HAND_SWAY_PHASE_LEFT_RETURN) {
        state->handSwayOffsetX = -HAND_SWAY_AMPLITUDE_PX;
        state->handSwayLeadTargetX = HAND_SWAY_LEAD_FP;
    } else if (phase == HAND_SWAY_PHASE_RIGHT) {
        state->handSwayOffsetX = 0;
        state->handSwayLeadTargetX = HAND_SWAY_LEAD_FP;
    } else {
        state->handSwayOffsetX = HAND_SWAY_AMPLITUDE_PX;
        state->handSwayLeadTargetX = -HAND_SWAY_LEAD_FP;
    }

    state->handSwayRearOffsetX = HandSwayRearOffsetFromSightOffset(state->handSwayOffsetX);
}

static WORD UpdateHandSway(RangeSessionState *state, BOOL joystickMoving) {
    ULONG elapsed;
    WORD oldOffset;
    WORD newOffset;
    WORD oldRearOffset;
    WORD newRearOffset;

    if (!state) {
        return 0;
    }

    if (joystickMoving) {
        state->handSwayPhase = HAND_SWAY_PHASE_REST;
        state->handSwayOffsetX = 0;
        state->handSwayRearOffsetX = 0;
        state->handSwayLeadTargetX = 0;
        state->handSwayPhaseStampValid = FALSE;
        return 0;
    }

    if (!state->handSwayPhaseStampValid) {
        StartHandSwayPhase(state, HAND_SWAY_PHASE_REST);
        return 0;
    }

    oldOffset = state->handSwayOffsetX;
    oldRearOffset = state->handSwayRearOffsetX;
    newOffset = oldOffset;
    elapsed = ElapsedTicksSince(&state->handSwayPhaseStamp);

    if (state->handSwayPhase == HAND_SWAY_PHASE_REST) {
        newOffset = 0;
        state->handSwayLeadTargetX = 0;

        if (elapsed >= HAND_SWAY_REST_TICKS) {
            StartHandSwayPhase(state, HAND_SWAY_PHASE_LEFT);
            return 0;
        }
    } else if (state->handSwayPhase == HAND_SWAY_PHASE_LEFT) {
        state->handSwayLeadTargetX = -HAND_SWAY_LEAD_FP;

        if (elapsed >= HAND_SWAY_MOVE_TICKS) {
            newOffset = -HAND_SWAY_AMPLITUDE_PX;
            state->handSwayOffsetX = newOffset;
            StartHandSwayPhase(state, HAND_SWAY_PHASE_LEFT_RETURN);
            return (WORD)(state->handSwayRearOffsetX - oldRearOffset);
        } else {
            newOffset = (WORD) - (((LONG)elapsed * HAND_SWAY_AMPLITUDE_PX) / HAND_SWAY_MOVE_TICKS);
        }
    } else if (state->handSwayPhase == HAND_SWAY_PHASE_LEFT_RETURN) {
        state->handSwayLeadTargetX = HAND_SWAY_LEAD_FP;

        if (elapsed >= HAND_SWAY_MOVE_TICKS) {
            newOffset = 0;
            state->handSwayOffsetX = newOffset;
            StartHandSwayPhase(state, HAND_SWAY_PHASE_RIGHT);
            return (WORD)(state->handSwayRearOffsetX - oldRearOffset);
        } else {
            newOffset = (WORD)(-HAND_SWAY_AMPLITUDE_PX +
                               (((LONG)elapsed * HAND_SWAY_AMPLITUDE_PX) / HAND_SWAY_MOVE_TICKS));
        }
    } else if (state->handSwayPhase == HAND_SWAY_PHASE_RIGHT) {
        state->handSwayLeadTargetX = HAND_SWAY_LEAD_FP;

        if (elapsed >= HAND_SWAY_MOVE_TICKS) {
            newOffset = HAND_SWAY_AMPLITUDE_PX;
            state->handSwayOffsetX = newOffset;
            StartHandSwayPhase(state, HAND_SWAY_PHASE_RIGHT_RETURN);
            return (WORD)(state->handSwayRearOffsetX - oldRearOffset);
        } else {
            newOffset = (WORD)(((LONG)elapsed * HAND_SWAY_AMPLITUDE_PX) / HAND_SWAY_MOVE_TICKS);
        }
    } else {
        state->handSwayLeadTargetX = -HAND_SWAY_LEAD_FP;

        if (elapsed >= HAND_SWAY_MOVE_TICKS) {
            newOffset = 0;
            state->handSwayOffsetX = newOffset;
            StartHandSwayPhase(state, HAND_SWAY_PHASE_REST);
            return (WORD)(state->handSwayRearOffsetX - oldRearOffset);
        } else {
            newOffset = (WORD)(HAND_SWAY_AMPLITUDE_PX -
                               (((LONG)elapsed * HAND_SWAY_AMPLITUDE_PX) / HAND_SWAY_MOVE_TICKS));
        }
    }

    state->handSwayOffsetX = newOffset;
    newRearOffset = HandSwayRearOffsetFromSightOffset(newOffset);
    state->handSwayRearOffsetX = newRearOffset;
    return (WORD)(newRearOffset - oldRearOffset);
}

static UWORD CalculateTimeBonus(UWORD totalTime, UWORD accuracyPercent) {
    UWORD raw;

    if (accuracyPercent == 0 || totalTime >= TIME_BONUS_ZERO_TIME) {
        return 0;
    }

    raw = (UWORD)(((TIME_BONUS_ZERO_TIME - totalTime) * TIME_BONUS_MAX) / TIME_BONUS_ZERO_TIME);

    return (UWORD)(((ULONG)raw * accuracyPercent) / 100);
}

static UWORD ScaleSummaryScore(UWORD score) {
    ULONG scaled = (ULONG)score * 20UL;

    if (scaled > 65535UL) {
        return 65535;
    }

    return (UWORD)scaled;
}

static void RegisterHit(void) {
    if (gHitCount < 65535) {
        gHitCount++;
    }
}

static UWORD GetHitCount(void) {
    return gHitCount;
}

static UWORD BuildHitCounterText(char *buf, UWORD value) {
    char digits[5];
    UWORD digitCount = 0;
    UWORD pos = 0;
    UWORD i;
    UWORD temp = value;

    buf[pos++] = 'H';
    buf[pos++] = 'I';
    buf[pos++] = 'T';
    buf[pos++] = 'S';
    buf[pos++] = ':';
    buf[pos++] = ' ';

    if (temp == 0) {
        buf[pos++] = '0';
        buf[pos] = '\0';
        return pos;
    }

    while (temp > 0 && digitCount < 5) {
        digits[digitCount++] = (char)('0' + (temp % 10));
        temp /= 10;
    }

    for (i = 0; i < digitCount; i++) {
        buf[pos++] = digits[digitCount - 1 - i];
    }

    buf[pos] = '\0';
    return pos;
}

static UWORD BuildAccuracyText(char *buf, UWORD value) {
    static const char prefix[] = "ACCURACY: ";
    char digits[3];
    UWORD digitCount = 0;
    UWORD pos = 0;
    UWORD i;
    UWORD temp = value;

    for (i = 0; i < (sizeof(prefix) - 1); i++) {
        buf[pos++] = prefix[i];
    }

    if (temp == 0) {
        buf[pos++] = '0';
        buf[pos++] = '%';
        buf[pos] = '\0';
        return pos;
    }

    while (temp > 0 && digitCount < 3) {
        digits[digitCount++] = (char)('0' + (temp % 10));
        temp /= 10;
    }

    for (i = 0; i < digitCount; i++) {
        buf[pos++] = digits[digitCount - 1 - i];
    }

    buf[pos++] = '%';
    buf[pos] = '\0';
    return pos;
}

static UWORD CalculateAccuracyPercent(UWORD score) {
    ULONG percent = ((ULONG)score * 100UL) / (ULONG)HUD_MAX_POSSIBLE_SCORE;

    if (percent > 100UL) {
        percent = 100UL;
    }

    return (UWORD)percent;
}

static UWORD TextLen(const char *text) {
    UWORD len = 0;

    if (!text) {
        return 0;
    }

    while (text[len] != '\0') {
        len++;
    }

    return len;
}

static void DebugBeepError(SoundError err) {
    UWORD count = 0;
    UWORD i;

    switch (err) {
        case SOUND_ERR_PORT:
            count = 1;
            break;
        case SOUND_ERR_IOREQ:
            count = 2;
            break;
        case SOUND_ERR_OPENDEVICE:
            count = 3;
            break;
        case SOUND_ERR_OPENFILE:
            count = 4;
            break;
        case SOUND_ERR_FILESIZE:
            count = 5;
            break;
        case SOUND_ERR_ALLOCMEM:
            count = 6;
            break;
        case SOUND_ERR_READFILE:
            count = 7;
            break;
        default:
            count = 1;
            break;
    }

    for (i = 0; i < count; i++) {
        Delay(8);
    }
}

static ULONG ElapsedTicksSince(const struct DateStamp *start) {
    struct DateStamp now;
    LONG dd;
    LONG dm;
    LONG dt;
    LONG total;

    DateStamp(&now);

    dd = (LONG)now.ds_Days - (LONG)start->ds_Days;
    dm = (LONG)now.ds_Minute - (LONG)start->ds_Minute;
    dt = (LONG)now.ds_Tick - (LONG)start->ds_Tick;

    total = dd * (24L * 60L * 60L * DOS_TICKS_PER_SEC) + dm * (60L * DOS_TICKS_PER_SEC) + dt;

    if (total < 0) {
        total = 0;
    }

    return (ULONG)total;
}

static void AddTicksToDateStamp(struct DateStamp *stamp, ULONG ticks) {
    ULONG ticksPerMinute = (ULONG)(60L * DOS_TICKS_PER_SEC);
    ULONG minutesPerDay = (ULONG)(24L * 60L);
    ULONG totalTicks;
    ULONG extraMinutes;
    ULONG totalMinutes;

    if (!stamp || ticks == 0) {
        return;
    }

    totalTicks = (ULONG)stamp->ds_Tick + ticks;
    extraMinutes = totalTicks / ticksPerMinute;
    stamp->ds_Tick = (LONG)(totalTicks % ticksPerMinute);

    totalMinutes = (ULONG)stamp->ds_Minute + extraMinutes;
    stamp->ds_Days += (LONG)(totalMinutes / minutesPerDay);
    stamp->ds_Minute = (LONG)(totalMinutes % minutesPerDay);
}

static void StartReloadPrompt(RangeSessionState *state) {
    if (!state) {
        return;
    }

    state->reloadState = RELOAD_STATE_WAIT_PUSH;
    state->reloadNeedsNeutral = TRUE;
    DateStamp(&state->reloadSpeechStamp);
    state->reloadSpeechStampValid = TRUE;
    state->reloadSpeechFirstPromptPending = TRUE;
}

static void CompleteReloadPrompt(RangeSessionState *state) {
    if (!state) {
        return;
    }

    state->reloadState = RELOAD_STATE_NONE;
    state->reloadNeedsNeutral = TRUE;
    state->reloadSpeechStampValid = FALSE;
    state->reloadSpeechFirstPromptPending = FALSE;
    state->reloadFinishStampValid = FALSE;
    /* Do not stop Speech_Reload here. If the instructor has already started
     * saying "Reload! Reload!", let the sample finish naturally. Any
     * Speech_Hit cue that happens during this short window is intentionally
     * skipped by the speech channel priority logic.
     */
}

static void StartReloadFinishing(RangeSessionState *state) {
    if (!state) {
        return;
    }

    state->reloadState = RELOAD_STATE_FINISHING;
    state->reloadNeedsNeutral = TRUE;
    state->reloadSpeechStampValid = FALSE;
    state->reloadSpeechFirstPromptPending = FALSE;
    DateStamp(&state->reloadFinishStamp);
    state->reloadFinishStampValid = TRUE;
}

static void UpdateReloadFinishing(RangeSessionState *state) {
    if (!state || state->reloadState != RELOAD_STATE_FINISHING) {
        return;
    }

    if (!state->reloadFinishStampValid) {
        DateStamp(&state->reloadFinishStamp);
        state->reloadFinishStampValid = TRUE;
        return;
    }

    if (ElapsedTicksSince(&state->reloadFinishStamp) >= RELOAD_FINISH_TICKS) {
        CompleteReloadPrompt(state);
    }
}

static void UpdateReloadSpeech(RangeSessionState *state) {
    ULONG elapsed;
    ULONG delayTicks;

    if (!state || (state->reloadState != RELOAD_STATE_WAIT_PUSH &&
                   state->reloadState != RELOAD_STATE_WAIT_PULL)) {
        return;
    }

    if (!state->reloadSpeechStampValid) {
        DateStamp(&state->reloadSpeechStamp);
        state->reloadSpeechStampValid = TRUE;
        state->reloadSpeechFirstPromptPending = TRUE;
        return;
    }

    elapsed = ElapsedTicksSince(&state->reloadSpeechStamp);
    delayTicks = state->reloadSpeechFirstPromptPending ? RELOAD_SPEECH_INITIAL_DELAY_TICKS
                                                       : RELOAD_SPEECH_INTERVAL_TICKS;

    if (elapsed >= delayTicks) {
        Sound_PlaySpeechReload();
        DateStamp(&state->reloadSpeechStamp);
        state->reloadSpeechFirstPromptPending = FALSE;
    }
}

static BOOL ShotCooldownReady(BOOL active, const struct DateStamp *lastShotStamp) {
    if (!active) {
        return TRUE;
    }

    return (ElapsedTicksSince(lastShotStamp) >= SHOT_COOLDOWN_TICKS) ? TRUE : FALSE;
}

static void MarkShotFired(BOOL *active, struct DateStamp *lastShotStamp) {
    DateStamp(lastShotStamp);
    *active = TRUE;
}

static BOOL IntersectRect(WORD ax, WORD ay, WORD aw, WORD ah, WORD bx, WORD by, WORD bw, WORD bh,
                          WORD *outX, WORD *outY, WORD *outW, WORD *outH) {
    WORD x1 = (ax > bx) ? ax : bx;
    WORD y1 = (ay > by) ? ay : by;
    WORD x2 = ((ax + aw) < (bx + bw)) ? (ax + aw) : (bx + bw);
    WORD y2 = ((ay + ah) < (by + bh)) ? (ay + ah) : (by + bh);
    WORD w = (WORD)(x2 - x1);
    WORD h = (WORD)(y2 - y1);

    if (w <= 0 || h <= 0) {
        return FALSE;
    }

    *outX = x1;
    *outY = y1;
    *outW = w;
    *outH = h;
    return TRUE;
}

static void DrawMaskedClipped(const struct BitMap *srcBm, PLANEPTR maskPlane,
                              struct RastPort *dstRP, WORD dstX, WORD dstY, WORD srcW, WORD srcH) {
    WORD sx = 0;
    WORD sy = 0;
    WORD w = srcW;
    WORD h = srcH;
    WORD dx = dstX;
    WORD dy = dstY;

    if (!srcBm || !maskPlane || !dstRP || !dstRP->BitMap) {
        return;
    }

    if (dx < 0) {
        sx = (WORD)(-dx);
        w = (WORD)(w - sx);
        dx = 0;
    }

    if (dy < 0) {
        sy = (WORD)(-dy);
        h = (WORD)(h - sy);
        dy = 0;
    }

    if ((dx + w) > SCR_W) {
        w = (WORD)(SCR_W - dx);
    }

    if ((dy + h) > SCR_H) {
        h = (WORD)(SCR_H - dy);
    }

    if (w <= 0 || h <= 0) {
        return;
    }

    BltMaskBitMapRastPort((struct BitMap *)srcBm, sx, sy, dstRP, dx, dy, w, h, 0xE0, maskPlane);
    WaitBlit();
}

static WORD RecoilOffsetY(BOOL *active, UWORD *tick) {
    WORD offset = 0;

    if (!*active) {
        return 0;
    }

    if (*tick < RECOIL_UP_TICKS) {
        offset = (WORD)(*tick + 1);

        if (offset > RECOIL_PIXELS) {
            offset = RECOIL_PIXELS;
        }
    } else if (*tick < RECOIL_TOTAL_TICKS) {
        UWORD downTick = (UWORD)(*tick - RECOIL_UP_TICKS);
        offset = (WORD)(RECOIL_PIXELS - (downTick + 1));

        if (offset < 0) {
            offset = 0;
        }
    }

    (*tick)++;

    if (*tick >= RECOIL_TOTAL_TICKS) {
        *tick = 0;
        *active = FALSE;
    }

    return (WORD)(-offset);
}

static WORD RearRecoilOffsetY(WORD frontRecoilY, WORD *history) {
    WORD delayed = history[0];
    UWORD i;

    for (i = 0; i < (RECOIL_REAR_DELAY_TICKS - 1); i++) {
        history[i] = history[i + 1];
    }

    history[RECOIL_REAR_DELAY_TICKS - 1] = frontRecoilY;
    return delayed;
}

static void DrawResultFlash(struct RastPort *rp, UWORD colorIndex) {
    if (!rp) {
        return;
    }

    SetAPen(rp, colorIndex);
    RectFill(rp, 0, 0, SCR_W - 1, RESULT_FLASH_THICKNESS - 1);
    RectFill(rp, 0, SCR_H - RESULT_FLASH_THICKNESS, SCR_W - 1, SCR_H - 1);
    RectFill(rp, 0, 0, RESULT_FLASH_THICKNESS - 1, SCR_H - 1);
    RectFill(rp, SCR_W - RESULT_FLASH_THICKNESS, 0, SCR_W - 1, SCR_H - 1);
}

static void DrawTextWithShadowEx(struct RastPort *rp, struct TextFont *font, WORD x, WORD y,
                                 UWORD pen, UWORD shadowColor, const char *text, UWORD len) {
    if (!rp || !text || len == 0) {
        return;
    }

    if (font) {
        SetFont(rp, font);
    }

    SetDrMd(rp, JAM1);

    SetAPen(rp, shadowColor);
    Move(rp, x + HUD_SHADOW_OFFSET_X, y + HUD_SHADOW_OFFSET_Y + rp->TxBaseline);
    Text(rp, (STRPTR)text, len);

    SetAPen(rp, pen);
    Move(rp, x, y + rp->TxBaseline);
    Text(rp, (STRPTR)text, len);
}

static void DrawTextWithShadow(struct RastPort *rp, struct TextFont *font, WORD x, WORD y,
                               UWORD pen, const char *text, UWORD len) {
    DrawTextWithShadowEx(rp, font, x, y, pen, HUD_SHADOW_PEN, text, len);
}

static void DrawHitCounter(struct RastPort *rp, struct TextFont *font) {
    char text[16];
    UWORD len;

    len = BuildHitCounterText(text, GetHitCount());
    DrawTextWithShadow(rp, font, HUD_TEXT_X, HUD_TEXT_Y, HUD_TEXT_PEN, text, len);
}

static void DrawAmmoBlocks(struct RastPort *rp, struct TextFont *font, UWORD ammoCount) {
    char text[(HUD_AMMO_BLOCK_COUNT * 3) + 1];
    UWORD blocksVisible;
    UWORD i;
    UWORD pos = 0;
    WORD x;
    WORD width;

    if (!rp) {
        return;
    }

    if (ammoCount > HUD_AMMO_MAX) {
        ammoCount = HUD_AMMO_MAX;
    }

    blocksVisible = (UWORD)((ammoCount + (HUD_MAGAZINE_SIZE - 1)) / HUD_MAGAZINE_SIZE);

    for (i = 0; i < HUD_AMMO_BLOCK_COUNT; i++) {
        text[pos++] = '[';
        text[pos++] = (i >= (HUD_AMMO_BLOCK_COUNT - blocksVisible)) ? '\x7F' : ' ';
        text[pos++] = ']';
    }

    text[pos] = '\0';

    if (font) {
        SetFont(rp, font);
    }

    width = TextLength(rp, (STRPTR)text, pos);
    x = (WORD)(SCR_W - HUD_MARGIN_RIGHT - width);

    DrawTextWithShadow(rp, font, x, HUD_AMMO_Y, HUD_TEXT_PEN, text, pos);
}

static void DrawLastShotResult(struct RastPort *rp, struct TextFont *font, BOOL shotTaken,
                               BOOL lastShotHit) {
    static const char gHitText[] = "HIT";
    static const char gMissText[] = "MISS";
    const char *text;
    UWORD len;

    if (!shotTaken) {
        return;
    }

    text = lastShotHit ? gHitText : gMissText;
    len = lastShotHit ? 3 : 4;
    DrawTextWithShadow(rp, font, HUD_TEXT_X, HUD_RESULT_Y, HUD_TEXT_PEN, text, len);
}

static const char *GetScoreText(UBYTE score) {
    switch (score) {
        case SCORE_EXCELLENT:
            return "EXCELLENT";
        case SCORE_GOOD:
            return "GOOD";
        case SCORE_AVERAGE:
            return "AVERAGE";
        case SCORE_BELOW_AVG:
            return "BELOW AVG";
        case SCORE_POOR:
            return "POOR";
        default:
            return NULL;
    }
}

static UWORD GetScoreTextPen(UBYTE score) {
    switch (score) {
        case SCORE_EXCELLENT:
            return SCORE_FLASH_EXCELLENT_COLOR;
        case SCORE_GOOD:
            return SCORE_FLASH_GOOD_COLOR;
        case SCORE_AVERAGE:
            return SCORE_FLASH_AVERAGE_COLOR;
        case SCORE_BELOW_AVG:
            return SCORE_FLASH_BELOW_AVG_COLOR;
        case SCORE_POOR:
            return SCORE_FLASH_POOR_COLOR;
        default:
            return HUD_TEXT_PEN;
    }
}

static UWORD GetFlashColorForScore(UBYTE score) {
    switch (score) {
        case SCORE_EXCELLENT:
            return SCORE_FLASH_EXCELLENT_COLOR;
        case SCORE_GOOD:
            return SCORE_FLASH_GOOD_COLOR;
        case SCORE_AVERAGE:
            return SCORE_FLASH_AVERAGE_COLOR;
        case SCORE_BELOW_AVG:
            return SCORE_FLASH_BELOW_AVG_COLOR;
        case SCORE_POOR:
            return SCORE_FLASH_POOR_COLOR;
        case SCORE_MISS:
        default:
            return SCORE_FLASH_MISS_COLOR;
    }
}

static void DrawShotQuality(struct RastPort *rp, struct TextFont *font, BOOL shotTaken,
                            BOOL lastShotHit, UBYTE lastShotScore) {
    const char *text;
    UWORD len;

    if (!shotTaken || !lastShotHit) {
        return;
    }

    text = GetScoreText(lastShotScore);
    len = TextLen(text);

    if (len == 0) {
        return;
    }

    DrawTextWithShadowEx(rp, font, HUD_TEXT_X, HUD_QUALITY_Y, GetScoreTextPen(lastShotScore),
                         HUD_QUALITY_SHADOW_PEN, text, len);
}

static void DrawCenterStatusText(struct RastPort *rp, struct TextFont *font, BOOL paused,
                                 BOOL showFinalScore) {
    static const char gPausedText[] = "PAUSED";
    static const char gSummaryText[] = "SUMMARY";
    const char *text;
    UWORD len;
    WORD x;
    WORD width;

    if (!rp || (!paused && !showFinalScore)) {
        return;
    }

    text = showFinalScore ? gSummaryText : gPausedText;

    if (font) {
        SetFont(rp, font);
    }

    len = TextLen(text);
    width = TextLength(rp, (STRPTR)text, len);
    x = (WORD)((SCR_W - width) / 2);
    DrawTextWithShadow(rp, font, x, HUD_PAUSED_Y, HUD_TEXT_PEN, text, len);
}

static void DrawReloadLine(struct RastPort *rp, struct TextFont *font, WORD y, const char *text) {
    UWORD len;
    WORD x;
    WORD width;

    if (!rp || !text) {
        return;
    }

    if (font) {
        SetFont(rp, font);
    }

    len = TextLen(text);
    width = TextLength(rp, (STRPTR)text, len);
    x = (WORD)((SCR_W - width) / 2);
    DrawTextWithShadowEx(rp, font, x, y, SCORE_FLASH_BELOW_AVG_COLOR, HUD_QUALITY_SHADOW_PEN, text,
                         len);
}

static void DrawReloadStatusText(struct RastPort *rp, struct TextFont *font, BOOL reloadRequired,
                                 BOOL paused, BOOL showFinalScore) {
    (void)paused;

    if (!rp || !reloadRequired || showFinalScore) {
        return;
    }

    /* Keep reload messages in fixed lines reserved below PAUSED.
       When the game is not paused, the PAUSED line simply remains empty. */
    DrawReloadLine(rp, font, HUD_PAUSED_Y + 13, "RELOAD!");
    DrawReloadLine(rp, font, HUD_PAUSED_Y + 26, "(PUSH FORWARD AND PULL BACK)");
}

static void DrawAccuracy(struct RastPort *rp, struct TextFont *font, BOOL showFinalScore) {
    char text[24];
    UWORD len;
    WORD x;
    WORD width;
    UWORD accuracy;

    if (!showFinalScore || !rp) {
        return;
    }

    if (font) {
        SetFont(rp, font);
    }

    accuracy = CalculateAccuracyPercent(TargetScoring_GetTotalScore());
    len = BuildAccuracyText(text, accuracy);
    width = TextLength(rp, (STRPTR)text, len);
    x = (WORD)((SCR_W - width) / 2);
    DrawTextWithShadow(rp, font, x, HUD_ACCURACY_Y, HUD_TEXT_PEN, text, len);
}

static LONG ClampLeadFP(LONG v) {
    if (v > LEAD_MAX_FP) {
        return LEAD_MAX_FP;
    }

    if (v < -LEAD_MAX_FP) {
        return -LEAD_MAX_FP;
    }

    return v;
}

static void DrawCenteredTextWithShadow(struct RastPort *rp, struct TextFont *font, WORD y,
                                       UWORD pen, UWORD shadowPenValue, const char *text) {
    WORD width;
    WORD x;
    UWORD len;

    if (!rp || !text) {
        return;
    }

    if (font) {
        SetFont(rp, font);
    }

    len = TextLen(text);
    width = TextLength(rp, (STRPTR)text, len);
    x = (WORD)((SCR_W - width) / 2);
    DrawTextWithShadowEx(rp, font, x, y, pen, shadowPenValue, text, len);
}

static void DrawEndRoundOverlay(struct RastPort *rp, struct TextFont *font, BOOL levelCompleted,
                                BOOL lastShotHit, UBYTE lastShotScore, UWORD sessionScore) {
    const char *qualityText;

    if (!rp) {
        return;
    }

    DrawCenteredTextWithShadow(rp, font, ENDROUND_TITLE_Y, HUD_TEXT_PEN, HUD_SHADOW_PEN,
                               levelCompleted ? "LEVEL COMPLETED" : "OUT OF AMMO");

    DrawHitCounter(rp, font);

    if (lastShotHit) {
        DrawLastShotResult(rp, font, TRUE, TRUE);
        qualityText = GetScoreText(lastShotScore);

        if (qualityText) {
            DrawTextWithShadowEx(rp, font, HUD_TEXT_X, HUD_QUALITY_Y,
                                 GetScoreTextPen(lastShotScore), HUD_QUALITY_SHADOW_PEN,
                                 qualityText, (UWORD)strlen(qualityText));
        }
    } else {
        DrawLastShotResult(rp, font, TRUE, FALSE);
    }
}

BOOL RunRangeWithFrontSight(BOOL useDBuf, RangeSummaryData *outSummary) {
    RangeSessionState state;
    BOOL endScreenDrawn = FALSE;
    struct DateStamp reloadPauseStamp;
    BOOL reloadPauseStampValid = FALSE;
    InitRangeSessionState(&state, IS_NEW_GAME_SESSION);

    AmacsBob frontSight;
    AmacsBob rearSight;
    struct TextFont *hudFont = NULL;

    const LONG V_MAX = 13312;
    const LONG V_MIN = 96;
    const LONG V_STOP = 32;
    const LONG ACCEL_DIV = 12;
    const LONG DECAY_NUM = 2;
    const LONG DECAY_DEN = 256;
    const UWORD START_DELAY = 3;
    struct BitMap bg;
    BOOL haveBg = FALSE;
    PLANEPTR tempMaskPlane = NULL;
    struct BitMap maskSrcBm;
    struct BitMap maskTmpBm;
    struct RastPort maskTmpRP;

#define ringX state.ringX
#define ringY state.ringY
#define ax state.ax
#define ay state.ay
#define vx state.vx
#define vy state.vy
#define prevDirX state.prevDirX
#define prevDirY state.prevDirY
#define holdX state.holdX
#define holdY state.holdY
#define leadX state.leadX
#define leadY state.leadY
#define shotCooldownActive state.shotCooldownActive
#define shotNeedsRelease state.shotNeedsRelease
#define recoilActive state.recoilActive
#define recoilTick state.recoilTick
#define rearRecoilHistory state.rearRecoilHistory
#define resultFlashTicks state.resultFlashTicks
#define resultFlashColor state.resultFlashColor
#define lastShotStamp state.lastShotStamp
#define paused state.paused
#define shotTaken state.shotTaken
#define lastShotHit state.lastShotHit
#define lastShotScore state.lastShotScore
#define currentFrontRecoilY state.currentFrontRecoilY
#define currentRearRecoilY state.currentRearRecoilY
#define ammoCount state.ammoCount
#define reloadState state.reloadState
#define reloadNeedsNeutral state.reloadNeedsNeutral
#define reloadSpeechStamp state.reloadSpeechStamp
#define reloadSpeechStampValid state.reloadSpeechStampValid
#define reloadSpeechFirstPromptPending state.reloadSpeechFirstPromptPending
#define reloadFinishStamp state.reloadFinishStamp
#define reloadFinishStampValid state.reloadFinishStampValid
#define showFinalScore state.showFinalScore
#define sessionComplete state.sessionComplete
#define roundEnding state.roundEnding
#define levelCompleted state.levelCompleted
#define finalScoreStamp state.finalScoreStamp
#define finalScoreStampValid state.finalScoreStampValid

    if (!Bob_LoadRawAndMask(&frontSight, FRONTSIGHT_RAW, FRONTSIGHT_MASK, FRONTSIGHT_W,
                            FRONTSIGHT_H, 5)) {
        return FALSE;
    }

    if (!Bob_LoadRawAndMask(&rearSight, REARSIGHT_RAW, REARSIGHT_MASK, REARSIGHT_W, REARSIGHT_H,
                            5)) {
        Bob_Free(&frontSight);
        return FALSE;
    }

    hudFont = OpenFont((struct TextAttr *)&gHudFontAttr);

    if (!TargetsHandler_Init()) {
        if (hudFont) {
            CloseFont(hudFont);
        }

        Bob_Free(&frontSight);
        Bob_Free(&rearSight);
        return FALSE;
    }

    if (!Sound_Init()) {
        DebugBeepError(Sound_GetLastError());
    }

    /* Ensure a clean session state even when coming back from a previous run */
    TargetsHandler_Reset();
    TargetsHandler_SetPaused(FALSE);
    Sound_SetPaused(FALSE);
    DateStamp(&state.roundStartStamp);

    tempMaskPlane = (PLANEPTR)AllocRaster(FRONTSIGHT_W, FRONTSIGHT_H);

    if (!tempMaskPlane) {
        Sound_Shutdown();
        TargetsHandler_Shutdown();

        if (hudFont) {
            CloseFont(hudFont);
        }

        Bob_Free(&frontSight);
        Bob_Free(&rearSight);
        return FALSE;
    }

    InitBitMap(&maskSrcBm, 1, FRONTSIGHT_W, FRONTSIGHT_H);
    maskSrcBm.Planes[0] = frontSight.mask;

    InitBitMap(&maskTmpBm, 1, FRONTSIGHT_W, FRONTSIGHT_H);
    maskTmpBm.Planes[0] = tempMaskPlane;

    InitRastPort(&maskTmpRP);
    maskTmpRP.BitMap = &maskTmpBm;

    InitBitMap(&bg, 5, SCR_W, SCR_H);

    {
        UWORD p;
        for (p = 0; p < 5; p++) {
            bg.Planes[p] = (PLANEPTR)AllocRaster(SCR_W, SCR_H);

            if (!bg.Planes[p]) {
                UWORD q;

                for (q = 0; q < 5; q++) {
                    if (bg.Planes[q]) {
                        FreeRaster(bg.Planes[q], SCR_W, SCR_H);
                        bg.Planes[q] = NULL;
                    }
                }

                FreeRaster(tempMaskPlane, FRONTSIGHT_W, FRONTSIGHT_H);
                tempMaskPlane = NULL;
                Sound_Shutdown();
                TargetsHandler_Shutdown();

                if (hudFont) {
                    CloseFont(hudFont);
                }

                Bob_Free(&frontSight);
                Bob_Free(&rearSight);
                return FALSE;
            }
        }
    }

    {
        struct Screen *scr = Gfx_GetScreen();

        if (scr && scr->RastPort.BitMap) {
            WaitBlit();
            BltBitMap(scr->RastPort.BitMap, 0, 0, &bg, 0, 0, SCR_W, SCR_H, 0xC0, 0xFF, NULL);
            WaitBlit();
            haveBg = TRUE;
        }
    }

    for (;;) {
        Input_PollWindow(Gfx_GetWindow());

        if (!paused) {
            Sound_Update();
        }

        if (Input_QuitPressed()) {
            break;
        }

        if (!roundEnding && !showFinalScore && Input_KeyPressed(0x19)) {
            BOOL wasPaused = paused;
            paused = (BOOL)!paused;
            Sound_SetPaused(paused);
            TargetsHandler_SetPaused(paused);

            /* Do not stop an already playing Speech_Reload sample when the
             * game is paused.  Just prevent new reload reminders from being
             * started while paused, and make the 5s reminder timer count only
             * active gameplay time.
             */
            if (!wasPaused && paused && reloadState != RELOAD_STATE_NONE) {
                DateStamp(&reloadPauseStamp);
                reloadPauseStampValid = TRUE;
            } else if (wasPaused && !paused && reloadPauseStampValid) {
                if (reloadState != RELOAD_STATE_NONE && reloadSpeechStampValid) {
                    AddTicksToDateStamp(&reloadSpeechStamp, ElapsedTicksSince(&reloadPauseStamp));
                }
                if (reloadState == RELOAD_STATE_FINISHING && reloadFinishStampValid) {
                    AddTicksToDateStamp(&reloadFinishStamp, ElapsedTicksSince(&reloadPauseStamp));
                }
                reloadPauseStampValid = FALSE;
            }
        }

        if (!paused) {
            if (!Input_IsFireDown()) {
                shotNeedsRelease = FALSE;
            }

            if (!roundEnding && !showFinalScore && reloadState == RELOAD_STATE_NONE &&
                Input_FirePressed()) {
                if (ammoCount > 0 && !shotNeedsRelease &&
                    ShotCooldownReady(shotCooldownActive, &lastShotStamp)) {
                    WORD aimX;
                    WORD aimY;
                    WORD sightOffsetX;
                    WORD sightOffsetY;
                    UWORD hitDelayTicks;
                    UBYTE hitScore = SCORE_MISS;
                    UBYTE hitVolume = 64;
                    BOOL targetsComplete = FALSE;

                    ammoCount--;
                    Sound_PlayShot();
                    MarkShotFired(&shotCooldownActive, &lastShotStamp);
                    shotNeedsRelease = TRUE;
                    recoilActive = TRUE;
                    recoilTick = 0;
                    shotTaken = TRUE;

                    aimX = (WORD)(ringX - RING_OFFSET_X + FRONT_AIM_X);
                    aimY = (WORD)(ringY - RING_OFFSET_Y + FRONT_AIM_Y);
                    sightOffsetX = (WORD)(leadX / 256);
                    sightOffsetY = (WORD)(leadY / 256);

                    if (TargetsHandler_CheckHit(aimX, aimY, sightOffsetX, sightOffsetY,
                                                &hitDelayTicks, &hitScore, &hitVolume)) {
                        RegisterHit();
                        Sound_PlayHit(hitDelayTicks, hitVolume);
                        lastShotHit = TRUE;
                        lastShotScore = hitScore;
                        resultFlashColor = GetFlashColorForScore(hitScore);

                        if (state.sessionScore <= (65535 - hitScore)) {
                            state.sessionScore = (UWORD)(state.sessionScore + hitScore);
                        } else {
                            state.sessionScore = 65535;
                        }
                    } else {
                        lastShotHit = FALSE;
                        lastShotScore = SCORE_MISS;
                        resultFlashColor = SCORE_FLASH_MISS_COLOR;
                    }

                    resultFlashTicks = RESULT_FLASH_TICKS;
                    targetsComplete = TargetsHandler_IsComplete();

                    if (targetsComplete || ammoCount == 0) {
                        levelCompleted = targetsComplete;
                        state.finalShotValid = TRUE;
                        state.finalShotHitSnap = lastShotHit;
                        state.finalShotScoreSnap = lastShotScore;

                        if (outSummary) {
                            UWORD totalTime = (UWORD)ElapsedSecondsSince(&state.roundStartStamp);

                            outSummary->score = ScaleSummaryScore(state.sessionScore);
                            outSummary->accuracy = CalculateAccuracyPercent(state.sessionScore);
                            outSummary->totalTime = totalTime;
                            outSummary->timeBonus =
                                state.sessionScore > 0
                                    ? CalculateTimeBonus(totalTime, outSummary->accuracy)
                                    : 0;
                            outSummary->summaryLastShotHit = state.finalShotHitSnap;
                            outSummary->summaryLastShotScore = state.finalShotScoreSnap;
                        }

                        roundEnding = TRUE;
                        sessionComplete = TRUE;

                        DateStamp(&finalScoreStamp);
                        finalScoreStampValid = TRUE;
                    } else if ((ammoCount % HUD_MAGAZINE_SIZE) == 0) {
                        StartReloadPrompt(&state);
                        shotNeedsRelease = TRUE;
                    }
                }
            }

            if (!roundEnding && !showFinalScore && reloadState != RELOAD_STATE_NONE) {
                BOOL reloadUp = Input_Up();
                BOOL reloadDown = Input_Down();

                /* Fire during reload is ignored completely.  Consume the edge
                 * here so it cannot be applied immediately after the magazine
                 * is seated.  If Fire is held, require a release before the
                 * next valid shot.
                 */
                if (Input_FirePressed() || Input_IsFireDown()) {
                    shotNeedsRelease = TRUE;
                }

                if (reloadNeedsNeutral) {
                    if (!reloadUp && !reloadDown) {
                        reloadNeedsNeutral = FALSE;
                    }
                } else if (reloadState == RELOAD_STATE_WAIT_PUSH) {
                    if (reloadUp) {
                        Sound_PlayReloadMagOut();
                        reloadState = RELOAD_STATE_WAIT_PULL;
                    }
                } else if (reloadState == RELOAD_STATE_WAIT_PULL) {
                    if (reloadDown) {
                        Sound_PlayReloadMagIn();
                        StartReloadFinishing(&state);
                        reloadPauseStampValid = FALSE;
                        shotNeedsRelease = TRUE;
                    }
                }
            }

            if (!roundEnding && !showFinalScore) {
                UpdateReloadFinishing(&state);
                UpdateReloadSpeech(&state);
            }

            if (!roundEnding) {
                TargetsHandler_Tick();
            }

            {
                int dirX = (Input_Right() ? 1 : 0) - (Input_Left() ? 1 : 0);
                int dirY = (Input_Down() ? 1 : 0) - (Input_Up() ? 1 : 0);

                if (dirX != 0) {
                    if (prevDirX == 0 || dirX != prevDirX) {
                        ringX += (WORD)dirX;
                        holdX = 1;
                        vx = 0;
                        ax = 0;
                    } else {
                        if (holdX < 0xFFFF) {
                            holdX++;
                        }

                        if (holdX >= START_DELAY) {
                            LONG target = (LONG)dirX * V_MAX;
                            LONG dv = target - vx;

                            vx += dv / ACCEL_DIV;

                            if (vx < V_MIN && vx > -V_MIN) {
                                vx = (LONG)dirX * V_MIN;
                            }

                            ax += vx;

                            while (ax >= 256) {
                                ax -= 256;
                                ringX++;
                            }

                            while (ax <= -256) {
                                ax += 256;
                                ringX--;
                            }
                        }
                    }
                } else {
                    holdX = 0;
                    prevDirX = 0;
                    vx = (vx * DECAY_NUM) / DECAY_DEN;

                    if (vx < V_STOP && vx > -V_STOP) {
                        vx = 0;
                    }

                    ax += vx;

                    while (ax >= 256) {
                        ax -= 256;
                        ringX++;
                    }

                    while (ax <= -256) {
                        ax += 256;
                        ringX--;
                    }

                    if (vx == 0) {
                        ax = 0;
                    }
                }

                if (dirY != 0) {
                    if (prevDirY == 0 || dirY != prevDirY) {
                        ringY += (WORD)dirY;
                        holdY = 1;
                        vy = 0;
                        ay = 0;
                    } else {
                        if (holdY < 0xFFFF) {
                            holdY++;
                        }

                        if (holdY >= START_DELAY) {
                            LONG target = (LONG)dirY * V_MAX;
                            LONG dv = target - vy;

                            vy += dv / ACCEL_DIV;

                            if (vy < V_MIN && vy > -V_MIN) {
                                vy = (LONG)dirY * V_MIN;
                            }

                            ay += vy;

                            while (ay >= 256) {
                                ay -= 256;
                                ringY++;
                            }

                            while (ay <= -256) {
                                ay += 256;
                                ringY--;
                            }
                        }
                    }
                } else {
                    holdY = 0;
                    prevDirY = 0;
                    vy = (vy * DECAY_NUM) / DECAY_DEN;

                    if (vy < V_STOP && vy > -V_STOP) {
                        vy = 0;
                    }

                    ay += vy;

                    while (ay >= 256) {
                        ay -= 256;
                        ringY++;
                    }

                    while (ay <= -256) {
                        ay += 256;
                        ringY--;
                    }

                    if (vy == 0) {
                        ay = 0;
                    }
                }

                {
                    BOOL joystickMoving = (dirX != 0 || dirY != 0) ? TRUE : FALSE;
                    WORD handSwayDeltaX = UpdateHandSway(&state, joystickMoving);
                    WORD breathDeltaY = UpdateBreathing(&state, joystickMoving);

                    if (!joystickMoving && handSwayDeltaX != 0) {
                        ringX = (WORD)(ringX + handSwayDeltaX);
                    }

                    if (!joystickMoving && breathDeltaY != 0) {
                        ringY = (WORD)(ringY + breathDeltaY);
                    }
                }

                prevDirX = dirX;
                prevDirY = dirY;
            }

            if (ringX < -OVERSCAN_X_TOTAL) {
                ringX = -OVERSCAN_X_TOTAL;
            }

            if (ringX > SCR_W - REARSIGHT_W + OVERSCAN_X_TOTAL) {
                ringX = (SCR_W - REARSIGHT_W + OVERSCAN_X_TOTAL);
            }

            if (ringY < 0) {
                ringY = 0;
            }

            if (ringY > SCR_H - REARSIGHT_H + OVERSCAN_Y) {
                ringY = (SCR_H - REARSIGHT_H + OVERSCAN_Y);
            }

            {
                LONG targetLeadX = (vx * LEAD_MAX_FP) / V_MAX;
                LONG targetLeadY = (vy * LEAD_MAX_FP) / V_MAX;

                if (state.handSwayLeadTargetX != 0 && vx == 0 && vy == 0) {
                    targetLeadX += state.handSwayLeadTargetX;
                }

                if (state.breathLeadTargetY != 0 && vx == 0 && vy == 0) {
                    targetLeadY += state.breathLeadTargetY;
                }

                targetLeadX = ClampLeadFP(targetLeadX);
                targetLeadY = ClampLeadFP(targetLeadY);

                leadX += (targetLeadX - leadX) / LEAD_FOLLOW_DIV;
                leadY += (targetLeadY - leadY) / LEAD_FOLLOW_DIV;

                if (vx == 0 && vy == 0 && state.breathLeadTargetY == 0 &&
                    state.handSwayLeadTargetX == 0) {
                    leadX = (leadX * LEAD_DECAY_NUM) / LEAD_DECAY_DEN;
                    leadY = (leadY * LEAD_DECAY_NUM) / LEAD_DECAY_DEN;

                    if (leadX < LEAD_STOP_FP && leadX > -LEAD_STOP_FP) {
                        leadX = 0;
                    }

                    if (leadY < LEAD_STOP_FP && leadY > -LEAD_STOP_FP) {
                        leadY = 0;
                    }
                }
            }
        }

        {
            WORD frontRecoilY;
            WORD rearRecoilY;
            WORD frontX;
            WORD frontY;
            struct RastPort *rp = useDBuf ? Gfx_GetDrawRastPort() : &Gfx_GetScreen()->RastPort;

            if (!paused) {
                currentFrontRecoilY = RecoilOffsetY(&recoilActive, &recoilTick);
                currentRearRecoilY = RearRecoilOffsetY(currentFrontRecoilY, rearRecoilHistory);
            }

            frontRecoilY = currentFrontRecoilY;
            rearRecoilY = currentRearRecoilY;
            frontX = (WORD)(ringX - RING_OFFSET_X + (leadX / 256));
            frontY = (WORD)(ringY - RING_OFFSET_Y + (leadY / 256) + frontRecoilY);

            if (haveBg) {
                WaitBlit();
                BltBitMap(&bg, 0, 0, rp->BitMap, 0, 0, SCR_W, SCR_H, 0xC0, 0xFF, NULL);
                WaitBlit();
            }

            if (!roundEnding) {
                TargetsHandler_Draw(rp);
            }

            WaitBlit();
            BltBitMap(&maskSrcBm, 0, 0, &maskTmpBm, 0, 0, FRONTSIGHT_W, FRONTSIGHT_H, 0xC0, 0xFF,
                      NULL);
            WaitBlit();

            {
                WORD occX = ringX + OCCL_REL_X;
                WORD occY = ringY + rearRecoilY + OCCL_REL_Y;
                WORD ix;
                WORD iy;
                WORD iw;
                WORD ih;

                if (IntersectRect(frontX, frontY, FRONTSIGHT_W, FRONTSIGHT_H, occX, occY, OCCL_W,
                                  OCCL_H, &ix, &iy, &iw, &ih)) {
                    WORD relX = (WORD)(ix - frontX);
                    WORD relY = (WORD)(iy - frontY);

                    SetAPen(&maskTmpRP, 0);
                    RectFill(&maskTmpRP, relX, relY, relX + iw - 1, relY + ih - 1);
                }
            }

            DrawMaskedClipped(&frontSight.bm, tempMaskPlane, rp, frontX, frontY, FRONTSIGHT_W,
                              FRONTSIGHT_H);
            DrawMaskedClipped(&rearSight.bm, rearSight.mask, rp, ringX, (WORD)(ringY + rearRecoilY),
                              REARSIGHT_W, REARSIGHT_H);

            if (!roundEnding) {
                if (resultFlashTicks > 0) {
                    DrawResultFlash(rp, resultFlashColor);

                    if (!paused) {
                        resultFlashTicks--;
                    }
                }

                DrawHitCounter(rp, hudFont);
                DrawAmmoBlocks(rp, hudFont, ammoCount);
                DrawLastShotResult(rp, hudFont, shotTaken, lastShotHit);
                DrawShotQuality(rp, hudFont, shotTaken, lastShotHit, lastShotScore);
                DrawCenterStatusText(rp, hudFont, paused, showFinalScore);
                DrawReloadStatusText(rp, hudFont, reloadState != RELOAD_STATE_NONE, paused,
                                     showFinalScore);
                DrawAccuracy(rp, hudFont, showFinalScore);

                if (useDBuf) {
                    Gfx_SwapBuffers();
                }
            } else if (!endScreenDrawn) {
                if (haveBg) {
                    WaitBlit();
                    BltBitMap(&bg, 0, 0, rp->BitMap, 0, 0, SCR_W, SCR_H, 0xC0, 0xFF, NULL);
                    WaitBlit();
                }

                DrawEndRoundOverlay(rp, hudFont, levelCompleted, state.finalShotHitSnap,
                                    state.finalShotScoreSnap, state.sessionScore);

                if (useDBuf) {
                    Gfx_SwapBuffers();
                }

                endScreenDrawn = TRUE;
            }
        }

        if (roundEnding && !recoilActive && ShotCooldownReady(shotCooldownActive, &lastShotStamp) &&
            finalScoreStampValid && ElapsedSecondsSince(&finalScoreStamp) >= 4UL) {
            sessionComplete = TRUE;
            break;
        }

        WaitTOF();
    }

    Sound_Shutdown();
    TargetsHandler_Shutdown();

    if (haveBg) {
        UWORD p;
        for (p = 0; p < 5; p++) {
            if (bg.Planes[p]) {
                FreeRaster(bg.Planes[p], SCR_W, SCR_H);
                bg.Planes[p] = NULL;
            }
        }
    }

    if (tempMaskPlane) {
        FreeRaster(tempMaskPlane, FRONTSIGHT_W, FRONTSIGHT_H);
        tempMaskPlane = NULL;
    }

    if (hudFont) {
        CloseFont(hudFont);
    }

    Bob_Free(&frontSight);
    Bob_Free(&rearSight);

#undef ringX
#undef ringY
#undef ax
#undef ay
#undef vx
#undef vy
#undef prevDirX
#undef prevDirY
#undef holdX
#undef holdY
#undef leadX
#undef leadY
#undef shotCooldownActive
#undef shotNeedsRelease
#undef recoilActive
#undef recoilTick
#undef rearRecoilHistory
#undef resultFlashTicks
#undef resultFlashColor
#undef lastShotStamp
#undef paused
#undef shotTaken
#undef lastShotHit
#undef lastShotScore
#undef currentFrontRecoilY
#undef currentRearRecoilY
#undef ammoCount
#undef reloadState
#undef reloadNeedsNeutral
#undef reloadSpeechStamp
#undef reloadSpeechStampValid
#undef reloadSpeechFirstPromptPending
#undef reloadFinishStamp
#undef reloadFinishStampValid
#undef showFinalScore
#undef sessionComplete
#undef roundEnding
#undef levelCompleted

    return state.sessionComplete;
}
