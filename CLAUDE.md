# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

reVC is a fully reverse-engineered Grand Theft Auto: Vice City (`miami` branch). It builds and runs on Windows, Linux, macOS, FreeBSD, and Android on x86, amd64, arm, and arm64.

## Build Commands

### Linux (Conan + CMake — recommended)
```
conan export vendor/librw librw/master@
mkdir build && cd build
conan install .. reVC/miami@ -if build -o reVC:audio=openal -o librw:platform=gl3 -o librw:gl3_gfxlib=glfw --build missing -s reVC:build_type=RelWithDebInfo -s librw:build_type=RelWithDebInfo
conan build .. -if build -bf build -pf package
```

### Linux (Premake)
```sh
./premake5Linux --with-librw gmake2
cd build && make config=release_x86_64
```

### Windows (Premake + Visual Studio)
```bat
premake-vs2022.cmd
:: Open build/reVC.sln in Visual Studio, build reVC project
```

### macOS (Premake)
```sh
./premake5Linux --with-librw --os=macosx gmake2
cd build && make config=release_x86_64
```

### Build options
- `--with-librw` — build librw alongside reVC (vs. using system/external)
- `--with-lto` — enable Link Time Optimization
- `--with-asan` — enable Address Sanitizer
- `--with-opus` — enable Opus audio codec support
- `--no-git-hash` / `--no-full-paths` — reproducible build options

### CMake options
```
-DREVC_AUDIO=OAL|MSS        # audio backend (default OAL)
-DREVC_WITH_OPUS=ON|OFF     # Opus support
-DREVC_WITH_SANITIZERS=ON   # UBSan
-DREVC_WITH_ASAN=ON         # ASan
```

## Architecture

reVC is a single monolithic executable. All game code lives under `src/`, organized by subsystem:

| Directory | Purpose |
|-----------|---------|
| `src/core/` | Core loop, world manager, streaming, pools, frontend, zones, config |
| `src/renderer/` | Rendering: HUD, coronas, particles, shadows, weather, timecycle |
| `src/collision/` | Collision detection and response |
| `src/entities/` | Entity base classes and managers |
| `src/peds/` | Pedestrian AI, animation blending, player control |
| `src/vehicles/` | Vehicle physics, heli, boat, car control |
| `src/weapons/` | Weapon types, bullet traces, explosions |
| `src/objects/` | Dynamic world objects |
| `src/buildings/` | Static building rendering and LOD |
| `src/animation/` | Animation system (AnimBlend) |
| `src/audio/` | Audio system with backends in `audio/oal/` (OpenAL), `audio/eax/` |
| `src/control/` | Gamepad, controller config |
| `src/math/` | Vector, matrix, quaternion math |
| `src/modelinfo/` | Model type information registry |
| `src/save/` | Save/load game state |
| `src/text/` | GXT text system |
| `src/rw/` | RenderWare helper/utility code |

### Platform abstraction (`src/skel/`)

The platform layer follows the RenderWare skeleton pattern. Entry point is `main.cpp` → `WinMain`/`main` → `Game::InitialiseOnceAfterRW()`.

- **`skeleton.h`** — public API (events, input, timer, camera). All platform backends implement this interface.
- **`platform.h`** — platform-specific function declarations (`psInitialize`, `psTimer`, etc.).
- **`glfw/glfw.cpp`** — primary cross-platform backend (Windows OpenGL, Linux, macOS, BSD)
- **`sdl2/sdl2.cpp`** — SDL2 backend used on Android (`LIBRW_SDL2` define)
- **`android/`** — Android-specific JNI bridge (`AndroidMain.cpp`, `JavaWrapper`)
- **`win/`** — Windows-specific D3D8/D3D9 backend (`RWLIBS` or `USE_D3D9`)
- **`crossplatform.cpp/.h`** — filesystem path utilities, locale/language detection, `casepath`

### RenderWare abstraction (`src/fakerw/`)

When building with librw (`LIBRW` define), stub headers in `fakerw/` redirect RenderWare API calls through librw. When building with original RW (`RWLIBS`), the real RenderWare SDK headers are used directly.

### Rendering backends (preprocessor-controlled)

- `RWLIBS` — original RenderWare D3D8 (Windows only)
- `LIBRW` + `RW_D3D9` — librw with Direct3D 9 (Windows only)
- `LIBRW` + `RW_GL3` — librw with OpenGL 3.x via GLFW or SDL2 (all platforms)

### Audio backends

- `AUDIO_MSS` — Miles Sound System (Windows, requires original MSS DLLs)
- `AUDIO_OAL` — OpenAL (cross-platform, default)

### Key configuration

`src/core/config.h` defines all pool sizes, entity limits, and compile-time options. Feature toggles and bugfix guards (e.g., `FIX_BUGS`) are also here. Runtime settings live in `reVC.ini` (not the original `gta_vc.set`).

## Coding Conventions

- **C++ standard**: C++11 maximum — do not use C++14 or later features
- **Types**: Always use project typedefs (`int8`, `uint8`, `int16`, `uint16`, `int32`, `uint32`, `bool`). Never use `unsigned` bare, `char` for non-characters, or Win32 types (`BYTE`, `DWORD`) outside platform-specific code.
- **Pointers**: `int *ptr;` style (asterisk attached to variable, not type)
- **Indentation**: TABS only
- **Brace style**: K&R-ish — brace on next line for functions/structs, same line for control flow. No braces for single-statement bodies. `else` on same line with closing brace.
- **Variable naming**: Hungarian-notation influenced — `f` prefix for float, `i`/`n` for integer, `b` for boolean, `m_` for private members, `ms_` for private static members.
- **Magic numbers**: Avoid. Use enums even when the exact meaning is unknown (`FOOBAR_TYPE_4` over `4`).

## PR Guidelines

Accepted contributions:
- Features that existed in at least one original GTA version
- Bug fixes (behind `FIX_BUGS` preprocessor guard if fixing original behavior)
- Un-reversed platform-specific or unused code
- Making reversed code more accurate (matching original assembly)
- Cross-platform skeleton/compatibility layer improvements
- Translation fixes for original languages

No custom/gameplay features unless guarded by preprocessor conditions, and no mod-like additions.

## Android Port (`android-port` branch)

### Build
```sh
cd android/launcher && ./gradlew assembleDebug
```
Requires NDK 30+, SDK 35+. Prebuilt `.so` files (SDL2, OpenAL, mpg123) are in `vendor/` — run `android/setup_libs.sh` to copy to `jniLibs/`.

### First-launch flow
1. SAF folder picker → user selects GTA VC directory
2. Files validated (`models/gta3.img` must exist)
3. Background copy to `getExternalFilesDir()` via `DocumentsContract` (app-owned files)
4. Game loads from app-private storage — no permissions needed
5. Subsequent launches skip picker

### Key changes from upstream
- `FileMgr.cpp` — `\` → `/` conversion (Windows paths)
- `sdl2.cpp` — SDL hints, `STORAGE_ROOT` via `SDL_AndroidGetExternalStoragePath`
- `crossplatform.cpp` — `casepath` does `opendir`/`readdir`; works because FUSE is case-insensitive
- `AndroidManifest.xml` — `extractNativeLibs=true` (SDL2 needs filesystem .so), landscape, gamepad-only
- `CdStream_posix.cpp` — Android uses `statfs` not `statvfs`, pthread mutex instead of semaphore
