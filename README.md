## AMACS
AMACS (*Amiga Multi-purpose Arcade Combat Simulator*) is a military-themed target shooting simulator for classic Commodore Amiga computers.
The project is inspired by the Multi-purpose Arcade Combat Simulator (MACS), a U.S. Army marksmanship training program originally developed in the 1980s for systems such as the Apple II and Commodore 64, and later adapted for the Super Nintendo Entertainment System (SNES).
AMACS is not a direct port. Instead, it recreates the core training concepts of MACS while embracing the capabilities and user experience of AmigaOS. Unlike the SNES version, which relied on a light gun, AMACS is designed to work with standard Amiga peripherals including joysticks, mice and keyboards.
The project is developed and tested on real Amiga hardware, as well as under WinUAE. It is designed to run from a floppy disk or hard disk drive, or under emulation.

### AI-Assisted Development
AMACS is also an experiment in AI-assisted software development. Most of the C source code is generated collaboratively with ChatGPT, with additional support from Grok for selected tasks. All code and assets are reviewed, tested and integrated by the project author.

### Tools and Assets
Graphics are created using Personal Paint, Scenery Animator and GIMP.
The game uses sampled sound effects and narration generated with ElevenLabs. Music is based on public-domain recordings associated with the U.S. Army and U.S. Marine Corps.

### Features
- Multiple target types and engagement distances
- Distance-based target scaling and scoring
- Detailed hit visualisation and performance summaries
- OCS/ECS/AGA compatibility
- Workbench-friendly operation
- Floppy disk compatible distribution
- Real hardware focused development

### Requirements
- AmigaOS 3.x
- lowlevel.library
- Motorola 68000 CPU or higher
- 1 MB RAM minimum

## Controls
### Global
- **Amiga + Q** – Quit to Workbench.

### Menu Navigation
- **Joystick Fire** / **Left Mouse Button** – Pull the trigger to continue.

### Firing Range
- **Joystick** – Aim using the iron sights.
- **Joystick Forward then Back** – Reload the weapon.
- **Joystick Fire** / **Left Mouse Button** – Pull the trigger to fire.
- **P** – Pause the simulation.

## Floppy Disk Edition
The floppy disk must be writable in order to save the high-score table.
AMACS stores high scores in the `Scores.dat` file on the game disk.

## Acknowledgments
The floppy disk edition of AMACS uses **GoWB**, a utility written by **Oliver Wagner**, to automatically load Workbench before launching the game. This ensures that the required Workbench environment and system libraries are available while keeping the startup process simple and transparent for the user.

AMACS includes excerpts from the following public domain recordings performed by official United States military ensembles:
- **"Adjutant's Call"** — Sgt. Codie Lynn Williams, United States Marine Corps
- **"Yankee Doodle"** — United States Army Chorus
- **"Four Ruffles and Flourishes"** — United States Army Band

These recordings are public domain works of the United States federal government and are available through Wikimedia Commons.

### Current Version
AMACS v0.556
Development is ongoing, with future plans including additional game modes, expanded range content and an experimental Multiscan/VGA showcase version.

## Project Information
### Repository:
https://github.com/leopold-t/amacs

### Video Showcase
https://www.youtube.com/playlist?list=PLh1sSJnx8_CuRfMMP2JcWgUo9CzC6hu0i

### Background Reading
Article about the original MACS (*Multi-purpose Arcade Combat Simulator*) *(Polish language)*:
https://www.tupalski.eu/macs-czyli-o-zastosowaniu-commodore-64-w-wojskach-usa

### Contact
Author:
Leopold "Leon" Tupalski

E-mail:
leopoldtupalski@yahoo.com

Feedback, bug reports, suggestions and contributions are welcome.
