# Linux Gamepad Support (GInput parity) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enable reVC's built-in gamepad menu on the Linux/GLFW build and add rumble + automatic controller-type detection through an SDL3 gamepad sidecar.

**Architecture:** GLFW keeps delivering pad input (already working). A new SDL3 module (`sdlpad.cpp`) initialized with `SDL_INIT_GAMEPAD` only tracks the same physical pad GLFW uses (GUID-matched) and supplies rumble (`SDL_RumbleGamepad`) and controller type (`SDL_GetGamepadType`). It hooks into the existing per-frame `_psHandleVibration()` slot in `glfw.cpp`, mirroring how the Nintendo Switch port consumes `ShakeFreq/ShakeDur`.

**Tech Stack:** C++11 (project standard), CMake, GLFW 3.4, SDL3 (system package, 3.4.12), re3/miami codebase conventions (`nil`, tabs, `debug()` logging).

## Global Constraints

- Repo: `/home/raidon/src/reVC`, build dir `build/`, already configured with `-DLIBRW_PLATFORM=GL3 -DGLFW_HAS_X11=FALSE -DCMAKE_BUILD_TYPE=RelWithDebInfo`.
- Game install dir (test target): `/games/steam/steamapps/common/Grand Theft Auto Vice City` — running the game opens a window on the user's Wayland session; keep test runs short (`timeout 60`).
- Code style: tabs, `nil` not `NULL`, `debug("...")` for logging, function name on its own line in definitions.
- Any SDL3 failure must degrade to no-rumble/no-detection with a `debug()` log — never fatal, never a crash.
- Build command: `cmake --build /home/raidon/src/reVC/build -j$(nproc)` (full log to a file; do NOT pipe through `tail` only).
- Test hardware: 8BitDo Ultimate 2 Wireless (XInput-style; reports as Xbox-type), connected on `/dev/input/js1`.
- `<scratchpad>` in commands below = `/tmp/claude-1000/-games-steam-steamapps-common-Grand-Theft-Auto-Vice-City/5e5b1840-acd5-42b7-a5b9-cf041badf38c/scratchpad`.

---

### Task 1: Enable GAMEPAD_MENU on Linux

**Files:**
- Modify: `/home/raidon/src/reVC/src/core/config.h:364-366`

**Interfaces:**
- Consumes: nothing new.
- Produces: `GAMEPAD_MENU` defined on Linux → `CMenuManager::m_PrefsControllerType` (int8 member) and `CMenuManager::CONTROLLER_DUALSHOCK2/3/4, CONTROLLER_XBOX360, CONTROLLER_XBOXONE, CONTROLLER_NINTENDO_SWITCH` enum (Frontend.h:726-737) exist — Task 3 relies on these.

- [ ] **Step 1: Change the guard**

In `src/core/config.h`, find (line ~364):

```cpp
#	if defined(XINPUT) || defined(GTA_HANDHELD)
#		define GAMEPAD_MENU		// Add gamepad menu
#	endif
```

Replace with:

```cpp
#	if defined(XINPUT) || defined(GTA_HANDHELD) || defined(__linux__)
#		define GAMEPAD_MENU		// Add gamepad menu
#	endif
```

- [ ] **Step 2: Build**

Run: `cmake --build /home/raidon/src/reVC/build -j$(nproc) > /tmp/claude-1000/-games-steam-steamapps-common-Grand-Theft-Auto-Vice-City/5e5b1840-acd5-42b7-a5b9-cf041badf38c/scratchpad/build-task1.log 2>&1; echo exit:$?; grep -m5 "error:" /tmp/claude-1000/-games-steam-steamapps-common-Grand-Theft-Auto-Vice-City/5e5b1840-acd5-42b7-a5b9-cf041badf38c/scratchpad/build-task1.log`

Expected: `exit:0`, no errors. The joystick-detect menu code is already branched `#if defined RW_GL3 && !defined LIBRW_SDL2` vs `#elif defined XINPUT` (MenuScreensCustom.cpp:300-360), so fallout is unlikely. If an error references an XInput-only symbol (`XInputJoy1`, `XINPUT_*`) inside a `GAMEPAD_MENU` block, wrap that statement in `#ifdef XINPUT` preserving the GLFW alternative that surrounds it — do NOT delete game logic.

- [ ] **Step 3: Smoke-test the menu**

Run: `cd "/games/steam/steamapps/common/Grand Theft Auto Vice City" && cp /home/raidon/src/reVC/build/src/reVC ./reVC && timeout 45 ./reVC > /tmp/claude-1000/-games-steam-steamapps-common-Grand-Theft-Auto-Vice-City/5e5b1840-acd5-42b7-a5b9-cf041badf38c/scratchpad/run-task1.log 2>&1; echo exit:$?; grep -ci "segmentation\|assert" /tmp/claude-1000/-games-steam-steamapps-common-Grand-Theft-Auto-Vice-City/5e5b1840-acd5-42b7-a5b9-cf041badf38c/scratchpad/run-task1.log`

Expected: `exit:0` (or 124 if the timeout kills it while running — also fine), grep count `0`. The user confirms visually later; automated check is only "boots, no crash".

- [ ] **Step 4: Commit**

```bash
cd /home/raidon/src/reVC && git add src/core/config.h && git commit -m "Enable GAMEPAD_MENU on Linux

Controller settings page with PS/Xbox/Switch prompt selection and
vibration toggle; all prompt TXDs already ship in gamefiles.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: SDL3 hardware probe (pre-implementation test)

**Files:**
- Create: `/tmp/claude-1000/-games-steam-steamapps-common-Grand-Theft-Auto-Vice-City/5e5b1840-acd5-42b7-a5b9-cf041badf38c/scratchpad/rumbletest.c` (throwaway, not committed)

**Interfaces:**
- Consumes: system SDL3.
- Produces: confirmed facts for Task 3 — the 8BitDo's SDL GUID string, `SDL_GetGamepadType()` value, and that `SDL_RumbleGamepad` physically works.

- [ ] **Step 1: Write the probe**

```c
// rumbletest.c - verify SDL3 sees the pad, prints GUID/type, fires rumble
#include <SDL3/SDL.h>
#include <stdio.h>

int main(void)
{
	if(!SDL_Init(SDL_INIT_GAMEPAD)){ printf("init fail: %s\n", SDL_GetError()); return 1; }
	int count = 0;
	SDL_JoystickID *ids = SDL_GetGamepads(&count);
	printf("gamepads: %d\n", count);
	if(count == 0){ SDL_free(ids); return 1; }
	for(int i = 0; i < count; i++){
		char guid[33];
		SDL_GUIDToString(SDL_GetJoystickGUIDForID(ids[i]), guid, sizeof(guid));
		printf("  [%d] name='%s' guid=%s\n", i, SDL_GetGamepadNameForID(ids[i]), guid);
	}
	SDL_Gamepad *pad = SDL_OpenGamepad(ids[0]);
	SDL_free(ids);
	if(!pad){ printf("open fail: %s\n", SDL_GetError()); return 1; }
	printf("opened: %s type=%d\n", SDL_GetGamepadName(pad), (int)SDL_GetGamepadType(pad));
	int ok = SDL_RumbleGamepad(pad, 0xffff, 0xffff, 1500);
	printf("rumble: %s\n", ok ? "OK - pad should buzz for 1.5s" : SDL_GetError());
	SDL_Delay(2000);
	SDL_CloseGamepad(pad);
	SDL_Quit();
	return ok ? 0 : 1;
}
```

- [ ] **Step 2: Compile and run it**

Run: `cd <scratchpad> && gcc rumbletest.c -o rumbletest $(pkg-config --cflags --libs sdl3) && ./rumbletest`

Expected: `gamepads: 1` (or 2 with the Steam Controller), a GUID line for the 8BitDo, `type=` a nonzero `SDL_GamepadType` (XBOX360=3 or STANDARD=1 depending on firmware mode), `rumble: OK`. Ask the user to confirm the pad buzzed. If `SDL_RumbleGamepad` fails here, STOP — re-plan (the sidecar design assumes SDL3 can rumble this pad).

Also compare the printed SDL GUID against GLFW's: chars 4-7 will differ (SDL3 CRC field), the rest match — this validates the masked-GUID matching in Task 3. GLFW GUID can be printed with a 5-line probe if in doubt: `glfwInit(); glfwGetJoystickGUID(GLFW_JOYSTICK_1)`.

---

### Task 3: SDL3 sidecar module + vibration/type hooks

**Files:**
- Create: `/home/raidon/src/reVC/src/skel/glfw/sdlpad.h`
- Create: `/home/raidon/src/reVC/src/skel/glfw/sdlpad.cpp`
- Modify: `/home/raidon/src/reVC/src/CMakeLists.txt:133-147` (GLFW block)
- Modify: `/home/raidon/src/reVC/src/skel/glfw/glfw.cpp:396-399` (vibration stubs), `glfw.cpp:592` (psTerminate)

**Interfaces:**
- Consumes: `GAMEPAD_MENU` enums from Task 1; `CPad::GetPad(0)->ShakeDur/ShakeFreq` (int16/uint8, Pad.h); `CTimer::GetTimeStepInMilliseconds()`; `PSGLOBAL(joy1id)` (int); `glfwGetJoystickGUID(int)` / `glfwGetJoystickName(int)`.
- Produces: `SdlPad_Init()`, `SdlPad_Shutdown()`, `bool SdlPad_Update(const char *glfwGUID, const char *glfwName)` (true = newly opened pad), `void SdlPad_Rumble(uint16 lowMag, uint16 highMag, uint32 durationMs)`, `int SdlPad_GetControllerType(void)` returning `SDLPAD_TYPE_*`.

- [ ] **Step 1: Add SDL3 to the build**

In `src/CMakeLists.txt`, inside the existing `if(LIBRW_PLATFORM_GL3 AND LIBRW_GL3_GFXLIB STREQUAL "GLFW")` block, after `endif (GLFW_HAS_X11)` and before the block's `endif()`:

```cmake
	find_package(SDL3 CONFIG QUIET)
	if (SDL3_FOUND)
		target_link_libraries(${EXECUTABLE} PRIVATE SDL3::SDL3)
		target_compile_definitions(${EXECUTABLE} PRIVATE SDL3_GAMEPAD)
		message(STATUS "SDL3 found - gamepad rumble and controller type detection enabled")
	else ()
		message(STATUS "SDL3 not found - building without gamepad rumble")
	endif ()
```

Note: sources are gathered by `file(GLOB_RECURSE ...)` (src/CMakeLists.txt:4) — the new .cpp is picked up by re-running cmake configure, which the build step does automatically when CMakeLists.txt changed.

- [ ] **Step 2: Write sdlpad.h**

```cpp
#pragma once

#ifdef SDL3_GAMEPAD

// SDL3 gamepad sidecar: rumble + controller type detection for the pad
// GLFW is polling. GLFW remains the input source; see
// docs/superpowers/specs/2026-07-29-linux-gamepad-support-design.md

enum {
	SDLPAD_TYPE_XBOX360,
	SDLPAD_TYPE_XBOXONE,
	SDLPAD_TYPE_PS3,
	SDLPAD_TYPE_PS4,
	SDLPAD_TYPE_SWITCH,
};

void SdlPad_Init(void);
void SdlPad_Shutdown(void);
// Reconcile with GLFW's active joystick (pass nil when none).
// Returns true when a new gamepad was just opened.
bool SdlPad_Update(const char *glfwGUID, const char *glfwName);
bool SdlPad_IsOpen(void);
void SdlPad_Rumble(uint16 lowMag, uint16 highMag, uint32 durationMs);
int SdlPad_GetControllerType(void);

#endif
```

- [ ] **Step 3: Write sdlpad.cpp**

```cpp
#ifdef SDL3_GAMEPAD

#include <SDL3/SDL.h>

#include "common.h"
#include "sdlpad.h"

static bool sdlInited = false;
static SDL_Gamepad *sdlPad = nil;
static char curGUID[40];	// GLFW GUID of the matched pad, "" = none

// SDL3 embeds a CRC of the joystick name in GUID chars 4-7; GLFW emits
// SDL2-format GUIDs with zeroes there. Compare everything but that field.
static bool
GUIDsMatch(const char *a, const char *b)
{
	if(a == nil || b == nil)
		return false;
	if(strlen(a) != 32 || strlen(b) != 32)
		return false;
	return strncmp(a, b, 4) == 0 && strcmp(a + 8, b + 8) == 0;
}

void
SdlPad_Init(void)
{
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	sdlInited = SDL_Init(SDL_INIT_GAMEPAD);
	if(!sdlInited)
		debug("SdlPad: SDL_Init failed: %s - rumble disabled\n", SDL_GetError());
}

static void
ClosePad(void)
{
	if(sdlPad){
		SDL_CloseGamepad(sdlPad);
		sdlPad = nil;
	}
	curGUID[0] = '\0';
}

void
SdlPad_Shutdown(void)
{
	if(!sdlInited)
		return;
	ClosePad();
	SDL_Quit();
	sdlInited = false;
}

bool
SdlPad_Update(const char *glfwGUID, const char *glfwName)
{
	if(!sdlInited)
		return false;

	SDL_UpdateGamepads();

	if(sdlPad && !SDL_GamepadConnected(sdlPad))
		ClosePad();

	if(glfwGUID == nil){
		ClosePad();
		return false;
	}

	// still tracking the same pad?
	if(sdlPad && strcmp(curGUID, glfwGUID) == 0)
		return false;
	ClosePad();

	int count = 0;
	SDL_JoystickID *ids = SDL_GetGamepads(&count);
	if(ids == nil)
		return false;

	SDL_JoystickID found = 0;
	for(int i = 0; i < count && found == 0; i++){
		char guidstr[33];
		SDL_GUIDToString(SDL_GetJoystickGUIDForID(ids[i]), guidstr, sizeof(guidstr));
		if(GUIDsMatch(guidstr, glfwGUID))
			found = ids[i];
	}
	if(found == 0 && glfwName)
		for(int i = 0; i < count && found == 0; i++){
			const char *name = SDL_GetGamepadNameForID(ids[i]);
			if(name && strcmp(name, glfwName) == 0)
				found = ids[i];
		}
	if(found == 0 && count == 1)
		found = ids[0];
	SDL_free(ids);

	if(found == 0)
		return false;

	sdlPad = SDL_OpenGamepad(found);
	if(sdlPad == nil){
		debug("SdlPad: SDL_OpenGamepad failed: %s\n", SDL_GetError());
		return false;
	}
	strncpy(curGUID, glfwGUID, sizeof(curGUID)-1);
	debug("SdlPad: rumble/type device: %s\n", SDL_GetGamepadName(sdlPad));
	return true;
}

bool
SdlPad_IsOpen(void)
{
	return sdlPad != nil;
}

void
SdlPad_Rumble(uint16 lowMag, uint16 highMag, uint32 durationMs)
{
	static bool wasZero = true;
	if(sdlPad == nil)
		return;
	bool zero = lowMag == 0 && highMag == 0;
	if(zero && wasZero)
		return;
	SDL_RumbleGamepad(sdlPad, lowMag, highMag, durationMs);
	wasZero = zero;
}

int
SdlPad_GetControllerType(void)
{
	if(sdlPad == nil)
		return SDLPAD_TYPE_XBOXONE;
	switch(SDL_GetGamepadType(sdlPad)){
	case SDL_GAMEPAD_TYPE_XBOX360: return SDLPAD_TYPE_XBOX360;
	case SDL_GAMEPAD_TYPE_XBOXONE: return SDLPAD_TYPE_XBOXONE;
	case SDL_GAMEPAD_TYPE_PS3: return SDLPAD_TYPE_PS3;
	case SDL_GAMEPAD_TYPE_PS4:
	case SDL_GAMEPAD_TYPE_PS5: return SDLPAD_TYPE_PS4;
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return SDLPAD_TYPE_SWITCH;
	default: return SDLPAD_TYPE_XBOXONE;
	}
}

#endif
```

- [ ] **Step 4: Hook into glfw.cpp**

In `src/skel/glfw/glfw.cpp`, find the vibration stubs (line ~396):

```cpp
#else
static void _psInitializeVibration() {}
static void _psHandleVibration() {}
#endif
```

Replace with:

```cpp
#elif defined(SDL3_GAMEPAD)
#include "sdlpad.h"

static void _psInitializeVibration() { SdlPad_Init(); }
static void _psHandleVibration()
{
	CPad *pad = CPad::GetPad(0);

	const char *guid = nil, *name = nil;
	if(PSGLOBAL(joy1id) != -1){
		guid = glfwGetJoystickGUID(PSGLOBAL(joy1id));
		name = glfwGetJoystickName(PSGLOBAL(joy1id));
	}
	bool newPad = SdlPad_Update(guid, name);
#ifdef GAMEPAD_MENU
	if(newPad){
		switch(SdlPad_GetControllerType()){
		case SDLPAD_TYPE_XBOX360:
			FrontEndMenuManager.m_PrefsControllerType = CMenuManager::CONTROLLER_XBOX360; break;
		case SDLPAD_TYPE_PS3:
			FrontEndMenuManager.m_PrefsControllerType = CMenuManager::CONTROLLER_DUALSHOCK3; break;
		case SDLPAD_TYPE_PS4:
			FrontEndMenuManager.m_PrefsControllerType = CMenuManager::CONTROLLER_DUALSHOCK4; break;
		case SDLPAD_TYPE_SWITCH:
			FrontEndMenuManager.m_PrefsControllerType = CMenuManager::CONTROLLER_NINTENDO_SWITCH; break;
		default:
			FrontEndMenuManager.m_PrefsControllerType = CMenuManager::CONTROLLER_XBOXONE; break;
		}
	}
#else
	(void)newPad;
#endif

	// mirror the XInput vibration logic (CPad::AffectFromXinput, src/core/Pad.cpp)
	if(pad->ShakeDur < CTimer::GetTimeStepInMilliseconds())
		pad->ShakeDur = 0;
	else
		pad->ShakeDur -= CTimer::GetTimeStepInMilliseconds();
	if(pad->ShakeDur == 0)
		pad->ShakeFreq = 0;

	uint16 mag = (uint16)((float)pad->ShakeFreq / 255.0f * 65535.0f);
	SdlPad_Rumble(mag, mag, pad->ShakeDur);
}
#else
static void _psInitializeVibration() {}
static void _psHandleVibration() {}
#endif
```

(`_psInitializeVibration()` is already called from psInitialize at glfw.cpp:481; `_psHandleVibration()` already called per frame from `CapturePad`, glfw.cpp bottom — no new call sites needed for those. `Pad.h`, `Timer.h`, `Frontend.h` are already included at the top of glfw.cpp.)

Then in `psTerminate` (glfw.cpp:~592), replace:

```cpp
void
psTerminate(void)
{
	return;
}
```

with:

```cpp
void
psTerminate(void)
{
#ifdef SDL3_GAMEPAD
	SdlPad_Shutdown();
#endif
	return;
}
```

- [ ] **Step 5: Build**

Run: `cmake --build /home/raidon/src/reVC/build -j$(nproc) > <scratchpad>/build-task3.log 2>&1; echo exit:$?; grep -m5 "error:" <scratchpad>/build-task3.log`

Expected: cmake reconfigures (CMakeLists changed), prints `SDL3 found - gamepad rumble and controller type detection enabled`, `exit:0`, no errors.

- [ ] **Step 6: Runtime smoke test with pad connected**

Run: `cd "/games/steam/steamapps/common/Grand Theft Auto Vice City" && cp /home/raidon/src/reVC/build/src/reVC ./reVC && timeout 45 ./reVC > <scratchpad>/run-task3.log 2>&1; echo exit:$?; grep -i "SdlPad" <scratchpad>/run-task3.log`

Expected: `SdlPad: rumble/type device: <8BitDo name>` in the log, no crash. If `SdlPad: SDL_Init failed` appears, capture `SDL_GetError` text and investigate before proceeding.

- [ ] **Step 7: Commit**

```bash
cd /home/raidon/src/reVC && git add src/skel/glfw/sdlpad.h src/skel/glfw/sdlpad.cpp src/skel/glfw/glfw.cpp src/CMakeLists.txt && git commit -m "Add SDL3 gamepad sidecar: rumble + controller type detection

GLFW remains the input source; SDL3 (gamepad subsystem only) tracks
the same pad via masked-GUID match and provides SDL_RumbleGamepad
driven by ShakeFreq/ShakeDur (mirrors the XInput path) plus
SDL_GetGamepadType-based automatic prompt selection.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: End-to-end verification and handoff

**Files:**
- Modify: none (verification only; game dir binary already updated in Task 3 Step 6).

**Interfaces:**
- Consumes: everything above.
- Produces: verified feature; user-facing summary.

- [ ] **Step 1: User verification checklist** — present to the user:
  1. Options → Controller: page shows controller type selector; switching type changes help-prompt icons.
  2. With the 8BitDo on, prompts auto-select Xbox style.
  3. Get in a car, hit a wall: pad rumbles; Options → Controller → Vibration OFF stops it.
  4. Turn pad off/on mid-game: input and rumble come back, no crash.
  5. No-pad regression: launch once with the pad powered off — game boots, keyboard/mouse works.

- [ ] **Step 2: Record outcome** — if all pass, update memory (`revc-building-pool-crash.md` gets a sibling note or link) and mark tasks complete. Any failure: return to the failing task, fix, rebuild, re-verify before claiming done.
