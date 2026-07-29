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
