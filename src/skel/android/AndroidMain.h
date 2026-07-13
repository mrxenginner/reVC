//
// Created by mrxenginner on 13/07/2025.
//

#ifndef REVC_ANDROIDMAIN_H
#define REVC_ANDROIDMAIN_H

#if defined ANDROID

#include "common.h"

#define JNI_WRAPPER extern "C" __attribute__ ((visibility("default")))
#define JAVA_WRAPPER extern "C" JNIEXPORT void JNICALL

extern uintptr_t g_libREVC;
extern char* StorageRootBuffer;

namespace AndWrapper {
    extern bool AppInitialized;
    extern bool AppStarted;

    bool InitLibraries();
    void* GetJNI();
    void* GetJNIFunc();
}
#endif

#endif //REVC_ANDROIDMAIN_H
