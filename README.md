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

AMACS adapts its audio configuration to the available physical Chip RAM. Systems with 1 MB or more of Chip RAM can use Enhanced Audio when the required samples are present in `audio/enhanced/`. On 512 KB Chip RAM systems, Enhanced Audio is automatically disabled to preserve memory and stability.

## Controls
### Global
- **Amiga + Q** – Quit to Workbench.

### Menu Navigation
- **Joystick** / **WASD** – Navigate menu options.
- **Joystick Fire** / **Left Mouse Button** – Pull the trigger to select or continue.

### Firing Range
- **Joystick** / **WASD** – Aim using the iron sights.
- **Joystick Forward then Back** / **W then S** – Reload the weapon.
- **Joystick Fire** / **Left Mouse Button** – Pull the trigger to fire.
- **P** – Pause the simulation.

## Floppy Disk Edition
The floppy disk must be writable in order to save the high-score table. AMACS stores high scores in the `Scores.dat` file on the game disk. Due to the limited capacity of a standard Amiga floppy disk, the music in the floppy edition is limited to the in-menu drum excerpt from **"Yankee Doodle"**.

## Acknowledgments
The floppy disk edition of AMACS uses **GoWB**, a utility written by **Oliver Wagner**, to automatically load Workbench before launching the game. This ensures that the required Workbench environment and system libraries are available while keeping the startup process simple and transparent for the user.

AMACS includes excerpts from the following public domain recordings performed by official United States military ensembles:
- **"Adjutant's Call"** — Sgt. Codie Lynn Williams, United States Marine Corps
- **"Yankee Doodle"** — United States Army Chorus
- **"Four Ruffles and Flourishes"** — United States Army Band

These recordings are public domain works of the United States federal government and are available through Wikimedia Commons.

### Current Version
AMACS v0.601

### What's New in v0.601
- Added a new Main Menu screen.
- Added a Zeroing screen with selectable 250 m and 300 m battlesight zero (BZO) settings and trajectory visualisation.
- The BZO values were corrected using **AD-A160 410, Basic Rifle Marksmanship Shooter's Book** and **FM 23-9, Rifle Marksmanship** as references.
- Added a level briefing screen before entering the firing range.
- Optimised game resources to reduce the amount of required assets.
- Expanded and refined visual and audio effects.
- Added automatic LowMem/Enhanced Audio selection based on physical Chip RAM.
- Fixed a major Chip RAM leak when exiting the program.
- Improved redraw synchronisation for blinking menu items.
- Added WASD keyboard controls alongside joystick input for menu navigation, aiming and reloading.

## FAQ
**Q: Why doesn't AMACS support a light gun?**  
**A:** There is no readily available light gun that can simply be connected to an Amiga. Supporting one would require uncommon hardware or a custom adapter, while classic light guns such as the NES Zapper also depend on CRT displays and generally do not work with modern LCDs. AMACS therefore focuses on standard, widely available controllers.

**Q: Is a joystick or joypad required?**  
**A:** No. AMACS can also be controlled with the keyboard using WASD, with the left mouse button as the trigger. A joystick is fully supported and recommended, however, as its grip is the closest of these control methods to the M16 pistol grip.

**Q: Why are some sounds missing on an Amiga with 512 KB of Chip RAM?**  
**A:** AMACS automatically disables Enhanced Audio on 512 KB Chip RAM systems to preserve enough memory for stable operation. The basic audio set is used instead.

**Q: Why do I hear the narrator — the Drill Sergeant — on one Amiga but not another?**  
**A:** Enhanced Audio requires at least 1 MB of physical Chip RAM and the required samples in `audio/enhanced/`. With only 512 KB of Chip RAM, or when those samples are unavailable, AMACS uses the standard audio set.

**Q: Why does AMACS return to Workbench when there is not enough free memory instead of disabling more features?**  
**A:** AMACS selects its feature set from the detected hardware configuration. If memory allocation still fails during startup, it is treated as an initialization error rather than triggering additional dynamic feature degradation.

**Q: Why are the iron sights so large?**  
**A:** As a rule of thumb, the M16 front sight post matches the width of a standard target at about 150 m. In the SNES version of MACS, which inspired AMACS, that target is 9 pixels wide at this distance. This relationship was used as a reference when scaling the iron sights in AMACS.

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
