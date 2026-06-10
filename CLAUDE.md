# CLAUDE.md — reVC

## What this is

**reVC** is a complete, hand-written reverse-engineering of **Grand Theft Auto: Vice City** in C++ — the Vice City sibling of the `re3` project. It compiles to a standalone `reVC` executable that plays the retail game using the **original game assets** (you must own a copy of VC; reVC ships no copyrighted data).

This is the *engine* source, not a mod. Anything that was hardcoded in `gta-vc.exe` (physics, collision, rendering, ped/vehicle state machines, water/drowning, AI) lives here as editable code. The game's mission script (`main.scm` / `main.txt`) is a separate, asset-side artifact interpreted at runtime by `src/control/Script*.cpp` — editing gameplay *mechanics* happens in this C++ tree, not in the SCM.

- **Active branch:** `miami` (the VC branch — `master`/`lcs` belong to sibling projects).
- **Language constraint:** **C++03 only.** Do *not* use C++11-or-later features (no `auto`, `nullptr` → use `nil`, no range-for, no lambdas, etc.). See README "Do not use features from C++11 or later."
- **License:** educational/documentation/modding only. No license granted; keep derivative work open-source.

## Core design principle: accuracy to the original binary

This codebase's prime directive is **matching the original game's behavior and (ideally) assembly**, not "clean code." Per `README.md` → Contributing:

- Code that isn't behind a preprocessor flag is meant to be **faithfully reversed** from the retail binary.
- **Original-behavior bug fixes must be wrapped in `#define FIX_BUGS`** (and related flags). Don't "fix" reversed code inline — gate it.
- New behavior should resemble something that exists in some GTA (III/VC/SA) and "look as if R* could have written it."

**Implication for modding (e.g. adding swimming):** custom mechanics that never existed in VC are legitimate as a *fork*, but they break the upstream accuracy contract. Gate custom additions behind your own preprocessor flag (e.g. `#define CUSTOM_SWIMMING`) so they're greppable and separable from reversed code. dll/asi/CLEO/limit-adjuster mods do **not** work against reVC — changes must be compiled into the exe.

## Directory map (`src/`)

| Dir | Responsibility |
|-----|----------------|
| `core/` | Engine spine: `main.cpp` (entry + `Idle` main loop), `World`, `Pools`, `Streaming`, `Game`, `Cam`/`Camera`, `Pad` (input), `Timer`/`TimeStep`, `config.h` (feature flags), `Frontend` (menus), `Wanted`, `Zones`, `FileLoader`. |
| `entities/` | Base class hierarchy: `CPlaceable` → `CEntity` → `CPhysical`. Everything in the world (peds, vehicles, objects, buildings) derives from these. `Physical.cpp` holds movement/force/collision-response. |
| `peds/` | Ped logic. `Ped.cpp/.h` is the base (state machine, **buoyancy/drowning**, fighting), `PlayerPed.cpp` (player control), `PedAI.cpp`, `CivilianPed`/`CopPed`/`EmergencyPed`, `Gangs`, `Population`, `PedFight.cpp` (`InflictDamage`). |
| `vehicles/` | `Vehicle` base + `Automobile`, `Bike`, `Boat`, `Heli`, `Plane`, `Train`. `Floater.cpp` = **buoyancy physics** (`mod_Buoyancy`), `HandlingMgr` (reads `handling.cfg`), `Transmission`. |
| `weapons/` | `Weapon`, `WeaponInfo` (reads `weapon.dat`), projectiles, explosions, bullets. `WeaponType.h` includes `WEAPONTYPE_DROWNING`. |
| `control/` | High-level game systems & the **mission script interpreter**: `Script.cpp`+`Script2..8.cpp` (SCM opcode dispatch), `Garages`, `Pickups`, `CarCtrl`/`CarAI`, `PathFind`, `Replay`, `Restart`, `GameLogic`, `Phones`. |
| `renderer/` | Rendering: `Renderer`, `WaterLevel.cpp` (water surface + rendering), `WaterCreatures`, `Hud`, `Font`, `Coronas`, `Particle`, `Shadows`, `Weather`, `Timecycle`, `Draw`. |
| `animation/` | Anim-blend system: `AnimManager` (loads anim groups), `AnimationId.h` (e.g. `ANIM_STD_DROWN`), `RpAnimBlend`, `CutsceneMgr`, `Bones`. |
| `collision/` | Collision models/detection (`CColModel`, `CColPoint`, line tests). |
| `audio/` | `AudioManager`, `AudioLogic`, `DMAudio`, `MusicManager`; backends `sampman_miles` (MSS) / `sampman_oal` (OpenAL). |
| `math/` | `CVector`, `CVector2D`, `CMatrix`, `CQuaternion`, etc. |
| `modelinfo/` | Model metadata/registry. |
| `objects/` | Dynamic world objects, projectiles-as-objects, cranes, etc. |
| `save/` | Save/load (game save blocks). |
| `text/` | GXT text loading (`CText`, `messages.gxt`). |
| `rw/`, `fakerw/`, `skel/` | RenderWare layer. `rw/` wraps RW, `fakerw/` is a shim, `skel/` is the cross-platform OS/windowing/glfw skeleton (window, events, time). |
| `extras/` | reVC-specific additions (NOT reversed code): `debugmenu` (Ctrl-M), `custompipes`, `postfx`/screendroplets, `frontendoption`, `GitSHA1`. |

## Architecture essentials

- **Entry / main loop:** `core/main.cpp`. `RsEventHandler` boots the skeleton; the per-frame work is `Idle()` (~line 1540) which calls `CGame::Process()` then renders. `FrontendIdle()` runs when in menus.
- **Object storage = pools, not `new`.** `core/Pools.h` defines fixed-size `CPool<>`s (`CPedPool`, `CVehiclePool`, `CObjectPool`, ...). Entities are allocated from these pools; pool sizes are the classic "limits." Iterating the world = walking pools or querying `CWorld` sectors.
- **Class hierarchy:** `CPlaceable` (matrix/position) → `CEntity` (model, flags, render) → `CPhysical` (mass, velocity, collision response) → `CPed` / `CVehicle` / `CObject`. `CPlayerPed : CPed`.
- **Ped state machine:** `CPed::m_nPedState` (enum `PED_IDLE`, `PED_FALL`, `PED_JUMP`, `PED_DIE`, `PED_DRIVING`, ... in `peds/Ped.h`). State drives which `Process*` runs each frame and which animation plays. **There is no swim state** — that's the central gap for a swimming feature.
- **Animations** are data-driven from the game's anim files via `CAnimManager`; IDs in `animation/AnimationId.h`. **VC ships no swim animations**, so a swimming feature is asset-blocked, not just code-blocked.
- **Input:** `CPad::GetPad(0)` → `NewState`/`OldState` (`CControllerState`). Edge-detect with `...JustDown()` helpers in `core/Pad.h`.
- **Mission script:** `main.scm` is compiled SCM bytecode; `CRunningScript::ProcessOneCommand()` (`control/Script.cpp`) dispatches opcodes in 100-wide bands across `Script.cpp`..`Script8.cpp`. SCM can only call opcodes the engine exposes — it cannot add mechanics.

## Water / drowning (the swimming-mod hot path)

The behavior that makes deep water lethal lives in **`CPed::ProcessBuoyancy()`** — `src/peds/Ped.cpp:1628` (called from `ProcessControl` at `Ped.cpp:1773`):

- `mod_Buoyancy.ProcessBuoyancy(...)` (impl in `vehicles/Floater.cpp`) returns whether the ped is in water and the buoyancy impulse; sets `bTouchingWater` / `bIsInWater`.
- When submerged it sets `bIsDrowning = true` and calls `InflictDamage(nil, WEAPONTYPE_DROWNING, 3.0f * timestep, ...)` — `Ped.cpp:1690` (continuous drown) and `Ped.cpp:1666` (the reach-dry-land hit).
- Death routes to `ANIM_STD_DROWN` via `WEAPONTYPE_DROWNING` in `PedFight.cpp:3032`.
- Relevant flags in `Ped.h`: `bIsInWater`, `bIsDrowning`, `bDrownsInWater` (defaults set in `Ped.cpp:282-283`). `bDrownsInWater` is already checked in `InflictDamage` (`PedFight.cpp:2680`) — a natural hook point.

**To prototype swimming:** intercept here — replace the drown-damage branch with a new swim state + surface-clamped buoyancy + input-driven movement, gated behind a custom `#define`. The hard blocker remains **animations** (must be imported/authored; none exist in VC assets).

## Build

- **Windows (MSVC 2015/2017/2019):** run a `premake-vsXXXX.cmd` at repo root → opens `build/reVC.sln` → build. Needs the DX9 SDK (archived; see README). Set `GTA_VC_RE_DIR` env var to your VC install so the post-build step copies the exe there.
- **Other premake commands:** `premake5.exe` / `premake5Linux`. CMake + Conan path also supported (see README "Linux Conan").
- **Renderers:** original RenderWare (D3D8) **or** `librw` (D3D9 / OpenGL / GLES) — `librw` is a submodule under `vendor/` (clone with `--recursive`).
- **Audio:** MSS (Miles, uses original GTA dlls) or OpenAL.
- **Submodules matter:** clone `git clone --recursive -b miami ...`; `librw` won't be present otherwise.

## Conventions (`CODING_STYLE.md`)

- Indent with **TABS**.
- Brace on **same line** for control statements (`if (...) {`), **next line** for function/struct/class definitions. Function **return type on its own line**.
- No braces around single statements; `else` on the same line as braces.
- Use project typedefs: `int8/16/32`, `uint8/16/32`, `bool` — never `unsigned`, `char` (except actual chars), `__int16`, or win32 types (`BYTE`/`WORD`).
- Pointer style `int *ptr;` (not `int* ptr`). Use **`nil`**, not `NULL`/`nullptr`.
- No magic numbers where an enum belongs — name it (`FOOBAR_TYPE_4`) even if meaning is unknown.
- Overall goal: code that reads "as if R* could have written it."

## Feature flags — `src/core/config.h`

Behavior is toggled by `#define`s, not runtime config. Key ones:
- `GTA_VERSION` (target build, e.g. `GTAVC_PC_11`), `GTA_PC`, `GTA_PS2_STUFF`.
- `FIX_BUGS` — **wraps all corrections to reversed code** (keep defined except for accuracy-comparison release builds). `FIX_BUGS_64` required for 64-bit.
- `MORE_LANGUAGES`, `EXTENDED_OFFSCREEN_DESPAWN_RANGE`, `EXTENDED_COLOURFILTER`, `EXTENDED_PIPELINES`, `NEW_RENDERER`, `FREE_CAM`.
- Limit increases and many tweaks live here — check `config.h` first before assuming something is hardcoded elsewhere.

## Working in this repo — guidance for changes

1. **Mechanics → C++, not SCM.** New gameplay capability belongs in `peds/`, `vehicles/`, `entities/`, `weapons/`, etc. The SCM only orchestrates existing opcodes.
2. **Gate custom (non-reversed) code behind a preprocessor flag** so it stays separable from faithfully-reversed code (mirrors how `FIX_BUGS` is used).
3. **Respect pools and the entity hierarchy** — don't `new` world entities; allocate from the relevant `CPool`.
4. **Stick to C++03 and the tab/brace style** above; use `nil` and the int typedefs.
5. **Rebuild = full premake/MSVC (or CMake) build** — there is no hot-reload; changes require recompiling the exe.
