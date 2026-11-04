# pppEmu

**pppEmu** is a PS4 homebrew emulator front-end: one app, one game list, for nine
consoles plus arcade. It's a fork of [pEMU](https://github.com/Cpasjuste/pemu) by
Cpasjuste (pSNES, pGEN, pNES, pGBA, pFBNeo), with network (SMB) loading, downloaded box
art, a skinnable dark theme and a demo (attract) mode added.
The original pEMU readme is in [README_PEMU.md](README_PEMU.md).

| Console | Emulator | Runs in |
|---|---|---|
| Master System, Game Gear, Mega Drive / Genesis, Sega CD | Genesis Plus GX | `pgen.bin` |
| NES | Nestopia | `pnes.bin` |
| Game Boy, Game Boy Color, Game Boy Advance | mGBA | `pgba.bin` |
| Super Nintendo | snes9x | pppEmu itself (`eboot.bin`) |
| Arcade (CPS, Neo Geo...) | FinalBurn Neo | `pfbneo.bin` |

All the emulators ship in the same package. pppEmu lists every game; when a game of
another console is started, pppEmu hands over to that console's program
(`sceSystemServiceLoadExec`), and quitting the game brings pppEmu back on the same game.

## Features

- **One list for all consoles**: L2 / R2 switch between ALL, Master System, Game Gear,
  Mega Drive, Sega CD, NES, Game Boy, Game Boy Color, Game Boy Advance, Super Nintendo
  and Arcade.
- **Games from anywhere**: a network share (SMB / Samba / Windows), `/data/pppemu/roms/`
  and USB drives. Each console's folder is found by name (`SNES`,
  `Nintendo - Super Nintendo Entertainment System`, `Mega Drive`, `GBA`, `FBNeo`...),
  sub folders included. Network games are downloaded when started.
- **Box art and game info**: downloaded from libretro (thumbnails and database) and
  stored on the PS4. The bottom box shows an in-game screenshot.
- **Arcade**: FBNeo romsets shown with their full names, bios files hidden, parent and bios
  zips downloaded with network games.
- **Demo mode**: random games one after the other (2 minutes each by default), after an idle
  time or with L3 + R3, with a rainbow "DEMO MODE" banner. The next game starts straight
  away, without the list or a loading box in between. Any button goes back to the list on
  that game. Saves are never written during a demo.
- **Shared settings**: display (scaling, filter, effect) and controller settings are the
  same in all the emulators. A game only keeps the settings changed for that game.
- **Skin**: dark semi-transparent boxes over a background picture, one picture per console
  page if wanted. Borders in the kiwi logo's colours flowing around the boxes, the selection
  pulsing (MAIN MENU > UI_OPTIONS > BORDER_ANIMATION: OFF, SLOW, NORMAL, FAST).
- **Game Boy screen colours**: original Game Boy games in white, classic green, light blue or
  pink (Game Boy emulator options, EMULATION > GB_COLORS, also per game).
- **Save states** for all consoles, also from the list (L1 on a game, STATES).
- **The share password** is stored encrypted in `config.cfg`.

## Install

1. Build the package (below), or get it from the GitHub workflow artifacts.
2. Install `IV0001-PPPE00001_00-PPPE0000*.pkg` on a jailbroken PS4 (ezRemote, etaHEN...).
   When upgrading from a version older than 1.00, delete pppEmu from the home screen first
   (the data stays).
3. Open pppEmu, press R1 (main menu), NETWORK: set the server, folder, username and password
   (the PS4 keyboard opens), TEST_CONNECTION, then restart pppEmu.

## Folders on the PS4

```
/data/pppemu/
   config.cfg            settings (menu, network, artwork, display, controls, demo mode)
   pppemu.log            debug log, the last line shows the last step that ran
   roms/                 local games (console folders inside, found by name)
   media/<console>/      downloaded box art, screenshots and game information
   skins/default/        background.jpg, background_<console>.jpg (1920x1080)
   cache/                downloaded network games, game list cache
   psnes/                Super Nintendo: saves, save states, per-game settings, cheats
   pgen/                 Sega consoles (+ bios/ for Sega CD: bios_CD_U.bin, _E, _J)
   pnes/                 NES
   pgba/                 Game Boy consoles (optional bios/gba_bios.bin)
   pfbneo/               Arcade
```

Back up `/data/pppemu/` now and then: all the saves are there.

More details (network, folder names, backgrounds, demo mode settings): [SMB.md](SMB.md).

## Building (Debian / Ubuntu)

Needs the PS4 OpenOrbis toolchain from [pacbrew](https://github.com/PacBrew) (`ps4-openorbis`,
`ps4-openorbis-portlibs`) and perl (used by FBNeo's build).

```
git clone --recursive <this repository> pemu
cd pemu && mkdir cmake-build && cd cmake-build
export OPENORBIS=/opt/pacbrew/ps4/openorbis
cmake -G "Unix Makefiles" -DPLATFORM_PS4=ON -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc) psnes_pkg
```

The package is made in `cmake-build/`. Run `cmake ..` again when files were added to the
sources (new patches). FBNeo makes the first build long. The version is set in
`src/cores/psnes/CMakeLists.txt` (`PPPEMU_VERSION`): raise it for every release.

The `ps4-pppemu` GitHub workflow (Actions tab, Run workflow) builds the same package.

## Possible improvements

Ideas discussed but not done yet:

- **More consoles through FBNeo**: PC Engine / TurboGrafx-16, ColecoVision, MSX, SG-1000,
  Neo Geo Pocket, ZX Spectrum (the arcade program already runs them, each needs a page).
- **Self-playing demos**: record a game session once and replay it in demo mode (Nestopia,
  snes9x and FBNeo can record input). Demo mode now relies on the games' own attract demos.
- **Game descriptions** in the bottom box from ScreenScraper (account + developer keys) or
  TheGamesDB (API key). Would also give Sega CD and arcade games their genre and players
  (libretro has no data for them).
- **Sega CD in demo mode** (off on purpose: big downloads, bios).
- **Name lookup through the router** if a network blocks the public DNS servers used for
  the artwork downloads (1.1.1.1, 8.8.8.8, 9.9.9.9).
- **Start button default**: fresh installs map Start to the touchpad (it can be remapped in
  the GAMEPAD settings).
- **Releases**: tag tested versions (`v1.03`...) with their package.

Not tested yet: USB drives, long demo runs, 2-player games, real Neo Geo / CPS sets.

## Credits

- [pEMU](https://github.com/Cpasjuste/pemu) and libcross2d by Cpasjuste
- Emulators: snes9x, Genesis Plus GX, Nestopia, mGBA, FinalBurn Neo
- [libsmb2](https://github.com/sahlberg/libsmb2) (network shares)
- [libretro thumbnails and database](https://github.com/libretro) (box art, game information)
- OpenOrbis / pacbrew (PS4 toolchain)

License: GPLv3, see [LICENSE](LICENSE). The emulators keep their own licenses.
