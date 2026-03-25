#ifndef ASSETS_H
#define ASSETS_H

#include <exec/types.h>

/* Canonical palette symbols (defined in assets.c) */
extern const UWORD LogoPaletteRGB4[16];
extern const UWORD TitlePaletteRGB4[16];
extern const UWORD TrainingInfoPaletteRGB4[32];
extern const UWORD FundamentalsPaletteRGB4[32];
extern const UWORD TargetRangesPaletteRGB4[32];
extern const UWORD RangePaletteRGB4[32];

/* Friendly aliases (use these in code if you prefer) */
#define logoPalette LogoPaletteRGB4
#define titlePalette TitlePaletteRGB4
#define trainingInfoPalette TrainingInfoPaletteRGB4
#define fundamentalsPalette FundamentalsPaletteRGB4
#define targetRangesPalette TargetRangesPaletteRGB4
#define rangePalette RangePaletteRGB4

/* RAW graphics files */
#define LOGO_FILE "gfx/Logo.raw"
#define TITLE_FILE "gfx/Title.raw"
#define TRAINING_INFO_FILE "gfx/TrainingInfo.raw"
#define FUNDAMENTALS_FILE "gfx/Fundamentals.raw"
#define TARGET_RANGES_FILE "gfx/TargetRanges.raw"
#define RANGE_FILE "gfx/OahuRange.raw"

/* RAW sound files */
#define SHOT_FILE "audio/Shot.raw"

#endif