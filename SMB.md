# Network roms (SMB / Samba / Windows shares)

pSNES can list and launch roms stored on a network share (Windows shared folder,
Samba server, NAS). Roms are downloaded to a local cache when you launch them, so
the emulator core always reads a normal local file.

## Setup

1. Share a folder on your PC or NAS (SMB2 or SMB3).
2. Launch pSNES once so it creates its config file (`/data/psnes/config.cfg` on PS4).
3. Add one line per network folder to the `ROMS` section of the config file:

```
PSNES :
{
  ROMS :
  {
    SNES = ( "Super Nintendo", "0x00000004", "/data/psnes/roms/" );
    SNES_NAS = ( "Super Nintendo", "0x00000004", "smb://192.168.1.20/roms/snes/" );
  };
  ...
```

The first value is the system name, the second the system id (`0x00000004` for
Super Nintendo), the third the rom path. Each line needs a unique name
(`SNES_NAS`, `SNES_PC`...).

## URL format

```
smb://[domain;][user[:password]@]server[:port]/share/path/
```

| Example | Meaning |
|---|---|
| `smb://192.168.1.20/roms/snes/` | guest access |
| `smb://bob:mypassword@nas/games/SNES/` | user + password |
| `smb://WORKGROUP;bob:pw@nas:445/roms/` | with domain and port |
| `smb://nas/My%20Roms/Super%20Nintendo/` | spaces in share or folder names |

Special characters in the user name or password must be percent-encoded:
`@` → `%40`, `/` → `%2F`, `:` → `%3A`, `%` → `%25`, `;` → `%3B`.

The password is stored in plain text in the config file. Use a dedicated,
read-only account on your share.

## How it works

- **Listing:** at startup each `smb://` path is listed directly from the share
  (sub-folders are not scanned). Supported extensions are the usual pSNES ones
  (`.sfc`, `.smc`, `.zip`, ...).
- **gamelist.xml:** if the remote folder contains a `gamelist.xml`
  (EmulationStation format), names, genres and dates come from it.
- **Launching:** the rom is downloaded to `/data/psnes/cache/smb/<server>_<share>_<path>/`
  with a progress bar. If the cached copy has the same size as the remote file,
  it is used directly without downloading again.
- **Offline:** if the server is unreachable at startup, roms that were already
  downloaded still appear in the list and can be played.
- **Saves:** SRAM, save states and per-game configs are stored locally
  (`/data/psnes/sram`, `saves`, `configs`), named after the rom file, exactly as
  for local roms.
- **Cache size:** the cache is never cleaned automatically. Delete
  `/data/psnes/cache/smb/` (via FTP) to free space.

## Not supported yet

- Box art / video snaps (`media/` folder) from the share.
- Browsing sub-folders.
- Other pEMU emulators (the code lives in the shared UI, but each core still
  needs the 20-line hook from `psnes_ui_emu.cpp`).

## Building

`OPTION_SMB` is `ON` by default for PS4 and Linux. libsmb2 is a git submodule
(`external/libsmb2`) and is built automatically; its own PS4 compatibility layer
is enabled when `PLATFORM_PS4=ON`.

```
git submodule update --init external/libsmb2
mkdir cmake-build && cd cmake-build
cmake -G "Unix Makefiles" -DPLATFORM_PS4=ON -DCMAKE_BUILD_TYPE=Release ..
make psnes_pkg
```
