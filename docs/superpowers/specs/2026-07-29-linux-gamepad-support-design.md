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

### 2. Evdev rumble backend

New file `src/skel/glfw/linux-rumble.cpp` (+ small header), Linux-only, behind a new `EVDEV_RUMBLE` define (set in `src/core/config.h`, guarded by `#if defined(__linux__) && !defined(GTA_HANDHELD)`).

Interface (single active pad, mirroring the game's single-pad reality):
- `LinuxRumbleOpen(const char *joyName)` — scan `/dev/input/event*`; match `EVIOCGNAME` against the active GLFW joystick name; require `FF_RUMBLE` capability (`EVIOCGBIT(EV_FF)`); open RW, upload one `ff_effect` (type `FF_RUMBLE`, infinite length, magnitudes updated in place).
- `LinuxRumbleUpdate(uint16 leftMag, uint16 rightMag)` — re-upload effect with new magnitudes and play/stop it; no-ops cheaply when magnitudes are unchanged.
- `LinuxRumbleClose()` — stop effect, close fd.

Hook points in `src/skel/glfw/glfw.cpp`:
- Pad connect/disconnect (`joysChangeCB` / `_InputInitialiseJoys`): open/close the rumble device for the active pad.
- Per-frame pad poll (next to `glfwGetGamepadState`): replicate the XInput consumption logic — if `m_PrefsUseVibration` and `ShakeFreq > 0`, magnitude = `ShakeFreq/255 * 0xffff` for both motors; decrement `ShakeDur` by `CTimer::GetTimeStepInMilliseconds()`; zero freq when expired.

Error handling: any failure (no matching node, no FF capability, open/ioctl error) disables rumble silently (debug log only). Never fatal. Device access relies on standard udev `uaccess` ACLs for seated users; no elevated permissions.

### 3. Out of scope

- Steam Controller native support (use Steam Input / XInput emulation).
- DualShock 3 pressure-sensitive buttons, SIXAXIS motion (GInput hardware extras).
- Multi-pad rumble (game is single player, pad 0 only).

## Testing

1. Build compiles with `GAMEPAD_MENU` + `EVDEV_RUMBLE`.
2. Launch: Options → Controller page appears; switching controller type changes help-prompt icon style (PS/Xbox/Switch).
3. Rumble: standalone check that the FF effect fires on the 8BitDo (fftest-equivalent), then in-game verification by the user (vehicle collision), vibration toggle honored.
4. Regression: keyboard/mouse still works; game still boots with no pad connected; pad hot-plug during gameplay doesn't crash.
