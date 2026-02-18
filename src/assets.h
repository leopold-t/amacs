#ifndef ASSETS_H
#define ASSETS_H

#include <exec/types.h>

/* Canonical palette symbols (defined in assets.c) */
extern const UWORD LogoPaletteRGB4[16];
extern const UWORD TitlePaletteRGB4[16];
extern const UWORD TrngInfoPaletteRGB4[32];
extern const UWORD FundamentalsPaletteRGB4[32];
extern const UWORD TargetRangesPaletteRGB4[32];
extern const UWORD OahuRangePaletteRGB4[32];

/* Friendly aliases (use these in code if you prefer) */
#define logoPalette LogoPaletteRGB4
#define titlePalette TitlePaletteRGB4
#define trngInfoPalette TrngInfoPaletteRGB4
#define fundamentalsPalette FundamentalsPaletteRGB4
#define targetRangesPalette TargetRangesPaletteRGB4
#define oahuRangePalette OahuRangePaletteRGB4

/* RAW files */
#define LOGO_FILE "gfx/LOGO.RAW"
#define TITLE_FILE "gfx/TITLE.RAW"
#define TRNGINFO_FILE "gfx/TRNGINFO.RAW"
#define FUNDAMENTALS_FILE "gfx/FUNDAMENTALS.RAW"
#define TARGETRANGES_FILE "gfx/TARGETRANGES.RAW"
#define RANGE_FILE "gfx/OAHU_RANGE.RAW"

#endif