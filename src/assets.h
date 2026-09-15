#ifndef ASSETS_H
#define ASSETS_H

#include <exec/types.h>

/* Canonical palette symbols (defined in assets.c) */
extern const UWORD LogoPaletteRGB4[16];
extern const UWORD Title_ScreenPaletteRGB4[32];
extern const UWORD TrainingInfoPaletteRGB4[32];
extern const UWORD FundamentalsPaletteRGB4[32];
extern const UWORD TargetRangesPaletteRGB4[32];
extern const UWORD PerformancePaletteRGB4[32];
extern const UWORD RangePaletteRGB4[32];

/* Friendly aliases (use these in code if you prefer) */
#define logoPalette LogoPaletteRGB4
#define titlePalette Title_ScreenPaletteRGB4
#define trainingInfoPalette TrainingInfoPaletteRGB4
#define fundamentalsPalette FundamentalsPaletteRGB4
#define targetRangesPalette TargetRangesPaletteRGB4
#define performancePalette PerformancePaletteRGB4
#define rangePalette RangePaletteRGB4

/* RAW graphics files */
#define LOGO_FILE "gfx/Logo.raw"
#define RANGE_FILE "gfx/OahuRange.raw"
#define TITLE_FILE "gfx/Title.raw"
#define WOODLAND_FILE "gfx/Woodland.raw"

/* Basic sound files */
#define BIRD_CALL_FILE "audio/Bird_Call.raw"
#define DRUMS_LOOP_FILE "audio/Yankee_Doodle_Drums.raw"
#define SHOT_FILE "audio/Shot.raw"
#define TARGET_HIT_FILE "audio/TargetHit.raw"
#define RELOAD_MAG_IN_FILE "audio/Reload_Mag_In.raw"
#define RELOAD_MAG_OUT_FILE "audio/Reload_Mag_Out.raw"

/* Enhanced audio files for systems with 1 MB or more Chip RAM */
#define TITLE_MUSIC_FILE "audio/enhanced/Adjutants_Call_Title.raw"
#define HISCORE_FANFARE_FILE "audio/enhanced/Four_Ruffles_Fanfare.raw"
#define SPEECH_EXCELLENT_FILE "audio/enhanced/Speech_Excellent.raw"
#define SPEECH_HIT_FILE "audio/enhanced/Speech_Hit.raw"
#define SPEECH_MISS_FILE "audio/enhanced/Speech_Miss.raw"
#define SPEECH_PREPARE_TO_FIRE_FILE "audio/enhanced/Speech_PrepareToFire.raw"
#define SPEECH_RELOAD_FILE "audio/enhanced/Speech_Reload.raw"
#define SPEECH_SUPERB_FILE "audio/enhanced/Speech_Superb.raw"
#define SPEECH_UNACCEPTABLE_FILE "audio/enhanced/Speech_Unacceptable.raw"
#define SPEECH_WELL_DONE_FILE "audio/enhanced/Speech_WellDone.raw"

#endif