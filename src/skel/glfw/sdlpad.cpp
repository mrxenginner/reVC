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
