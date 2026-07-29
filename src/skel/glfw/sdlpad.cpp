#ifdef SDL3_GAMEPAD

#include <SDL3/SDL.h>

#include "common.h"
#include "sdlpad.h"

static bool sdlInited = false;
static SDL_Gamepad *sdlPad = nil;
static char curGUID[40];	// GLFW GUID of the matched pad, "" = none
static char lastGlfwGUID[40];	// GLFW GUID last seen, for rescan throttle
static int rescanCooldown = 0;	// calls left to skip before next failed-match rescan

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

static int
HexNibble(char c)
{
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	return 0;
}

// Parse a little-endian 16-bit value from 4 hex chars (2 hex-byte pairs,
// least-significant byte first) -- how GLFW encodes vendor/product in its
// GUID string.
static uint16
ParseGUIDWordLE(const char *hex4)
{
	uint8 b0 = (uint8)((HexNibble(hex4[0]) << 4) | HexNibble(hex4[1]));
	uint8 b1 = (uint8)((HexNibble(hex4[2]) << 4) | HexNibble(hex4[3]));
	return (uint16)((b1 << 8) | b0);
}

void
SdlPad_Init(void)
{
	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	sdlInited = SDL_Init(SDL_INIT_GAMEPAD);
	if(!sdlInited){
		debug("SdlPad: SDL_Init failed: %s - rumble disabled\n", SDL_GetError());
		return;
	}
	// we poll SDL_UpdateGamepads() ourselves; don't let SDL pile up an
	// event queue nobody drains.
	SDL_SetGamepadEventsEnabled(false);
	SDL_SetJoystickEventsEnabled(false);
}

static void
ClosePad(void)
{
	if(sdlPad){
		SDL_CloseGamepad(sdlPad);
		sdlPad = nil;
	}
	curGUID[0] = '\0';
	rescanCooldown = 0;
}

void
SdlPad_Shutdown(void)
{
	if(!sdlInited)
		return;
	ClosePad();
	SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
	SDL_Quit();
	sdlInited = false;
}

bool
SdlPad_Update(const char *glfwGUID, const char *glfwName)
{
	if(!sdlInited)
		return false;

	// SDL's udev-based hotplug detection runs off SDL_PumpEvents, not
	// SDL_UpdateGamepads -- without this the device list is frozen at init.
	SDL_PumpEvents();
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

	// a different GLFW pad than the one we last tried to match resets the
	// failed-match throttle -- it's a new device, worth an immediate scan.
	if(strcmp(lastGlfwGUID, glfwGUID) != 0){
		strncpy(lastGlfwGUID, glfwGUID, sizeof(lastGlfwGUID)-1);
		lastGlfwGUID[sizeof(lastGlfwGUID)-1] = '\0';
		rescanCooldown = 0;
	}

	// don't hammer SDL_GetGamepads()/matching every frame after a failed
	// match; retry only every ~120 calls (~2s at 60fps). ClosePad() (device
	// unplugged) resets the cooldown to zero so a real disconnect/reconnect
	// is picked up immediately.
	if(rescanCooldown > 0){
		rescanCooldown--;
		return false;
	}

	ClosePad();

	int count = 0;
	SDL_JoystickID *ids = SDL_GetGamepads(&count);
	if(ids == nil){
		rescanCooldown = 120;
		return false;
	}

	uint16 glfwVendor = 0, glfwProduct = 0;
	if(strlen(glfwGUID) >= 20){
		glfwVendor = ParseGUIDWordLE(glfwGUID + 8);
		glfwProduct = ParseGUIDWordLE(glfwGUID + 16);
	}

	SDL_JoystickID found = 0;

	// (a) exact masked-GUID match
	for(int i = 0; i < count && found == 0; i++){
		char guidstr[33];
		SDL_GUIDToString(SDL_GetJoystickGUIDForID(ids[i]), guidstr, sizeof(guidstr));
		if(GUIDsMatch(guidstr, glfwGUID))
			found = ids[i];
	}
	// (b) VID/PID match -- SDL's HIDAPI-driven pads carry a nonzero driver
	// signature in the GUID tail (and can report a different name), so the
	// masked-GUID check above misses them.
	if(found == 0 && glfwVendor != 0 && glfwProduct != 0)
		for(int i = 0; i < count && found == 0; i++){
			if(SDL_GetJoystickVendorForID(ids[i]) == glfwVendor &&
			   SDL_GetJoystickProductForID(ids[i]) == glfwProduct)
				found = ids[i];
		}
	// (c) name match
	if(found == 0 && glfwName)
		for(int i = 0; i < count && found == 0; i++){
			const char *name = SDL_GetGamepadNameForID(ids[i]);
			if(name && strcmp(name, glfwName) == 0)
				found = ids[i];
		}
	// (d) sole-gamepad fallback
	if(found == 0 && count == 1)
		found = ids[0];
	SDL_free(ids);

	if(found == 0){
		rescanCooldown = 120;
		return false;
	}

	sdlPad = SDL_OpenGamepad(found);
	if(sdlPad == nil){
		debug("SdlPad: SDL_OpenGamepad failed: %s\n", SDL_GetError());
		rescanCooldown = 120;
		return false;
	}
	strncpy(curGUID, glfwGUID, sizeof(curGUID)-1);
	curGUID[sizeof(curGUID)-1] = '\0';
	debug("SdlPad: rumble/type device: %s\n", SDL_GetGamepadName(sdlPad));
	return true;
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
