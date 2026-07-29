# Linux Gamepad Support for reVC (GInput parity)

**Date:** 2026-07-29
**Target:** reVC (re3 miami branch, commit a16fcd8d + local pool/Wayland patches), Linux/GLFW build
**Benchmark:** Silent's GInput mod for vanilla VC — console-accurate pad support, PS/Xbox button prompts, vibration, automatic keyboard/pad switching.

## Current state

Already working in the Linux build:
- Pad input via GLFW gamepad API (`glfwGetGamepadState`, glfw.cpp:2506) with `gamecontrollerdb.txt` mappings.
- `DETECT_JOYSTICK_MENU` — joystick detection menu.
- `BUTTON_ICONS` — button prompt textures render in help texts; all prompt TXDs ship in `models/` (frontend_ds2/ds3/ds4/x360/xone/nsw.txd).
- `DETECT_PAD_INPUT_SWITCH` — automatic keyboard↔pad prompt switching via `CPad::IsAffectedByController`.

Missing vs GInput:
1. **Gamepad menu** (`GAMEPAD_MENU`): controller-type selection (DualShock 2/3/4, Xbox 360/One, Switch prompts), vibration toggle, console-style pad configuration screen. Code exists but is guarded `#if defined(XINPUT) || defined(GTA_HANDHELD)` (config.h:364) — Windows/handheld only.
2. **Vibration**: `CPad::StartShake*` sets `ShakeDur`/`ShakeFreq`, but only the Windows XInput path consumes them (`Pad.cpp`, `AffectFromXinput`: freq→motor speed, dur decremented by timestep). GLFW has no rumble API, so Linux builds never vibrate.

## Design

### 1. Enable GAMEPAD_MENU on Linux

- `src/core/config.h`: extend the guard so the Linux/GLFW build defines `GAMEPAD_MENU`.
- Fix any non-Windows compile fallout in `Frontend.cpp` / `MenuScreensCustom.cpp` (survey found no XInput references inside `GAMEPAD_MENU` blocks; expect little or none).
- Default controller type remains `CONTROLLER_XBOXONE` (matches 8BitDo Ultimate 2 layout). Preference persists via existing settings save.
- With `GAMEPAD_MENU` + `DETECT_PAD_INPUT_SWITCH`, `CControllerConfigManager::GetWideStringOfCommandKeys` switches prompt sets by `m_PrefsControllerType` (PlayStation/Switch/Xbox) automatically.

### 2. SDL3 gamepad sidecar (rumble + controller-type detection)

**Revision 2026-07-29:** replaces the earlier evdev rumble design after user request to use SDL3 (installed: sdl3 3.4.12). GLFW remains the input source — pad buttons/sticks already work through `CapturePad` and `gamecontrollerdb.txt`. SDL3 is initialized with the gamepad subsystem only (no video) and provides the two capabilities GLFW lacks: rumble and controller-type identification. Full input migration to SDL3 was considered and rejected: no user-visible gain today, real regression risk in bindings/menu code.

New files `src/skel/glfw/sdlpad.cpp` / `sdlpad.h`, behind a new `SDL3_GAMEPAD` define (set by CMake when SDL3 is found and the gfxlib is GLFW). Interface (single active pad):
- `SdlPad_Init()` — `SDL_Init(SDL_INIT_GAMEPAD)`; failure disables the sidecar, never fatal.
- `SdlPad_Update()` — per frame: `SDL_UpdateGamepads()`; reconcile the open `SDL_Gamepad*` with the pad GLFW is using (`PSGLOBAL(joy1id)`), matching by SDL GUID — `glfwGetJoystickGUID()` vs `SDL_GetJoystickGUIDForID()` with GUID chars 4–7 masked (SDL3 embeds a CRC there that GLFW's SDL2-format GUIDs zero), falling back to name match, then to the only present gamepad. Handles hot-plug via this reconciliation.
- `SdlPad_Rumble(uint16 lowMag, uint16 highMag, uint32 durationMs)` — `SDL_RumbleGamepad`.
- `SdlPad_GetControllerType()` — maps `SDL_GetGamepadType()` to `CMenuManager::CONTROLLER_*` (PS3→DS3, PS4/PS5→DS4, Xbox360→360, XboxOne→One, Switch→NSW, default XboxOne).
- `SdlPad_Shutdown()`.

Hook points in `src/skel/glfw/glfw.cpp` (follows the existing `__SWITCH__` pattern):
- `_psInitializeVibration()` → `SdlPad_Init()`; shutdown on exit.
- `_psHandleVibration()` (already called each frame from `CapturePad`) → `SdlPad_Update()`, then replicate the XInput consumption logic: magnitude = `ShakeFreq/255 * 0xffff` for both motors; decrement `ShakeDur` by `CTimer::GetTimeStepInMilliseconds()`; zero freq when expired; feed `SdlPad_Rumble`. (`m_PrefsUseVibration` is enforced by `CPad::StartShake*`, same as the XInput path.)

Auto prompt type: when the sidecar opens a gamepad, set `FrontEndMenuManager.m_PrefsControllerType = SdlPad_GetControllerType()` (requires `GAMEPAD_MENU` from section 1). The menu remains a manual override until the next pad connect.

Build: `find_package(SDL3 CONFIG)` in `src/CMakeLists.txt`; sidecar compiled only when found (warning otherwise), linked `SDL3::SDL3`. Error handling throughout: any SDL failure degrades to no-rumble/no-detection with a debug log; never fatal.

### 3. Out of scope

- Steam Controller native support (use Steam Input / XInput emulation).
- DualShock 3 pressure-sensitive buttons, SIXAXIS motion (GInput hardware extras).
- Multi-pad rumble (game is single player, pad 0 only).

## Testing

1. Build compiles with `GAMEPAD_MENU` + `SDL3_GAMEPAD`.
2. Launch: Options → Controller page appears; switching controller type changes help-prompt icon style (PS/Xbox/Switch).
3. Rumble: standalone SDL3 test program confirms `SDL_RumbleGamepad` fires on the 8BitDo, then in-game verification by the user (vehicle collision), vibration toggle honored.
4. Auto-detection: connecting the 8BitDo (XInput-style) selects Xbox prompts without touching the menu.
5. Regression: keyboard/mouse still works; game still boots with no pad connected; pad hot-plug during gameplay doesn't crash.
