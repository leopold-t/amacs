#ifndef GFX_H
#define GFX_H

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>

/* Opens an always-behind black safety screen to prevent WB/CLI flicker. */
BOOL Gfx_OpenBlackScreen(UWORD width, UWORD height, UBYTE depth);

/* Closes the black safety screen (if open). */
void Gfx_CloseBlackScreen(void);

/* Opens a custom screen + borderless backdrop window for input. */
BOOL Gfx_OpenScreenAndWindow(UWORD width, UWORD height, UBYTE depth, ULONG displayID);

/* Closes current window + screen (if open). */
void Gfx_CloseScreenAndWindow(void);

/* Returns current window/screen (owned by gfx module). */
struct Screen *Gfx_GetScreen(void);
struct Window *Gfx_GetWindow(void);

/* Displays RAW image with fade-in from black to target palette. */
BOOL Gfx_ShowImageFadeInFromBlack(const char *file, const UWORD *targetPal, UWORD colors);

/* Crossfades between palettes, then loads RAW image, then fades in to target palette. */
BOOL Gfx_CrossFadeToImage(const char *file, const UWORD *fromPal, UWORD fromColors,
                          const UWORD *toPal, UWORD toColors);

/* Switches from current HiRes screen to a newly opened LoRes screen on black. */
BOOL Gfx_SwitchHiResToLoResOnBlack(const UWORD *currentHiPal16, UWORD loWidth, UWORD loHeight,
                                   UBYTE loDepth);

/* Fades the current screen out to black using the supplied palette. */
void Gfx_FadeOutCurrentScreenToBlack(const UWORD *currentPal, UWORD colors);

/* Fades the current screen in from black to the supplied palette. */
void Gfx_FadeInCurrentScreenFromBlack(const UWORD *targetPal, UWORD colors);

/* Enable double buffering on current screen (LoRes range). */
BOOL Gfx_EnableDoubleBuffering(void);

/* Disable double buffering (safe teardown). */
void Gfx_DisableDoubleBuffering(void);

/* Returns the RastPort you should draw into (back buffer if DBuf enabled, otherwise screen RP). */
struct RastPort *Gfx_GetDrawRastPort(void);

/* Swap DBuf buffers (present back buffer, switch draw buffer). */
void Gfx_SwapBuffers(void);

/* Returns TRUE if DBuf is currently active. */
BOOL Gfx_IsDoubleBufferingEnabled(void);

#endif /* GFX_H */
