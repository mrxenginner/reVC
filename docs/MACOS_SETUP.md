# reVC on macOS (Apple Silicon) — Setup Notes

Personal setup notes for building and running **reVC** (GTA: Vice City reverse-engineering)
on an Apple Silicon Mac (arm64), tested on macOS 26.x (M5).

## Summary

- Build system: **premake** (gmake2), target `debug_macosx-arm64-librw_gl3_glfw-oal`
  (OpenGL 3 + GLFW windowing + OpenAL audio).
- The game is a clean-room engine re-implementation and **requires a legitimate copy of
  GTA: Vice City** for its data files (`models/gta3.img`, `data/`, `audio/`, `text/`,
  `anim/`, ...). It ships **no** copyrighted content.
- Two gotchas are handled by `scripts/build_macos.sh`:
  1. **Homebrew LLVM breaks the build** — premake's makefiles set `CC=clang`/`CXX=clang++`,
     which resolve to Homebrew LLVM 21.x on this machine and fail to compile `librw`
     (libc++ `stdlib.h` errors about `size_t`, `lldiv_t`, `FP_NAN`, ...). Fix: force
     Apple's clang via `make CC=/usr/bin/clang CXX=/usr/bin/clang++`.
  2. **`src/extras/GitSHA1.cpp` is generated at build time** — it is gitignored and missing
     on a fresh checkout. Generate it with `./printHash.sh src/extras/GitSHA1.cpp`.

## Prerequisites (Homebrew)

```bash
brew install glfw openal-soft mpg123 libsndfile unshield p7zip premake
```

- `glfw` (>= 3.3), `openal-soft`, `mpg123`, `libsndfile` — build/runtime deps of reVC.
- `unshield` — extracts the InstallShield `.cab` files on the original game discs
  (`7z`/`bsdtar` can't read them).
- `p7zip` — general archive work.
- `premake` — only needed to (re)generate the build tree.

## Build

```bash
# from the repo root
./scripts/build_macos.sh            # = debug build for arm64
# or a release build
./scripts/build_macos.sh release_macosx-arm64-librw_gl3_glfw-oal
```

The script does the equivalent of:

```bash
# generate premake build tree (only if build/Makefile is missing)
premake5 --with-librw gmake2

# generate the git hash source file (gitignored, normally made by a prebuild step)
if [ ! -f src/extras/GitSHA1.cpp ]; then ./printHash.sh src/extras/GitSHA1.cpp; fi

# compile with APPLE's clang (the Homebrew LLVM on PATH breaks librw)
cd build
make -j"$(sysctl -n hw.ncpu)" CC=/usr/bin/clang CXX=/usr/bin/clang++ \
     config=debug_macosx-arm64-librw_gl3_glfw-oal
```

Binary output: `bin/macosx-arm64-librw_gl3_glfw-oal/Debug/reVC`
(or `Release/reVC` for a release build).

> If the premake build tree already exists (`build/Makefile`), premake is skipped —
> you can just run `make` directly in `build/` with the `CC`/`CXX` overrides above.

## Game data setup

Build a game folder (here `~/GTA-VC`) with the original GTA: Vice City data. Example
when the game comes as `VC PC.rar` containing `CD1.iso` and `CD2.iso`:

```bash
# 1. Extract the rar and mount the images
bsdtar -xf 'VC PC.rar'
hdiutil attach 'VC PC/CD1.iso' -nobrowse -mountpoint /tmp/vccd1
hdiutil attach 'VC PC/CD2.iso' -nobrowse -mountpoint /tmp/vccd2

# 2. The game data lives inside InstallShield .cab archives on CD1 (7z fails; use unshield)
mkdir -p /tmp/gtavc_extract
unshield x /tmp/vccd1/data2.cab  -d /tmp/gtavc_extract   # main game data
#   -> /tmp/gtavc_extract/App_Executables/  is the game root

# 3. Assemble the game folder
mkdir -p ~/GTA-VC
cp -R /tmp/gtavc_extract/App_Executables/. ~/GTA-VC/

# 4. Radio + cutscene audio (from CD2) and the repo's extra gamefiles + the binary
mkdir -p ~/GTA-VC/Audio/Stream
cp /tmp/vccd2/Audio/*.mp3 ~/GTA-VC/Audio/Stream/
rsync -a /tmp/vccd2/Audio/ ~/GTA-VC/Audio/          # .adf stations + cutscene mp3s
cp bin/macosx-arm64-librw_gl3_glfw-oal/Debug/reVC ~/GTA-VC/reVC
cp -R gamefiles/. ~/GTA-VC/
chmod +x ~/GTA-VC/reVC
xattr -d com.apple.quarantine ~/GTA-VC/reVC 2>/dev/null || true
```

### Audio format note (important)

The reVC "miami" fork (config `PS2_AUDIO_PATHS`) expects radio audio as
**XOR-0x22 encrypted `.adf`** files in `Audio/` (e.g. `Audio/WILD.ADF`) plus plain
`.mp3` ambience/cutscene files. The original Vice City CDs already ship exactly these
(`WILD.adf`, `FLASH.adf`, ... on CD2's `Audio/`), so copying CD2's `Audio/` contents into
the game's `Audio/` makes sound (radio, SFX, cutscenes) work. Without them reVC **hard
aborts** with "reVC Error! Can't open 'AUDIO\WILD.ADF'".

## Run

```bash
cd ~/GTA-VC && ./reVC
# or: ~/GTA-VC/run.sh  (see scripts/run.sh)
```

### Controls / input

- **Menus are keyboard-driven** (authentic): Arrow/WASD to move, **Enter** to confirm,
  **Backspace** to go back. Mouse clicks do **not** operate menus.
- **In-game mouse**: left = fire, right = lock target, wheel = cycle weapon/radio.
- Mouse bindings live in `~/GTA-VC/reVC.ini` → `[Bindings]`.

### Window size / windowed vs fullscreen

Edit `~/GTA-VC/reVC.ini` → `[VideoMode]`:

```ini
[VideoMode]
Width=1280
Height=720
Depth=32
Subsystem=0
Windowed=1        ; 1 = windowed, 0 = fullscreen
```

If the game window ever "doesn't accept clicks", make sure it is the **frontmost/focused
window** (click its title bar, or `osascript` activation).

