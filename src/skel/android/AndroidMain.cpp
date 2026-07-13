//
// Created by mrxenginner on 13/07/2025.
//

#if defined ANDROID

#include "AndroidMain.h"
#include "JavaWrapper.h"
#include "logger/log.h"
#include <SDL_hints.h>
#include <SDL_main.h>
#include <SignalHandler.h>
#include <patch/patch.h>

JavaVM* javaVM = NULL;
char* StorageRootBuffer = NULL;
uintptr_t g_libREVC = NULL;

bool AndWrapper::AppInitialized = false;
bool AndWrapper::AppStarted = false;

JAVA_WRAPPER Java_org_libsdl_app_SDLActivity_nativeSetupJNI(JNIEnv* env, jobject initGame)
{
    if(!javaVM) {
        env->GetJavaVM(&javaVM);
    }

    g_pJavaWrapper = new CJavaWrapper(env, initGame);
}

JAVA_WRAPPER Java_com_revc_game_core_REVC_setGamePath(JNIEnv *env, jobject obj, jstring value)
{
    const char* root = env->GetStringUTFChars(value, NULL);
    setenv("STORAGE_ROOT", root, 1);

    StorageRootBuffer = getenv("STORAGE_ROOT");
    debug("Storage Root: %s", StorageRootBuffer);

    env->ReleaseStringUTFChars(value, root);
}

bool AndWrapper::InitLibraries() {
	g_libREVC = Patch::FindLib("libreVC.so");

	if (!g_libREVC) {
		Logger::Log("[ERROR]: Required libraries not found!");
		return false;
	}

	Logger::Log("[INFO]: libreVC base: 0x%X", g_libREVC);
	return true;
}

void* AndWrapper::GetJNI() {
    return CJavaWrapper::GetEnv();
}

void* AndWrapper::GetJNIFunc() {
    return CJavaWrapper::GetEnv();
}

JNI_WRAPPER int InitializeGame() {
    debug("Initialize Game");

    if (!AndWrapper::InitLibraries()) return NULL;

    int argc = 0;
    char* argv[1] = { nullptr };

    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

    CrashHandler::SetupSignalHandlers();

    int result = SDL_main(argc, argv);
    return result;
}

#endif