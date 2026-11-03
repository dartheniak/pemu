# pppEmu: network roms (SMB / Samba / Windows shares)

pppEmu is a fork of pEMU (pSNES + pGEN in one app) that can list and launch roms stored on a
network share (Windows shared folder, Samba server, NAS). Roms are downloaded
to a local cache when you launch them, so the emulator core always reads a
normal local file.

It installs as its own app (title id `PPPE00001`, data in `/data/pppemu/`), so
it never conflicts with the official pSNES.

## Setup (in the app)

1. Share a folder on your PC or NAS (SMB2 or SMB3).
2. In pppEmu, open the main menu (OPTIONS / START) and go to **NETWORK**:
   - **SERVER**: the PC or NAS address, e.g. `192.168.1.20`. You can also enter
     a full Windows path like `\\192.168.1.20\Roms\SNES`: it is split into
     SERVER and FOLDER automatically.
   - **FOLDER**: the share and folder, e.g. `Roms/Nintendo - Super Nintendo Entertainment System`.
   - **USERNAME** / **PASSWORD**: leave the username empty for guest access.
   - **CLEAR_CACHE_ON_START**: ON deletes downloaded network games each time
     the app starts.
3. Select **TEST_CONNECTION**. It reports how many games it found, or what went
   wrong (wrong password, share or folder not found, server unreachable...).
4. Restart pppEmu: the network games appear in the list.

Text is entered with the PS4 on-screen keyboard. Use the server's IP address
rather than its name: the PS4 usually can't resolve local computer names.

## How it works

- **Sub folders:** the FOLDER and all its sub folders (up to 6 levels) are
  scanned, so games sorted into `A-M/`, `Hacks/`... are found. Point FOLDER at
  your SNES folder, not at a folder containing several consoles: `.zip` files
  of other systems would otherwise be listed as SNES games.
- **gamelist.xml:** if the FOLDER contains a `gamelist.xml` (EmulationStation
  format), names, genres and dates come from it.
- **Launching:** the rom is downloaded to `/data/pppemu/cache/smb/...` with a
  progress bar, then started like a local file.
- **Cache:** with CLEAR_CACHE_ON_START ON (default) the cache is emptied at each
  start. With it OFF, already downloaded games are reused (if unchanged on the
  server) and stay playable when the server is offline.
- **Saves:** SRAM, save states and per-game settings are stored on the PS4
  (`/data/pppemu/psnes/sram`, `saves`, `configs` for Super Nintendo, the other
  emulators in their own folders), named after the rom file.
- **Security:** the password is stored encrypted in `/data/pppemu/config.cfg` ("enc1:..."),
  with a random key kept in its own file (`/data/pppemu/.pppemu_key`): config.cfg can be
  copied or shown without giving the password away. Someone with the whole data folder
  (key file included) could still decrypt it, so a read-only account for the share is
  still a good idea. A password typed directly in config.cfg is encrypted at the next start.

## Config file (advanced)

Network paths can also be added by hand to the `ROMS` section of
`/data/pppemu/config.cfg`:

```
SNES_NAS = ( "Super Nintendo", "0x00000004", "smb://user:password@192.168.1.20/Roms/SNES/" );
```

Special characters must be percent-encoded in this form (space `%20`, `@`
`%40`, `/` `%2F`, `:` `%3A`, `%` `%25`). The in-app settings do this for you.

## Building

`OPTION_SMB` is `ON` by default for PS4 and Linux. libsmb2 is a git submodule
(`external/libsmb2`); on PS4 it is built by `src/smb2-ps4/CMakeLists.txt`
instead of its own build files (they break the PS4 toolchain flags).

```
git submodule update --init --depth 1
export OPENORBIS=/opt/pacbrew/ps4/openorbis
mkdir -p cmake-build && cd cmake-build
cmake -G "Unix Makefiles" -DPLATFORM_PS4=ON -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc) psnes_pkg
```

On Linux the text fields are typed in the terminal the app was started from
(development fallback for the PS4 keyboard).

## Several consoles (pppEmu)

pppEmu lists Master System, Game Gear, Mega Drive / Genesis, Sega CD, NES, Game Boy,
Game Boy Color, Game Boy Advance, Super Nintendo and Arcade games together (L2 / R2 switch
between ALL and one console, in that order). Each console's games are found by folder name, on the
network share, in `/data/pppemu/roms/` and on USB drives:

- Super Nintendo: `SNES`, `Super Nintendo`, `Nintendo - Super Nintendo Entertainment System`...
- Mega Drive / Genesis: `Mega Drive`, `Genesis`, `MD`, `Sega - Mega Drive - Genesis`...
- Master System: `Master System`, `SMS`, `Sega - Master System - Mark III`...
- Game Gear: `Game Gear`, `GG`, `Sega - Game Gear`...
- NES: `NES`, `Famicom`, `Nintendo - Nintendo Entertainment System`...
- Game Boy: `Game Boy`, `GB`, `Nintendo - Game Boy`...
- Game Boy Color: `Game Boy Color`, `GBC`, `Nintendo - Game Boy Color`...
- Game Boy Advance: `Game Boy Advance`, `GBA`, `Nintendo - Game Boy Advance`... (the bios is
  optional: `/data/pppemu/pgba/bios/gba_bios.bin`)
- Arcade (FBNeo, one page for everything): `FBNeo - Arcade Games`, `FBNeo`, `Arcade`, `MAME`,
  `Neo Geo`, `CPS1`, `CPS2`, `CPS3`... FBNeo romsets (`sf2.zip`), shown with their full names.
  Bios files (`neogeo.zip`, `pgm.zip`...) are hidden from the list but must be in the same
  folder; zips FBNeo doesn't know are hidden. A network game is downloaded with its parent
  sets and the folder's bios files. libretro has images for arcade games, no genre/players.
- Sega CD: `Sega CD`, `Mega-CD`, `Mega CD`, `Sega - Mega-CD - Sega CD`... (`.chd`, `.cue` + its
  `.bin` tracks, `.iso`). Needs the bios files in `/data/pppemu/pgen/bios/`: `bios_CD_U.bin`
  (USA), `bios_CD_E.bin` (Europe), `bios_CD_J.bin` (Japan). libretro has no game information
  (developer, genre...) for Sega CD, only images.

Backgrounds (in `/data/pppemu/skins/default/`, 1920x1080 jpg or png): `background.jpg`
for ALL, and one per console page: `background_super_nintendo.jpg`,
`background_mega_drive.jpg`, `background_master_system.jpg`, `background_game_gear.jpg`,
`background_nes.jpg`, `background_sega_cd.jpg`, `background_game_boy.jpg`,
`background_game_boy_color.jpg`, `background_game_boy_advance.jpg`, `background_arcade.jpg`.

Super Nintendo games run in pppEmu itself, NES games in the NES program (`/app0/pnes.bin`,
data in `/data/pppemu/pnes/`). Sega games run in the Genesis program
shipped in the same package (`/app0/pgen.bin`): pppEmu closes, the game starts, and quitting
the game (in-game menu, QUIT) brings back pppEmu on the same game. The Genesis program keeps
its saves and settings in `/data/pppemu/pgen/`. Artwork is stored per console in
`/data/pppemu/media/<console>/`.

## Folders (pppEmu)

```
/data/pppemu/            shared by all consoles
   config.cfg            menu, network, artwork, display and controller settings
   pppemu.log            debug log
   roms/  media/  skins/  cache/
   psnes/                Super Nintendo: sram, save states (saves), per-game settings, cheats
   pgen/                 Mega Drive, Master System, Game Gear, Sega CD (+ bios/)
   pnes/                 NES
   pgba/                 Game Boy, Game Boy Color, Game Boy Advance
   pfbneo/               Arcade
```

SNES files from older pppEmu builds (directly in `/data/pppemu/`) are moved to `psnes/`
the first time a newer build starts (a file already in `psnes/` is never overwritten).

Display (scaling, filter, effect, fps) and controller settings are shared: pppEmu passes them
to the Genesis / NES program when a game starts (`shared_settings.cfg`), and gets them back
when the game is closed.

## Demo mode (attract mode)

Random games are shown one after the other. Settings in the main menu (R1), DEMO_MODE:

- START_AFTER: idle time in the list before it starts (OFF, 1, 2, 5, 10 min). L3 + R3 (both
  sticks pressed) starts it at once.
- GAME_TIME: 30 sec, 1, 2 (default), 5, 10 min.
- GAMES: ALL, CURRENT PAGE (the L2 / R2 console page), FAVORITES, SUPER NINTENDO ONLY (no
  switching between programs).
- SOUND: ON / OFF.

Any button: back to the list on the game being shown. While a demo game runs, its saves are
never written. Sega CD games are not used (big downloads, bios needed).
