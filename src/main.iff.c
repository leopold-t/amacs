#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <libraries/iffparse.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/iffparse.h>

#include <stdio.h>
#include <stdlib.h>

#define WIDTH 640
#define HEIGHT 256
#define DEPTH 4
#define BYTES_PER_ROW (WIDTH / 8) // 640 / 8 = 80 bytes
#define BITPLANE_SIZE (BYTES_PER_ROW * HEIGHT) // 80 * 256 = 20,480 bytes
#define IMAGE_FILE "gfx/logo.iff"

// IFF/ILBM chunk IDs
#define ID_ILBM MAKE_ID('I','L','B','M')
#define ID_BMHD MAKE_ID('B','M','H','D')
#define ID_CMAP MAKE_ID('C','M','A','P')
#define ID_BODY MAKE_ID('B','O','D','Y')

// Define struct BitMapHeader (from NDK 3.9 iff/ilbm.h)
struct BitMapHeader {
    UWORD bmh_Width;
    UWORD bmh_Height;
    WORD  bmh_Left;
    WORD  bmh_Top;
    UBYTE bmh_Depth;
    UBYTE bmh_Masking;
    UBYTE bmh_Compression;
    UBYTE bmh_Pad1;
    UWORD bmh_Transparent;
    UBYTE bmh_XAspect;
    UBYTE bmh_YAspect;
    WORD  bmh_PageWidth;
    WORD  bmh_PageHeight;
};

// Global variables
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;
struct Library *IFFParseBase = NULL;
struct Screen *screen = NULL;
struct Window *window = NULL;
struct IFFHandle *iff = NULL;
UBYTE *bitplanes[DEPTH];
UWORD palette[16]; // 16 colors in RGB4 format

// Function to clean up and exit
void cleanup(void) {
    if (window) {
        CloseWindow(window);
        window = NULL;
    }
    if (screen) {
        CloseScreen(screen);
        screen = NULL;
    }
    if (iff) {
        CloseIFF(iff);
        FreeIFF(iff);
        iff = NULL;
    }
    for (int i = 0; i < DEPTH; i++) {
        if (bitplanes[i]) {
            FreeMem(bitplanes[i], BITPLANE_SIZE);
            bitplanes[i] = NULL;
        }
    }
    if (IntuitionBase) {
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
    }
    if (GfxBase) {
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = NULL;
    }
    if (IFFParseBase) {
        CloseLibrary(IFFParseBase);
        IFFParseBase = NULL;
    }
}

// Function to load and parse IFF/ILBM file
BOOL loadILBM(const char *filename) {
    // Debug: Print current directory
    char currentDir[256];
    BPTR lock = CurrentDir(0);
    if (lock) {
        if (NameFromLock(lock, currentDir, sizeof(currentDir))) {
            printf("Current directory: %s\n", currentDir);
        }
        CurrentDir(lock);
    }

    // Open IFF file
    iff = AllocIFF();
    if (!iff) {
        printf("Error: Cannot allocate IFF handle\n");
        return FALSE;
    }
    BPTR fileHandle = Open(filename, MODE_OLDFILE);
    if (!fileHandle) {
        printf("Error: Cannot open file %s\n", filename);
        return FALSE;
    }
    iff->iff_Stream = (ULONG)fileHandle;
    InitIFFasDOS(iff);
    if (OpenIFF(iff, IFFF_READ) != 0) {
        printf("Error: Cannot parse IFF file\n");
        Close(fileHandle);
        return FALSE;
    }

    // Check for ILBM form
    struct ContextNode *cn = CurrentChunk(iff);
    if (ParseIFF(iff, IFFPARSE_SCAN) != 0 || cn->cn_Type != ID_ILBM) {
        printf("Error: Not an ILBM file\n");
        Close(fileHandle);
        return FALSE;
    }

    // Collect BMHD chunk
    struct StoredProperty *prop;
    if ((prop = FindProp(iff, ID_ILBM, ID_BMHD)) == NULL) {
        printf("Error: No BMHD chunk found\n");
        Close(fileHandle);
        return FALSE;
    }
    struct BitMapHeader *bmhd = (struct BitMapHeader *)prop->sp_Data;
    if (bmhd->bmh_Width != WIDTH || bmhd->bmh_Height != HEIGHT || bmhd->bmh_Depth != DEPTH) {
        printf("Error: Image dimensions or depth mismatch (%dx%dx%d)\n",
               bmhd->bmh_Width, bmhd->bmh_Height, bmhd->bmh_Depth);
        return FALSE;
    }

    // Read palette (CMAP)
    if ((prop = FindProp(iff, ID_ILBM, ID_CMAP)) != NULL) {
        UBYTE *cmap = (UBYTE *)prop->sp_Data;
        LONG cmapSize = prop->sp_Size;
        if (cmapSize >= 16 * 3) { // 16 colors, 3 bytes per color (RGB)
            for (int i = 0; i < 16; i++) {
                palette[i] = ((cmap[i*3] >> 4) << 8) | // R
                             ((cmap[i*3+1] >> 4) << 4) | // G
                             (cmap[i*3+2] >> 4); // B
            }
        } else {
            printf("Warning: Invalid CMAP size, using default palette\n");
            for (int i = 0; i < 16; i++) palette[i] = 0; // Default to black
        }
    } else {
        printf("Warning: No CMAP chunk, using default palette\n");
        for (int i = 0; i < 16; i++) palette[i] = 0; // Default to black
    }

    // Allocate chip memory for bitplanes
    for (int i = 0; i < DEPTH; i++) {
        bitplanes[i] = (UBYTE *)AllocMem(BITPLANE_SIZE, MEMF_CHIP | MEMF_CLEAR);
        if (!bitplanes[i]) {
            printf("Error: Cannot allocate chip memory for bitplane %d\n", i);
            Close(fileHandle);
            return FALSE;
        }
    }

    // Read BODY chunk (uncompressed)
    cn = CurrentChunk(iff);
    if (cn != NULL && cn->cn_ID == ID_BODY) {
        for (int i = 0; i < DEPTH; i++) {
            if (ReadChunkBytes(iff, bitplanes[i], BITPLANE_SIZE) != BITPLANE_SIZE) {
                printf("Error: Failed to read bitplane %d\n", i);
                Close(fileHandle);
                return FALSE;
            }
        }
    } else {
        printf("Error: No BODY chunk or compressed data\n");
        Close(fileHandle);
        return FALSE;
    }

    Close(fileHandle);
    return TRUE;
}

int main(void) {
    // Open libraries
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 36);
    IFFParseBase = OpenLibrary("iffparse.library", 36);
    if (!IntuitionBase || !GfxBase || !IFFParseBase) {
        printf("Error: Cannot open libraries\n");
        cleanup();
        return RETURN_FAIL;
    }

    // Load ILBM file
    if (!loadILBM(IMAGE_FILE)) {
        cleanup();
        return RETURN_FAIL;
    }

    // Open a 640x256 16-color screen
    screen = OpenScreenTags(NULL,
        SA_Width, WIDTH,
        SA_Height, HEIGHT,
        SA_Depth, DEPTH,
        SA_DisplayID, HIRES_KEY,
        SA_Type, CUSTOMSCREEN,
        SA_ShowTitle, FALSE,
        SA_Title, (ULONG)"AMACS Logo",
        TAG_DONE);
    if (!screen) {
        printf("Error: Cannot open screen\n");
        cleanup();
        return RETURN_FAIL;
    }

    // Set palette
    LoadRGB4(&screen->ViewPort, palette, 16);

    // Open a borderless window
    window = OpenWindowTags(NULL,
        WA_CustomScreen, (ULONG)screen,
        WA_Width, WIDTH,
        WA_Height, HEIGHT,
        WA_Borderless, TRUE,
        WA_Activate, TRUE,
        WA_IDCMP, IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY,
        TAG_DONE);
    if (!window) {
        printf("Error: Cannot open window\n");
        cleanup();
        return RETURN_FAIL;
    }

    // Copy bitplanes to the screen
    for (int i = 0; i < DEPTH; i++) {
        screen->BitMap.Planes[i] = bitplanes[i];
    }

    // Main event loop
    BOOL done = FALSE;
    struct IntuiMessage *msg;
    while (!done) {
        WaitPort(window->UserPort);
        while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort))) {
            switch (msg->Class) {
                case IDCMP_MOUSEBUTTONS:
                    if (msg->Code == SELECTDOWN) { // Left mouse button
                        done = TRUE;
                    }
                    break;
                case IDCMP_RAWKEY:
                    if (msg->Code == 0x45) { // Esc key
                        done = TRUE;
                    }
                    break;
            }
            ReplyMsg((struct Message *)msg);
        }
    }

    // Clean up and exit
    cleanup();
    return RETURN_OK;
}