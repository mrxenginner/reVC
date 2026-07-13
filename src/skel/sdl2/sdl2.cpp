#if defined RW_GL3 && defined LIBRW_SDL2

long _dwOperatingSystemVersion;

#include <sys/sysinfo.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <unordered_map>

#include "common.h"
#include "rwcore.h"
#include "skeleton.h"
#include "platform.h"
#include "crossplatform.h"

#include "main.h"
#include "FileMgr.h"
#include "Text.h"
#include "Pad.h"
#include "Timer.h"
#include "DMAudio.h"
#include "ControllerConfig.h"
#include "Frontend.h"
#include "Game.h"
#include "PCSave.h"
#include "MemoryCard.h"
#include "Sprite2d.h"
#include "AnimViewer.h"
#include "Font.h"
#include "MemoryMgr.h"

#if defined ANDROID
#include "JavaWrapper.h"
extern char* StorageRootBuffer;
#endif

#define MAX_SUBSYSTEMS		(16)

rw::EngineOpenParams openParams;

static RwBool		  ForegroundApp = TRUE;
static RwBool		  WindowIconified = FALSE;
static RwBool		  WindowFocused = TRUE;

static RwBool		  RwInitialised = FALSE;

static RwSubSystemInfo GsubSysInfo[MAX_SUBSYSTEMS];
static RwInt32		GnumSubSystems = 0;
static RwInt32		GcurSel = 0, GcurSelVM = 0;

static RwBool useDefault;

// What is that for anyway?
#ifndef IMPROVED_VIDEOMODE
static RwBool defaultFullscreenRes = TRUE;
#else
static RwBool defaultFullscreenRes = FALSE;
static RwInt32 bestWndMode = -1;
#endif

static psGlobalType PsGlobal;

static SDL_GameController* gamepad1 = nullptr;
static SDL_GameController* gamepad2 = nullptr;

#define PSGLOBAL(var) (((psGlobalType *)(RsGlobal.ps))->var)

size_t _dwMemAvailPhys;
RwUInt32 gGameState;

#ifdef DETECT_JOYSTICK_MENU
char gSelectedJoystickName[128] = "";
#endif

/*
 *****************************************************************************
 */
void _psCreateFolder(const char *path)
{
#if defined(ANDROID)
	const char* pathroot = StorageRootBuffer;
	char dbPath[1024];
	if (path[0] == '/' && pathroot) {
		// Path is already absolute, use as-is
		snprintf(dbPath, sizeof(dbPath), "%s", path);
	} else if (pathroot) {
		snprintf(dbPath, sizeof(dbPath), "%s/%s", pathroot, path);
	} else {
		snprintf(dbPath, sizeof(dbPath), "%s", path);
	}
	mkdir(dbPath, 0755);
	debug("Creating Folder Path: %s", dbPath);
#else
    debug("Creating Folder Path: %s", path);
    struct stat info;
    char fullpath[PATH_MAX];
    realpath(path, fullpath);

    if (lstat(fullpath, &info) != 0) {
        if (errno == ENOENT || (errno != EACCES && !S_ISDIR(info.st_mode))) {
		debug("Creating Folder FullPath: %s", fullpath);
            mkdir(fullpath, 0755);
        }
    }
#endif
}

/*
 *****************************************************************************
 */
const char *_psGetUserFilesFolder()
{
    static char szUserFiles[256];
#if defined(ANDROID)
    const char *root = getenv("STORAGE_ROOT");
    if (root) {
        snprintf(szUserFiles, sizeof(szUserFiles), "%s/userfiles", root);
        _psCreateFolder(szUserFiles);
    } else {
        strcpy(szUserFiles, "userfiles");
    }
#else
    strcpy(szUserFiles, "userfiles");
    _psCreateFolder(szUserFiles);
#endif
    return szUserFiles;
}

/*
 *****************************************************************************
 */
RwBool
psCameraBeginUpdate(RwCamera *camera)
{
    if ( !RwCameraBeginUpdate(Scene.camera) )
    {
        ForegroundApp = FALSE;
        RsEventHandler(rsACTIVATE, (void *)FALSE);
        return FALSE;
    }

    return TRUE;
}

/*
 *****************************************************************************
 */
void
psCameraShowRaster(RwCamera *camera)
{
#ifdef LEGACY_MENU_OPTIONS
    if (FrontEndMenuManager.m_PrefsVsync || FrontEndMenuManager.m_bMenuActive)
#else
        if (FrontEndMenuManager.m_PrefsFrameLimiter || FrontEndMenuManager.m_bMenuActive)
#endif
        RwCameraShowRaster(camera, PSGLOBAL(window), rwRASTERFLIPWAITVSYNC);
    else
        RwCameraShowRaster(camera, PSGLOBAL(window), rwRASTERFLIPDONTWAIT);

    return;
}

/*
 *****************************************************************************
 */
RwImage *
psGrabScreen(RwCamera *pCamera)
{
#ifndef LIBRW
    RwRaster *pRaster = RwCameraGetRaster(pCamera);
	if (RwImage *pImage = RwImageCreate(pRaster->width, pRaster->height, 32)) {
		RwImageAllocatePixels(pImage);
		RwImageSetFromRaster(pImage, pRaster);
		return pImage;
	}
#else
    rw::Image *image = RwCameraGetRaster(pCamera)->toImage();
    image->removeMask();
    if(image)
        return image;
#endif
    return nil;
}

/*
 *****************************************************************************
 */
double
psTimer(void)
{
    struct timespec start;
#if defined(CLOCK_MONOTONIC_RAW)
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
#elif defined(CLOCK_MONOTONIC_FAST)
    clock_gettime(CLOCK_MONOTONIC_FAST, &start);
#else
	clock_gettime(CLOCK_MONOTONIC, &start);
#endif
    return start.tv_sec * 1000.0 + start.tv_nsec/1000000.0;
}

/*
 *****************************************************************************
 */
void
psMouseSetPos(RwV2d *pos)
{
    SDL_WarpMouseInWindow(PSGLOBAL(window), pos->x, pos->y);
    PSGLOBAL(lastMousePos.x) = (RwInt32)pos->x;
    PSGLOBAL(lastMousePos.y) = (RwInt32)pos->y;
}

/*
 *****************************************************************************
 */
RwMemoryFunctions*
psGetMemoryFunctions(void)
{
#ifdef USE_CUSTOM_ALLOCATOR
    return &memFuncs;
#else
    return nil;
#endif
}

/*
 *****************************************************************************
 */
RwBool
psInstallFileSystem(void)
{
    return (TRUE);
}

/*
 *****************************************************************************
 */
RwBool
psNativeTextureSupport(void)
{
    return true;
}

/*
 *****************************************************************************
 */
#ifdef UNDER_CE
#define CMDSTR	LPWSTR
#else
#define CMDSTR	LPSTR
#endif

/*
 *****************************************************************************
 */

static void _psInitializeVibration() {}
static void _psHandleVibration() {}

/*
 *****************************************************************************
 */
RwBool
psInitialize(void)
{
    PsGlobal.lastMousePos.x = PsGlobal.lastMousePos.y = 0.0f;

    RsGlobal.ps = &PsGlobal;

    PsGlobal.fullScreen = FALSE;
    PsGlobal.cursorIsInWindow = FALSE;
    WindowFocused = TRUE;
    WindowIconified = FALSE;

    PsGlobal.joy1id	= -1;
    PsGlobal.joy2id	= -1;

    CFileMgr::Initialise();

#ifdef PS2_MENU
    CPad::Initialise();
	CPad::GetPad(0)->Mode = 0;

	CGame::frenchGame = false;
	CGame::germanGame = false;
	CGame::nastyGame = true;
	CMenuManager::m_PrefsAllowNastyGame = true;

	// Mandatory for Linux(Unix? Posix?) to set lang. to environment lang.
	setlocale(LC_ALL, "");

	char *systemLang, *keyboardLang;

	systemLang = setlocale (LC_ALL, NULL);
	keyboardLang = setlocale (LC_CTYPE, NULL);

	short lang;
	lang = !strncmp(systemLang, "fr_",3) ? LANG_FRENCH :
					!strncmp(systemLang, "de_",3) ? LANG_GERMAN :
					!strncmp(systemLang, "en_",3) ? LANG_ENGLISH :
					!strncmp(systemLang, "it_",3) ? LANG_ITALIAN :
					!strncmp(systemLang, "es_",3) ? LANG_SPANISH :
					LANG_OTHER;

	if ( lang  == LANG_ITALIAN )
		CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_ITALIAN;
	else if ( lang  == LANG_SPANISH )
		CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_SPANISH;
	else if ( lang  == LANG_GERMAN )
	{
		CGame::germanGame = true;
		CGame::nastyGame = false;
		CMenuManager::m_PrefsAllowNastyGame = false;
		CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_GERMAN;
	}
	else if ( lang  == LANG_FRENCH )
	{
		CGame::frenchGame = true;
		CGame::nastyGame = false;
		CMenuManager::m_PrefsAllowNastyGame = false;
		CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_FRENCH;
	}
	else
		CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_AMERICAN;

	FrontEndMenuManager.InitialiseMenuContentsAfterLoadingGame();

	TheMemoryCard.Init();
#else
    C_PcSave::SetSaveDirectory(_psGetUserFilesFolder());

    InitialiseLanguage();

#endif

    _psInitializeVibration();

    gGameState = GS_START_UP;
    TRACE("gGameState = GS_START_UP");
    _dwOperatingSystemVersion = OS_WINXP; // To fool other classes

#ifndef PS2_MENU
    FrontEndMenuManager.LoadSettings();
#endif

#ifndef __APPLE__
    struct sysinfo systemInfo;
    sysinfo(&systemInfo);
    _dwMemAvailPhys = systemInfo.freeram;
    debug("Physical memory size %u\n", systemInfo.totalram);
    debug("Available physical memory %u\n", systemInfo.freeram);
#else
    uint64_t size = 0;
	uint64_t page_size = 0;
	size_t uint64_len = sizeof(uint64_t);
	size_t ull_len = sizeof(unsigned long long);
	sysctl((int[]){CTL_HW, HW_PAGESIZE}, 2, &page_size, &ull_len, NULL, 0);
	sysctl((int[]){CTL_HW, HW_MEMSIZE}, 2, &size, &uint64_len, NULL, 0);
	vm_statistics_data_t vm_stat;
	mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&vm_stat, &count);
	_dwMemAvailPhys = (uint64_t)(vm_stat.free_count * page_size);
	debug("Physical memory size %llu\n", _dwMemAvailPhys);
	debug("Available physical memory %llu\n", size);
#endif

    TheText.Unload();

    return TRUE;
}


/*
 *****************************************************************************
 */
void
psTerminate(void)
{
    return;
}

/*
 *****************************************************************************
 */
static RwChar **_VMList;

RwInt32 _psGetNumVideModes()
{
    return RwEngineGetNumVideoModes();
}

/*
 *****************************************************************************
 */
RwBool _psFreeVideoModeList()
{
    RwInt32 numModes;
    RwInt32 i;

    numModes = _psGetNumVideModes();

    if ( _VMList == nil )
        return TRUE;

    for ( i = 0; i < numModes; i++ )
    {
        RwFree(_VMList[i]);
    }

    RwFree(_VMList);

    _VMList = nil;

    return TRUE;
}

/*
 *****************************************************************************
 */
RwChar **_psGetVideoModeList()
{
    RwInt32 numModes;
    RwInt32 i;

    if ( _VMList != nil )
    {
        return _VMList;
    }

    numModes = RwEngineGetNumVideoModes();

    _VMList = (RwChar **)RwCalloc(numModes, sizeof(RwChar*));

    for ( i = 0; i < numModes; i++	)
    {
        RwVideoMode			vm;

        RwEngineGetVideoModeInfo(&vm, i);

        if ( vm.flags & rwVIDEOMODEEXCLUSIVE )
        {
            _VMList[i] = (RwChar*)RwCalloc(100, sizeof(RwChar));
            rwsprintf(_VMList[i],"%d X %d X %d", vm.width, vm.height, vm.depth);
        }
        else
            _VMList[i] = nil;
    }

    return _VMList;
}

/*
 *****************************************************************************
 */
void _psSelectScreenVM(RwInt32 videoMode)
{
    RwTexDictionarySetCurrent( nil );

    FrontEndMenuManager.UnloadTextures();

    if (!_psSetVideoMode(RwEngineGetCurrentSubSystem(), videoMode))
    {
        RsGlobal.quit = TRUE;
        printf("ERROR: Failed to select new screen resolution\n");
    }
    else
        FrontEndMenuManager.LoadAllTextures();
}

/*
 *****************************************************************************
 */

RwBool IsForegroundApp()
{
    return !!ForegroundApp;
}
/*
 *****************************************************************************
 */
RwBool
psSelectDevice()
{
    RwVideoMode			vm;
    RwInt32				subSysNum;
    RwInt32				AutoRenderer = 0;

    RwBool modeFound = FALSE;


    if (!useDefault)
    {
        GnumSubSystems = RwEngineGetNumSubSystems();
        if (!GnumSubSystems)
        {
            return FALSE;
        }

        /* Just to be sure ... */
        GnumSubSystems = (GnumSubSystems > MAX_SUBSYSTEMS) ? MAX_SUBSYSTEMS : GnumSubSystems;

        /* Get the names of all the sub systems */
        for (subSysNum = 0; subSysNum < GnumSubSystems; subSysNum++)
        {
            RwEngineGetSubSystemInfo(&GsubSysInfo[subSysNum], subSysNum);
        }

        /* Get the default selection */
        GcurSel = RwEngineGetCurrentSubSystem();
#ifdef IMPROVED_VIDEOMODE
        if (FrontEndMenuManager.m_nPrefsSubsystem < GnumSubSystems)
            GcurSel = FrontEndMenuManager.m_nPrefsSubsystem;
#endif
    }

    /* Set the driver to use the correct sub system */
    if (!RwEngineSetSubSystem(GcurSel))
    {
        return FALSE;
    }

#ifdef IMPROVED_VIDEOMODE
    FrontEndMenuManager.m_nPrefsSubsystem = GcurSel;
#endif

#ifndef IMPROVED_VIDEOMODE
    if (!useDefault)
	{
		if (_psGetVideoModeList()[FrontEndMenuManager.m_nDisplayVideoMode] && FrontEndMenuManager.m_nDisplayVideoMode)
		{
			FrontEndMenuManager.m_nPrefsVideoMode = FrontEndMenuManager.m_nDisplayVideoMode;
			GcurSelVM = FrontEndMenuManager.m_nDisplayVideoMode;
		}
		else
		{
#ifdef DEFAULT_NATIVE_RESOLUTION
			// get the native video mode
			HDC hDevice = GetDC(NULL);
			int w = GetDeviceCaps(hDevice, HORZRES);
			int h = GetDeviceCaps(hDevice, VERTRES);
			int d = GetDeviceCaps(hDevice, BITSPIXEL);
#else
			const int w = 640;
			const int h = 480;
			const int d = 16;
#endif
			while ( !modeFound && GcurSelVM < RwEngineGetNumVideoModes() )
			{
				RwEngineGetVideoModeInfo(&vm, GcurSelVM);
				if ( defaultFullscreenRes	&& vm.width	 != w
											|| vm.height != h
											|| vm.depth	 != d
											|| !(vm.flags & rwVIDEOMODEEXCLUSIVE) )
					++GcurSelVM;
				else
					modeFound = TRUE;
			}

			if ( !modeFound )
			{
#ifdef DEFAULT_NATIVE_RESOLUTION
				GcurSelVM = 1;
#else
				printf("WARNING: Cannot find 640x480 video mode, selecting device cancelled\n");
				return FALSE;
#endif
			}
		}
	}
#else
    if (!useDefault)
    {
        if(FrontEndMenuManager.m_nPrefsWidth == 0 ||
           FrontEndMenuManager.m_nPrefsHeight == 0 ||
           FrontEndMenuManager.m_nPrefsDepth == 0){
            // Defaults if nothing specified
            SDL_DisplayMode mode;
            // TODO how to get displayIndex for the current display?
            SDL_GetCurrentDisplayMode(0, &mode);
            FrontEndMenuManager.m_nPrefsWidth = mode.w;
            FrontEndMenuManager.m_nPrefsHeight = mode.h;
            FrontEndMenuManager.m_nPrefsDepth = 32;
            FrontEndMenuManager.m_nPrefsWindowed = 0;
        }

        // Find the videomode that best fits what we got from the settings file
        RwInt32 bestFsMode = -1;
        RwInt32 bestWidth = -1;
        RwInt32 bestHeight = -1;
        RwInt32 bestDepth = -1;
        for(GcurSelVM = 0; GcurSelVM < RwEngineGetNumVideoModes(); GcurSelVM++){
            RwEngineGetVideoModeInfo(&vm, GcurSelVM);

            if (!(vm.flags & rwVIDEOMODEEXCLUSIVE)){
                bestWndMode = GcurSelVM;
            } else {
                // try the largest one that isn't larger than what we wanted
                if(vm.width >= bestWidth && vm.width <= FrontEndMenuManager.m_nPrefsWidth &&
                   vm.height >= bestHeight && vm.height <= FrontEndMenuManager.m_nPrefsHeight &&
                   vm.depth >= bestDepth && vm.depth <= FrontEndMenuManager.m_nPrefsDepth){
                    bestWidth = vm.width;
                    bestHeight = vm.height;
                    bestDepth = vm.depth;
                    bestFsMode = GcurSelVM;
                }
            }
        }

        if(bestFsMode < 0){
            // Fallback to windowed mode — happens on Android/GLES where
            // there are no SDL fullscreen display modes
            bestFsMode = bestWndMode >= 0 ? bestWndMode : 0;
            FrontEndMenuManager.m_nPrefsWindowed = 1;
        }
        GcurSelVM = bestFsMode;

        FrontEndMenuManager.m_nDisplayVideoMode = GcurSelVM;
        FrontEndMenuManager.m_nPrefsVideoMode = FrontEndMenuManager.m_nDisplayVideoMode;

        FrontEndMenuManager.m_nSelectedScreenMode = FrontEndMenuManager.m_nPrefsWindowed;
    }
#endif

    RwEngineGetVideoModeInfo(&vm, GcurSelVM);

#ifdef IMPROVED_VIDEOMODE
    if (FrontEndMenuManager.m_nPrefsWindowed)
        GcurSelVM = bestWndMode;

    // Now GcurSelVM is 0 but vm has sizes(and fullscreen flag) of the video mode we want, that's why we changed the rwVIDEOMODEEXCLUSIVE conditions below
    FrontEndMenuManager.m_nPrefsWidth = vm.width;
    FrontEndMenuManager.m_nPrefsHeight = vm.height;
    FrontEndMenuManager.m_nPrefsDepth = vm.depth;
#endif

#ifndef PS2_MENU
    FrontEndMenuManager.m_nCurrOption = 0;
#endif

    /* Set up the video mode and set the apps window
    * dimensions to match */
    if (!RwEngineSetVideoMode(GcurSelVM))
    {
        return FALSE;
    }
    /*
    TODO
    if (vm.flags & rwVIDEOMODEEXCLUSIVE)
    {
        debug("%dx%dx%d", vm.width, vm.height, vm.depth);

        UINT refresh = GetBestRefreshRate(vm.width, vm.height, vm.depth);

        if ( refresh != (UINT)-1 )
        {
            debug("refresh %d", refresh);
            RwD3D8EngineSetRefreshRate((RwUInt32)refresh);
        }
    }
    */
#ifndef IMPROVED_VIDEOMODE
    if (vm.flags & rwVIDEOMODEEXCLUSIVE)
	{
		RsGlobal.maximumWidth = vm.width;
		RsGlobal.maximumHeight = vm.height;
		RsGlobal.width = vm.width;
		RsGlobal.height = vm.height;

		PSGLOBAL(fullScreen) = TRUE;
	}
#else
    RsGlobal.maximumWidth = FrontEndMenuManager.m_nPrefsWidth;
    RsGlobal.maximumHeight = FrontEndMenuManager.m_nPrefsHeight;
    RsGlobal.width = FrontEndMenuManager.m_nPrefsWidth;
    RsGlobal.height = FrontEndMenuManager.m_nPrefsHeight;

    PSGLOBAL(fullScreen) = !FrontEndMenuManager.m_nPrefsWindowed;
#endif

#ifdef MULTISAMPLING
    RwD3D8EngineSetMultiSamplingLevels(1 << FrontEndMenuManager.m_nPrefsMSAALevel);
#endif
    return TRUE;
}

bool IsThisJoystickBlacklisted(int i)
{
#ifndef DETECT_JOYSTICK_MENU
    return false;
#else
    if (SDL_IsGameController(i))
		return false;

	const char* joyname = SDL_JoystickNameForIndex(i);

	if (gSelectedJoystickName[0] != '\0'
			&& strncmp(joyname, gSelectedJoystickName, strlen(gSelectedJoystickName)) == 0) {
		return false;
	}

	return true;
#endif
}

void _InputInitialiseJoys()
{
    PSGLOBAL(joy1id) = -1;
    PSGLOBAL(joy2id) = -1;

    // Load our gamepad mappings
    const char* EnvControlConfig = getenv("SDL_GAMECONTROLLERCONFIG_FILE");

    if (EnvControlConfig != nil) {
        if (SDL_GameControllerAddMappingsFromFile(EnvControlConfig) <= 0) {
            Error("Could not load custom controller mapping (SDL_GAMECONTROLLERCONFIG_FILE env variable\n)");
        }
    } else {
#if defined ANDROID
        const char* pathRoot = getenv("STORAGE_ROOT");
        char SDL_GAMEPAD_DB_PATH[MAX_PATH];
        snprintf(SDL_GAMEPAD_DB_PATH, sizeof(SDL_GAMEPAD_DB_PATH), "%s/%s", pathRoot, "gamecontrollerdb.txt");
#else
        const char* SDL_GAMEPAD_DB_PATH = "gamecontrollerdb.txt";
#endif
        if (SDL_GameControllerAddMappingsFromFile(SDL_GAMEPAD_DB_PATH) <= 0) {
            debug ("You don't seem to have copied %s file from reVC/gamefiles "
                   "to GTA: Vice City directory. Some gamepads may not be recognized.\n",
                   SDL_GAMEPAD_DB_PATH);
        }
    }

    // Detect already-connected joysticks during init.
    // Needed on Android where gamepad may already be connected before SDL init.
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (!IsThisJoystickBlacklisted(i)) {
            if (PSGLOBAL(joy1id) == -1)
                PSGLOBAL(joy1id) = i;
            else if (PSGLOBAL(joy2id) == -1)
                PSGLOBAL(joy2id) = i;
            else
                break;
        }
    }

    if (PSGLOBAL(joy1id) != -1) {
        SDL_Joystick* joy1 = SDL_JoystickOpen(PSGLOBAL(joy1id));
        int count = SDL_JoystickNumButtons(joy1);
        SDL_JoystickClose(joy1);
#ifdef DETECT_JOYSTICK_MENU
        strncpy(gSelectedJoystickName, SDL_JoystickNameForIndex(PSGLOBAL(joy1id)), sizeof(gSelectedJoystickName));
#endif
        ControlsManager.InitDefaultControlConfigJoyPad(count);
    }
}

#if 0
TODO SDL2
-long _InputInitialiseMouse()
+int lastCursorMode = GLFW_CURSOR_HIDDEN;
+long _InputInitialiseMouse(bool exclusive)
 {
-	glfwSetInputMode(PSGLOBAL(window), GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
+	// Disabled = keep cursor centered and hide
+	lastCursorMode = exclusive ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_HIDDEN;
+	glfwSetInputMode(PSGLOBAL(window), GLFW_CURSOR, lastCursorMode);
 	return 0;
 }
#endif

long _InputInitialiseMouse(bool exclusive)
{
    // TODO SDL2 what to do about exclusive?
    SDL_ShowCursor(SDL_DISABLE);
    return 0;
}

void _InputShutdownMouse()
{
    // Not needed
}

bool _InputMouseNeedsExclusive()
{
#if 0
    TODO SDL2
	// That was the cause of infamous mouse bug on Win.
	RwVideoMode vm;
	RwEngineGetVideoModeInfo(&vm, GcurSelVM);

	// If windowed, free the cursor on menu(where this func. is called and DISABLED-HIDDEN transition is done accordingly)
	// If it's fullscreen, be sure that it didn't stuck on HIDDEN.
	return !(vm.flags & rwVIDEOMODEEXCLUSIVE) || lastCursorMode == GLFW_CURSOR_HIDDEN;
#endif
    return false;
}

void psPostRWinit(void)
{
    RwVideoMode vm;
    RwEngineGetVideoModeInfo(&vm, GcurSelVM);

    _InputInitialiseJoys();
    _InputInitialiseMouse(false);

    if(!(vm.flags & rwVIDEOMODEEXCLUSIVE))
        SDL_SetWindowSize(PSGLOBAL(window), RsGlobal.maximumWidth, RsGlobal.maximumHeight);

    // Make sure all keys are released
    CPad::GetPad(0)->Clear(true);
    CPad::GetPad(1)->Clear(true);
}

/*
 *****************************************************************************
 */
RwBool _psSetVideoMode(RwInt32 subSystem, RwInt32 videoMode)
{
    RwInitialised = FALSE;

    RsEventHandler(rsRWTERMINATE, nil);

    GcurSel = subSystem;
    GcurSelVM = videoMode;

    useDefault = TRUE;

    if (RsEventHandler(rsRWINITIALIZE, &openParams) == rsEVENTERROR)
        return FALSE;

    RwInitialised = TRUE;
    useDefault = FALSE;

    RwRect r;

    r.x = 0;
    r.y = 0;
    r.w = RsGlobal.maximumWidth;
    r.h = RsGlobal.maximumHeight;

    RsEventHandler(rsCAMERASIZE, &r);

    psPostRWinit();

    return TRUE;
}


/*
 *****************************************************************************
 */
static RwChar **
CommandLineToArgv(RwChar *cmdLine, RwInt32 *argCount)
{
    RwInt32 numArgs = 0;
    RwBool inArg, inString;
    RwInt32 i, len;
    RwChar *res, *str, **aptr;

    len = strlen(cmdLine);

    /*
     * Count the number of arguments...
     */
    inString = FALSE;
    inArg = FALSE;

    for(i=0; i<=len; i++)
    {
        if( cmdLine[i] == '"' )
        {
            inString = !inString;
        }

        if( (cmdLine[i] <= ' ' && !inString) || i == len )
        {
            if (inArg)
            {
                inArg = FALSE;

                numArgs++;
            }
        }
        else if( !inArg )
        {
            inArg = TRUE;
        }
    }

    /*
     * Allocate memory for result...
     */
    res = (RwChar *)malloc(sizeof(RwChar *) * numArgs + len + 1);
    str = res + sizeof(RwChar *) * numArgs;
    aptr = (RwChar **)res;

    strcpy(str, cmdLine);

    /*
     * Walk through cmdLine again this time setting pointer to each arg...
     */
    inArg = FALSE;
    inString = FALSE;

    for(i=0; i<=len; i++)
    {
        if( cmdLine[i] == '"' )
        {
            inString = !inString;
        }

        if( (cmdLine[i] <= ' ' && !inString) || i == len )
        {
            if (inArg)
            {
                if( str[i-1] == '"' )
                {
                    str[i-1] = '\0';
                }
                else
                {
                    str[i] = '\0';
                }

                inArg = FALSE;
            }
        }
        else if (!inArg && cmdLine[i] != '"')
        {
            inArg = TRUE;

            *aptr++ = &str[i];
        }
    }

    *argCount = numArgs;

    return (RwChar **)res;
}

/*
 *****************************************************************************
 */
void InitialiseLanguage()
{
    // Mandatory for Linux(Unix? Posix?) to set lang. to environment lang.
    setlocale(LC_ALL, "");

    char *systemLang, *keyboardLang;

    systemLang = setlocale (LC_ALL, NULL);
    keyboardLang = setlocale (LC_CTYPE, NULL);

    short primUserLCID, primSystemLCID;
    primUserLCID = primSystemLCID = !strncmp(systemLang, "fr_",3) ? LANG_FRENCH :
                                    !strncmp(systemLang, "de_",3) ? LANG_GERMAN :
                                    !strncmp(systemLang, "en_",3) ? LANG_ENGLISH :
                                    !strncmp(systemLang, "it_",3) ? LANG_ITALIAN :
                                    !strncmp(systemLang, "es_",3) ? LANG_SPANISH :
                                    LANG_OTHER;

    short primLayout = !strncmp(keyboardLang, "fr_",3) ? LANG_FRENCH : (!strncmp(keyboardLang, "de_",3) ? LANG_GERMAN : LANG_ENGLISH);

    short subUserLCID, subSystemLCID;
    subUserLCID = subSystemLCID = !strncmp(systemLang, "en_AU",5) ? SUBLANG_ENGLISH_AUS : SUBLANG_OTHER;
    short subLayout = !strncmp(keyboardLang, "en_AU",5) ? SUBLANG_ENGLISH_AUS : SUBLANG_OTHER;

    if (   primUserLCID	  == LANG_GERMAN
           || primSystemLCID == LANG_GERMAN
           || primLayout	  == LANG_GERMAN )
    {
        CGame::nastyGame = false;
        FrontEndMenuManager.m_PrefsAllowNastyGame = false;
        CGame::germanGame = true;
    }

    if (   primUserLCID	  == LANG_FRENCH
           || primSystemLCID == LANG_FRENCH
           || primLayout	  == LANG_FRENCH )
    {
        CGame::nastyGame = false;
        FrontEndMenuManager.m_PrefsAllowNastyGame = false;
        CGame::frenchGame = true;
    }

    if (   subUserLCID	 == SUBLANG_ENGLISH_AUS
           || subSystemLCID == SUBLANG_ENGLISH_AUS
           || subLayout	 == SUBLANG_ENGLISH_AUS )
        CGame::noProstitutes = true;

#ifdef NASTY_GAME
    CGame::nastyGame = true;
    FrontEndMenuManager.m_PrefsAllowNastyGame = true;
    CGame::noProstitutes = false;
#endif

    int32 lang;

    switch ( primSystemLCID )
    {
        case LANG_GERMAN:
        {
            lang = LANG_GERMAN;
            break;
        }
        case LANG_FRENCH:
        {
            lang = LANG_FRENCH;
            break;
        }
        case LANG_SPANISH:
        {
            lang = LANG_SPANISH;
            break;
        }
        case LANG_ITALIAN:
        {
            lang = LANG_ITALIAN;
            break;
        }
        default:
        {
            lang = ( subSystemLCID == SUBLANG_ENGLISH_AUS ) ? -99 : LANG_ENGLISH;
            break;
        }
    }

    FrontEndMenuManager.OS_Language = primUserLCID;

    switch ( lang )
    {
        case LANG_GERMAN:
        {
            FrontEndMenuManager.m_PrefsLanguage = CMenuManager::LANGUAGE_GERMAN;
            break;
        }
        case LANG_SPANISH:
        {
            FrontEndMenuManager.m_PrefsLanguage = CMenuManager::LANGUAGE_SPANISH;
            break;
        }
        case LANG_FRENCH:
        {
            FrontEndMenuManager.m_PrefsLanguage = CMenuManager::LANGUAGE_FRENCH;
            break;
        }
        case LANG_ITALIAN:
        {
            FrontEndMenuManager.m_PrefsLanguage = CMenuManager::LANGUAGE_ITALIAN;
            break;
        }
        default:
        {
            FrontEndMenuManager.m_PrefsLanguage = CMenuManager::LANGUAGE_AMERICAN;
            break;
        }
    }

    // TODO this is needed for strcasecmp to work correctly across all languages, but can these cause other problems??
    setlocale(LC_CTYPE, "C");
    setlocale(LC_COLLATE, "C");
    setlocale(LC_NUMERIC, "C");

    TheText.Unload();
    TheText.Load();
}

/*
 *****************************************************************************
 */

void HandleExit()
{
    // We now handle terminate message always, why handle on some cases?
    return;
}

void terminateHandler(int sig, siginfo_t *info, void *ucontext) {
#if defined(ANDROID)
    if(g_pJavaWrapper)
        g_pJavaWrapper->ExitGame();
#endif
    RsGlobal.quit = TRUE;
}

#ifdef FLUSHABLE_STREAMING
void dummyHandler(int sig){
    // Don't kill the app pls
}
#endif

void resizeCB(int width, int height) {
    /*
    * Handle event to ensure window contents are displayed during re-size
    * as this can be disabled by the user, then if there is not enough
    * memory things don't work.
    */
    /* redraw window */

    if (RwInitialised && gGameState == GS_PLAYING_GAME)
    {
        RsEventHandler(rsIDLE, (void *)TRUE);
    }

    if (RwInitialised && height > 0 && width > 0) {
        RwRect r;

        // TODO fix artifacts of resizing with mouse
        RsGlobal.maximumHeight = height;
        RsGlobal.maximumWidth = width;

        r.x = 0;
        r.y = 0;
        r.w = width;
        r.h = height;

        RsEventHandler(rsCAMERASIZE, &r);
    }
}

void scrollCB(double xoffset, double yoffset) {
    PSGLOBAL(mouseWheel) = yoffset;
}

bool lshiftStatus = false;
bool rshiftStatus = false;

static const std::unordered_map<int, int> keymap = {
        {SDLK_SPACE,		' '},
        {SDLK_QUOTE, 		'\''},
        {SDLK_COMMA, 		','},
        {SDLK_MINUS, 		'-'},
        {SDLK_PERIOD,		'.'},
        {SDLK_SLASH,		'/'},
        {SDLK_0,			'0'},
        {SDLK_1,			'1'},
        {SDLK_2,			'2'},
        {SDLK_3,			'3'},
        {SDLK_4,			'4'},
        {SDLK_5,			'5'},
        {SDLK_6,			'6'},
        {SDLK_7,			'7'},
        {SDLK_8,			'8'},
        {SDLK_9,			'9'},
        {SDLK_SEMICOLON,	';'},
        {SDLK_EQUALS,		'='},
        {SDLK_LEFTBRACKET,	'['},
        {SDLK_BACKSLASH,	'\\'},
        {SDLK_RIGHTBRACKET,	']'},
        {SDLK_BACKQUOTE,	'`'},
        {SDLK_ESCAPE,		rsESC},
        {SDLK_RETURN,		rsENTER},
        {SDLK_TAB,			rsTAB},
        {SDLK_BACKSPACE,	rsBACKSP},
        {SDLK_INSERT,		rsINS},
        {SDLK_DELETE,		rsDEL},
        {SDLK_RIGHT,		rsRIGHT},
        {SDLK_LEFT,			rsLEFT},
        {SDLK_DOWN,			rsDOWN},
        {SDLK_UP,			rsUP},
        {SDLK_PAGEUP,		rsPGUP},
        {SDLK_PAGEDOWN,		rsPGDN},
        {SDLK_HOME,			rsHOME},
        {SDLK_END,			rsEND},
        {SDLK_CAPSLOCK,		rsCAPSLK},
        {SDLK_SCROLLLOCK,	rsSCROLL},
        //{SDLK_PRINTSCREEN,	rsNULL},
        {SDLK_PAUSE,		rsPAUSE},
        {SDLK_F1,			rsF1},
        {SDLK_F2,			rsF2},
        {SDLK_F3,			rsF3},
        {SDLK_F4,			rsF4},
        {SDLK_F5,			rsF5},
        {SDLK_F6,			rsF6},
        {SDLK_F7,			rsF7},
        {SDLK_F8,			rsF8},
        {SDLK_F9,			rsF9},
        {SDLK_F10,			rsF10},
        {SDLK_F11,			rsF11},
        {SDLK_F12,			rsF12},
        //{SDLK_F13,			rsNULL},
        //{SDLK_F14,			rsNULL},
        //{SDLK_F15,			rsNULL},
        //{SDLK_F16,			rsNULL},
        //{SDLK_F17,			rsNULL},
        //{SDLK_F18,			rsNULL},
        //{SDLK_F19,			rsNULL},
        //{SDLK_F20,			rsNULL},
        //{SDLK_F21,			rsNULL},
        //{SDLK_F22,			rsNULL},
        //{SDLK_F23,			rsNULL},
        //{SDLK_F24,			rsNULL},
        //{SDLK_F25,			rsNULL},
        {SDLK_KP_0,			rsPADINS},
        {SDLK_KP_1,			rsPADEND},
        {SDLK_KP_2,			rsPADDOWN},
        {SDLK_KP_3,			rsPADPGDN},
        {SDLK_KP_4,			rsPADLEFT},
        {SDLK_KP_5,			rsPAD5},
        {SDLK_KP_6,			rsPADRIGHT},
        {SDLK_KP_7,			rsPADHOME},
        {SDLK_KP_8,			rsPADUP},
        {SDLK_KP_9,			rsPADPGUP},
        {SDLK_KP_DECIMAL,	rsPADDEL},
        {SDLK_KP_DIVIDE,	rsDIVIDE},
        {SDLK_KP_MULTIPLY,	rsTIMES},
        {SDLK_KP_MINUS,		rsMINUS},
        {SDLK_KP_PLUS,		rsPLUS},
        {SDLK_KP_ENTER,		rsPADENTER},
        //{SDLK_KP_EQUAL,		rsNULL},
        {SDLK_LSHIFT,		rsLSHIFT},
        {SDLK_LCTRL,		rsLCTRL},
        {SDLK_LALT,			rsLALT},
        {SDLK_LGUI,			rsLWIN},
        {SDLK_RSHIFT,		rsRSHIFT},
        {SDLK_RCTRL,		rsRCTRL},
        {SDLK_RALT,			rsRALT},
        {SDLK_RGUI,			rsRWIN},
        //{SDLK_MENU,			rsNULL}
};

void
keypressCB(int key, int action, int mods)
{
    RsKeyCodes ks = rsNULL;

    if (key <= 0)
        return;

    // for a mysterious reason, isalpha() crashes for e.g. SDLK_VOLUMEUP/DOWN
    if (key >= 'A' && key <= 'Z') {
        ks = (RsKeyCodes) key;
    } else if (key >= 'a' && key <= 'z') {
        ks = (RsKeyCodes) toupper(key);
    } else {
        auto it = keymap.find(key);

        if (it != keymap.end()) {
            ks = (RsKeyCodes) it->second;
        }
    }

    if (key == SDLK_LSHIFT)
        lshiftStatus = (action != SDL_KEYUP);

    if (key == SDLK_RSHIFT)
        rshiftStatus = (action != SDL_KEYUP);

    if (ks == rsNULL)
        return;

    switch (action) {
        case SDL_KEYDOWN:	RsKeyboardEventHandler(rsKEYDOWN, &ks); break;
        case SDL_KEYUP:		RsKeyboardEventHandler(rsKEYUP, &ks); break;
    }
}

// R* calls that in ControllerConfig, idk why
void
_InputTranslateShiftKeyUpDown(RsKeyCodes *rs) {
    RsKeyboardEventHandler(lshiftStatus ? rsKEYDOWN : rsKEYUP, &(*rs = rsLSHIFT));
    RsKeyboardEventHandler(rshiftStatus ? rsKEYDOWN : rsKEYUP, &(*rs = rsRSHIFT));
}

void
cursorCB(double xpos, double ypos) {
    if (!FrontEndMenuManager.m_bMenuActive)
        return;

    // TODO remove?
    //int winw, winh;
    //SDL_GetWindowSize(PSGLOBAL(window), &winw, &winh);
    FrontEndMenuManager.m_nMouseTempPosX = xpos; // * (RsGlobal.maximumWidth / winw);
    FrontEndMenuManager.m_nMouseTempPosY = ypos; // * (RsGlobal.maximumHeight / winh);
}

void
cursorEnterCB(int entered) {
    PSGLOBAL(cursorIsInWindow) = !!entered;
}

void
windowFocusCB(int focused) {
    WindowFocused = !!focused;
}

void
windowIconifyCB(int iconified) {
    WindowIconified = !!iconified;
}

void inputEventHandler() {
    SDL_Event event;

    if (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:	/* fall-through */
            case SDL_KEYUP:
                keypressCB(event.key.keysym.sym, event.type, 0);
                break;

            case SDL_MOUSEMOTION: cursorCB(event.motion.x, event.motion.y); break;
            case SDL_MOUSEWHEEL: scrollCB(event.wheel.x, event.wheel.y); break;

            // note that SDL_CONTROLLERDEVICEADDED/REMOVED exists, but it did not work for me
            case SDL_JOYDEVICEADDED:	/* fall-through */
            case SDL_JOYDEVICEREMOVED:
                joysChangeCB(event.jdevice.which, event.type);
                break;

            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_ENTER: cursorEnterCB(true); break;
                    case SDL_WINDOWEVENT_LEAVE: cursorEnterCB(false); break;
                    case SDL_WINDOWEVENT_FOCUS_GAINED: windowFocusCB(true); break;
                    case SDL_WINDOWEVENT_FOCUS_LOST: windowFocusCB(false); break;
                    // TODO should it be minimized/maximized/restored instead of shown/hidden?
                    case SDL_WINDOWEVENT_SHOWN: windowIconifyCB(false); break;
                    case SDL_WINDOWEVENT_HIDDEN: windowIconifyCB(true); break;
                }
                break;

            default:
                break;
        }
    }
}

/*
 *****************************************************************************
 */
int
main(int argc, char *argv[])
{
    RwV2d pos;
    RwInt32 i;

#ifdef USE_CUSTOM_ALLOCATOR
    InitMemoryMgr();
#endif

#if defined(ANDROID)
    // Gamepad-only: disable touch-to-mouse emulation
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    // Force landscape orientation regardless of auto-rotate
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

    // Game assets are copied to app-private storage by Java SAF picker.
    if (!getenv("STORAGE_ROOT")) {
        const char *extPath = SDL_AndroidGetExternalStoragePath();
        if (extPath) setenv("STORAGE_ROOT", extPath, 1);
    }
    StorageRootBuffer = getenv("STORAGE_ROOT");
#endif

    struct sigaction act;
    act.sa_sigaction = terminateHandler;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGTERM, &act, NULL);
#ifdef FLUSHABLE_STREAMING
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = dummyHandler;
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
#endif

    for(i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            const char *gamePath = argv[i+1];
            setenv("STORAGE_ROOT", gamePath, 1);
        }
    }

    /*
     * Initialize the platform independent data.
     * This will in turn initialize the platform specific data...
     */
    if( RsEventHandler(rsINITIALIZE, nil) == rsEVENTERROR )
    {
        return FALSE;
    }

    for(i=1; i<argc; i++)
    {
        RsEventHandler(rsPREINITCOMMANDLINE, argv[i]);
    }

    /*
     * Parameters to be used in RwEngineOpen / rsRWINITIALISE event
     */

    openParams.width = RsGlobal.maximumWidth;
    openParams.height = RsGlobal.maximumHeight;
    openParams.windowtitle = RsGlobal.appName;
    openParams.window = &PSGLOBAL(window);

    ControlsManager.MakeControllerActionsBlank();
    ControlsManager.InitDefaultControlConfiguration();

    /*
     * Initialize the 3D (RenderWare) components of the app...
     */
    if( rsEVENTERROR == RsEventHandler(rsRWINITIALIZE, &openParams) )
    {
        RsEventHandler(rsTERMINATE, nil);
        return 0;
    }

    psPostRWinit();

    ControlsManager.InitDefaultControlConfigMouse(MousePointerStateHelper.GetMouseSetUp());

//	glfwSetWindowPos(PSGLOBAL(window), 0, 0);

    /*
     * Parse command line parameters (except program name) one at
     * a time AFTER RenderWare initialization...
     */
    for(i=1; i<argc; i++)
    {
        RsEventHandler(rsCOMMANDLINE, argv[i]);
    }

    /*
     * Force a camera resize event...
     */
    {
        RwRect r;

        r.x = 0;
        r.y = 0;
        r.w = RsGlobal.maximumWidth;
        r.h = RsGlobal.maximumHeight;

        RsEventHandler(rsCAMERASIZE, &r);
    }

    {
        CFileMgr::SetDirMyDocuments();

#ifdef LOAD_INI_SETTINGS
        // At this point InitDefaultControlConfigJoyPad must have set all bindings to default and ms_padButtonsInited to number of detected buttons.
        // We will load stored bindings below, but let's cache ms_padButtonsInited before LoadINIControllerSettings and LoadSettings clears it,
        // so we can add new joy bindings **on top of** stored bindings.
        int connectedPadButtons = ControlsManager.ms_padButtonsInited;
#endif

        int32 gta3set = CFileMgr::OpenFile("gta_vc.set", "r");

        if ( gta3set )
        {
            ControlsManager.LoadSettings(gta3set);
            CFileMgr::CloseFile(gta3set);
        }

        CFileMgr::SetDir("");

#ifdef LOAD_INI_SETTINGS
        LoadINIControllerSettings();
        if (connectedPadButtons != 0)
            ControlsManager.InitDefaultControlConfigJoyPad(connectedPadButtons); // add (connected-saved) amount of new button assignments on top of ours

        // these have 2 purposes: creating .ini at the start, and adding newly introduced settings to old .ini at the start
        SaveINISettings();
        SaveINIControllerSettings();
#endif
    }

#ifdef PS2_MENU
    int32 r = TheMemoryCard.CheckCardStateAtGameStartUp(CARD_ONE);
	if (   r == CMemoryCard::ERR_DIRNOENTRY  || r == CMemoryCard::ERR_NOFORMAT
		&& r != CMemoryCard::ERR_OPENNOENTRY && r != CMemoryCard::ERR_NONE )
	{
		LoadingScreen(nil, nil, "loadsc0");

		TheText.Unload();
		TheText.Load();

		CFont::Initialise();

		FrontEndMenuManager.DrawMemoryCardStartUpMenus();
	}
#endif

    while ( TRUE )
    {
        RwInitialised = TRUE;

        /*
        * Set the initial mouse position...
        */
        pos.x = RsGlobal.maximumWidth * 0.5f;
        pos.y = RsGlobal.maximumHeight * 0.5f;

        RsMouseSetPos(&pos);

        /*
        * Enter the message processing loop...
        */

#ifndef MASTER
        if (gbModelViewer) {
            // This is TheModelViewer in LCS
            LoadingScreen("Loading the ModelViewer", NULL, GetRandomSplashScreen());
            CAnimViewer::Initialise();
            CTimer::Update();
#ifndef PS2_MENU
            FrontEndMenuManager.m_bGameNotLoaded = false;
#endif
        }
#endif

#ifdef PS2_MENU
        if (TheMemoryCard.m_bWantToLoad)
			LoadSplash(GetLevelSplashScreen(CGame::currLevel));

		TheMemoryCard.m_bWantToLoad = false;

		CTimer::Update();

		while( !RsGlobal.quit && !(FrontEndMenuManager.m_bWantToRestart || TheMemoryCard.b_FoundRecentSavedGameWantToLoad) && !SDL_QuitRequested())
#else
        while( !RsGlobal.quit && !FrontEndMenuManager.m_bWantToRestart && !SDL_QuitRequested())
#endif
        {
            inputEventHandler();

#ifndef MASTER
            if (gbModelViewer) {
                // This is TheModelViewerCore in LCS
                TheModelViewer();
            } else
#endif
            if ( ForegroundApp )
            {
                switch ( gGameState )
                {
                    case GS_START_UP:
                    {
#ifdef NO_MOVIES
                        gGameState = GS_INIT_ONCE;
#else
                        gGameState = GS_INIT_LOGO_MPEG;
#endif
                        TRACE("gGameState = GS_INIT_ONCE");
                        break;
                    }

                    case GS_INIT_LOGO_MPEG:
                    {
                        //if (!startupDeactivate)
                        //    PlayMovieInWindow(cmdShow, "movies\\Logo.mpg");
                        gGameState = GS_LOGO_MPEG;
                        TRACE("gGameState = GS_LOGO_MPEG;");
                        break;
                    }

                    case GS_LOGO_MPEG:
                    {
//					    CPad::UpdatePads();

//					    if (startupDeactivate || ControlsManager.GetJoyButtonJustDown() != 0)
                        ++gGameState;
//					    else if (CPad::GetPad(0)->GetLeftMouseJustDown())
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetEnterJustDown())
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetCharJustDown(' '))
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetAltJustDown())
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetTabJustDown())
//						    ++gGameState;

                        break;
                    }

                    case GS_INIT_INTRO_MPEG:
                    {
//#ifndef NO_MOVIES
//					    CloseClip();
//					    CoUninitialize();
//#endif
//
//					    if (CMenuManager::OS_Language == LANG_FRENCH || CMenuManager::OS_Language == LANG_GERMAN)
//						    PlayMovieInWindow(cmdShow, "movies\\GTAtitlesGER.mpg");
//					    else
//						    PlayMovieInWindow(cmdShow, "movies\\GTAtitles.mpg");

                        gGameState = GS_INTRO_MPEG;
                        TRACE("gGameState = GS_INTRO_MPEG;");
                        break;
                    }

                    case GS_INTRO_MPEG:
                    {
//					    CPad::UpdatePads();
//
//					    if (startupDeactivate || ControlsManager.GetJoyButtonJustDown() != 0)
                        ++gGameState;
//					    else if (CPad::GetPad(0)->GetLeftMouseJustDown())
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetEnterJustDown())
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetCharJustDown(' '))
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetAltJustDown())
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetTabJustDown())
//						    ++gGameState;

                        break;
                    }

                    case GS_INIT_ONCE:
                    {
                        //CoUninitialize();

#ifdef PS2_MENU
                        extern char version_name[64];
						if ( CGame::frenchGame || CGame::germanGame )
							LoadingScreen(NULL, version_name, "loadsc24");
						else
							LoadingScreen(NULL, version_name, "loadsc0");

						printf("Into TheGame!!!\n");
#else
                        LoadingScreen(nil, nil, "loadsc0");
#endif
                        if ( !CGame::InitialiseOnceAfterRW() )
                            RsGlobal.quit = TRUE;

#ifdef PS2_MENU
                        gGameState = GS_INIT_PLAYING_GAME;
#else
                        gGameState = GS_INIT_FRONTEND;
                        TRACE("gGameState = GS_INIT_FRONTEND;");
#endif
                        break;
                    }

#ifndef PS2_MENU
                    case GS_INIT_FRONTEND:
                    {
                        LoadingScreen(nil, nil, "loadsc0");

                        FrontEndMenuManager.m_bGameNotLoaded = true;

                        FrontEndMenuManager.m_bStartUpFrontEndRequested = true;

                        if ( defaultFullscreenRes )
                        {
                            defaultFullscreenRes = FALSE;
                            FrontEndMenuManager.m_nPrefsVideoMode = GcurSelVM;
                            FrontEndMenuManager.m_nDisplayVideoMode = GcurSelVM;
                        }

                        gGameState = GS_FRONTEND;
                        TRACE("gGameState = GS_FRONTEND;");
                        break;
                    }

                    case GS_FRONTEND:
                    {
                        if(!WindowIconified)
                            RsEventHandler(rsFRONTENDIDLE, nil);

#ifdef PS2_MENU
                        if ( !FrontEndMenuManager.m_bMenuActive || TheMemoryCard.m_bWantToLoad )
#else
                        if ( !FrontEndMenuManager.m_bMenuActive || FrontEndMenuManager.m_bWantToLoad )
#endif
                        {
                            gGameState = GS_INIT_PLAYING_GAME;
                            TRACE("gGameState = GS_INIT_PLAYING_GAME;");
                        }

#ifdef PS2_MENU
                        if (TheMemoryCard.m_bWantToLoad )
#else
                        if ( FrontEndMenuManager.m_bWantToLoad )
#endif
                        {
                            InitialiseGame();
                            FrontEndMenuManager.m_bGameNotLoaded = false;
                            gGameState = GS_PLAYING_GAME;
                            TRACE("gGameState = GS_PLAYING_GAME;");
                        }
                        break;
                    }
#endif

                    case GS_INIT_PLAYING_GAME:
                    {
#ifdef PS2_MENU
                        CGame::Initialise("DATA\\GTA3.DAT");

						//LoadingScreen("Starting Game", NULL, GetRandomSplashScreen());

						if (   TheMemoryCard.CheckCardInserted(CARD_ONE) == CMemoryCard::NO_ERR_SUCCESS
							&& TheMemoryCard.ChangeDirectory(CARD_ONE, TheMemoryCard.Cards[CARD_ONE].dir)
							&& TheMemoryCard.FindMostRecentFileName(CARD_ONE, TheMemoryCard.MostRecentFile) == true
							&& TheMemoryCard.CheckDataNotCorrupt(TheMemoryCard.MostRecentFile))
						{
							strcpy(TheMemoryCard.LoadFileName, TheMemoryCard.MostRecentFile);
							TheMemoryCard.b_FoundRecentSavedGameWantToLoad = true;

							if (CMenuManager::m_PrefsLanguage != TheMemoryCard.GetLanguageToLoad())
							{
								CMenuManager::m_PrefsLanguage = TheMemoryCard.GetLanguageToLoad();
								TheText.Unload();
								TheText.Load();
							}

							CGame::currLevel = (eLevelName)TheMemoryCard.GetLevelToLoad();
						}
#else
                        InitialiseGame();

                        FrontEndMenuManager.m_bGameNotLoaded = false;
#endif
                        gGameState = GS_PLAYING_GAME;
                        TRACE("gGameState = GS_PLAYING_GAME;");
                        break;
                    }

                    case GS_PLAYING_GAME:
                    {
                        float ms = (float)CTimer::GetCurrentTimeInCycles() / (float)CTimer::GetCyclesPerMillisecond();
                        if ( RwInitialised )
                        {
                            if (!FrontEndMenuManager.m_PrefsFrameLimiter || (1000.0f / (float)RsGlobal.maxFPS) < ms)
                                RsEventHandler(rsIDLE, (void *)TRUE);
                        }
                        break;
                    }
                }
            }
            else
            {
                if ( RwCameraBeginUpdate(Scene.camera) )
                {
                    RwCameraEndUpdate(Scene.camera);
                    ForegroundApp = TRUE;
                    RsEventHandler(rsACTIVATE, (void *)TRUE);
                }

            }
        }


        /*
        * About to shut down - block resize events again...
        */
        RwInitialised = FALSE;

        FrontEndMenuManager.UnloadTextures();
#ifdef PS2_MENU
        if ( !(FrontEndMenuManager.m_bWantToRestart || TheMemoryCard.b_FoundRecentSavedGameWantToLoad))
			break;
#else
        if ( !FrontEndMenuManager.m_bWantToRestart )
            break;
#endif

        CPad::ResetCheats();
        CPad::StopPadsShaking();

        DMAudio.ChangeMusicMode(MUSICMODE_DISABLE);

#ifdef PS2_MENU
        CGame::ShutDownForRestart();
#endif

        CTimer::Stop();

#ifdef PS2_MENU
        if (FrontEndMenuManager.m_bWantToRestart || TheMemoryCard.b_FoundRecentSavedGameWantToLoad)
		{
			if (TheMemoryCard.b_FoundRecentSavedGameWantToLoad)
			{
				FrontEndMenuManager.m_bWantToRestart = true;
				TheMemoryCard.m_bWantToLoad = true;
			}

			CGame::InitialiseWhenRestarting();
			DMAudio.ChangeMusicMode(MUSICMODE_GAME);
			FrontEndMenuManager.m_bWantToRestart = false;

			continue;
		}

		CGame::ShutDown();
		CTimer::Stop();

		break;
#else
        if ( FrontEndMenuManager.m_bWantToLoad )
        {
            CGame::ShutDownForRestart();
            CGame::InitialiseWhenRestarting();
            DMAudio.ChangeMusicMode(MUSICMODE_GAME);
            LoadSplash(GetLevelSplashScreen(CGame::currLevel));
            FrontEndMenuManager.m_bWantToLoad = false;
        }
        else
        {
#ifndef MASTER
            if ( gbModelViewer )
                CAnimViewer::Shutdown();
            else
#endif
            if ( gGameState == GS_PLAYING_GAME )
                CGame::ShutDown();

            CTimer::Stop();

            if ( FrontEndMenuManager.m_bFirstTime == true )
            {
                gGameState = GS_INIT_FRONTEND;
                TRACE("gGameState = GS_INIT_FRONTEND;");
            }
            else
            {
                gGameState = GS_INIT_PLAYING_GAME;
                TRACE("gGameState = GS_INIT_PLAYING_GAME;");
            }
        }

        FrontEndMenuManager.m_bFirstTime = false;
        FrontEndMenuManager.m_bWantToRestart = false;
#endif
    }


#ifndef MASTER
    if ( gbModelViewer )
        CAnimViewer::Shutdown();
    else
#endif
    if ( gGameState == GS_PLAYING_GAME )
        CGame::ShutDown();

    DMAudio.Terminate();

    _psFreeVideoModeList();


    /*
     * Tidy up the 3D (RenderWare) components of the application...
     */
    RsEventHandler(rsRWTERMINATE, nil);

    /*
     * Free the platform dependent data...
     */
    RsEventHandler(rsTERMINATE, nil);

    return 0;
}

/*
 *****************************************************************************
 */

RwV2d leftStickPos;
RwV2d rightStickPos;

void CapturePad(RwInt32 padID)
{
    static SDL_GameController* gamepad = nullptr;

    if (padID == 0)
        gamepad = gamepad1;
    else if(padID == 1)
        gamepad = gamepad2;
    else
        assert("invalid padID");

    if (gamepad == nullptr)
        return;

    SDL_Joystick* joy = SDL_GameControllerGetJoystick(gamepad);
    int joyId = SDL_JoystickInstanceID(joy);
    int numAxes = SDL_JoystickNumAxes(joy);

    if (ControlsManager.m_bFirstCapture == false) {
        memcpy(&ControlsManager.m_OldState, &ControlsManager.m_NewState, sizeof(ControlsManager.m_NewState));
    } else {
        // In case connected gamepad doesn't have L-R trigger axes.
        ControlsManager.m_NewState.mappedButtons[15] = 0;	// left trigger
        ControlsManager.m_NewState.mappedButtons[16] = 0;	// right trigger
    }

    // Update buttons state
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) {
        int state = SDL_GameControllerGetButton(gamepad, (SDL_GameControllerButton) i);
        assert(state == 0 || state == 1);
        ControlsManager.m_NewState.buttons[i] = state;
        ControlsManager.m_NewState.mappedButtons[i] = !!state;
    }

    ControlsManager.m_NewState.numButtons = SDL_CONTROLLER_BUTTON_MAX - 1;
    ControlsManager.m_NewState.id = joyId;
    ControlsManager.m_NewState.isGamepad = true; // SDL_IsGameController(joyId);

    if (ControlsManager.m_NewState.isGamepad) {
        // TRIGGERLEFT/RIGHT are in range 0..32767, which needs to be converted to -1 (released) .. 1 (pressed)
        float lt = (SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) - 16384.0) / 16384.0;
        float rt = (SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) - 16384.0) / 16384.0;

        // glfw returns 0.0 for non-existent axises(which is bullocks) so we treat it as deadzone, and keep value of previous frame.
        // otherwise if this axis is present, -1 = released, 1 = pressed
        if (lt != 0.0f)
            ControlsManager.m_NewState.mappedButtons[15] = lt > -0.8f;

        if (rt != 0.0f)
            ControlsManager.m_NewState.mappedButtons[16] = rt > -0.8f;
    }
    // TODO? L2-R2 axes(not buttons-that's fine) on joysticks that don't have SDL gamepad mapping AREN'T handled, and I think it's impossible to do without mapping.

    if (ControlsManager.m_bFirstCapture == true) {
        memcpy(&ControlsManager.m_OldState, &ControlsManager.m_NewState, sizeof(ControlsManager.m_NewState));
        ControlsManager.m_bFirstCapture = false;
    }

    RsPadButtonStatus bs;
    bs.padID = padID;
    RsPadEventHandler(rsPADBUTTONUP, (void *)&bs);

    // Gamepad axes are guaranteed to return 0.0f if that particular gamepad doesn't have that axis.
    // And that's really good for sticks, because gamepads return 0.0 for them when sticks are in released state.
    // Stick position is converted from range [-32768; 32767] to [-1; +1]
    leftStickPos.x = (ControlsManager.m_NewState.isGamepad && numAxes >= 1) ? SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_LEFTX) / 32768.0f : 0.0f;
    leftStickPos.y = (ControlsManager.m_NewState.isGamepad && numAxes >= 2) ? SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_LEFTY) / 32768.0f : 0.0f;

    rightStickPos.x = (ControlsManager.m_NewState.isGamepad && numAxes >= 3) ? SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_RIGHTX) / 32768.0f : 0.0f;
    rightStickPos.y = (ControlsManager.m_NewState.isGamepad && numAxes >= 4) ? SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_RIGHTY) / 32768.0f : 0.0f;

    {
        if (CPad::m_bMapPadOneToPadTwo)
            bs.padID = 1;

        RsPadEventHandler(rsPADBUTTONUP,   (void *)&bs);
        RsPadEventHandler(rsPADBUTTONDOWN, (void *)&bs);
    }

    {
        if (CPad::m_bMapPadOneToPadTwo)
            bs.padID = 1;

        CPad *pad = CPad::GetPad(bs.padID);

        if (Abs(leftStickPos.x)  > ControlsManager.m_lStickDeadzone)
            pad->PCTempJoyState.LeftStickX	= (int32)(leftStickPos.x  * 128.0f * ControlsManager.m_lStickSensX);

        if (Abs(leftStickPos.y)  > ControlsManager.m_lStickDeadzone)
            pad->PCTempJoyState.LeftStickY	= (int32)(leftStickPos.y  * 128.0f * ControlsManager.m_lStickSensY);

        if (Abs(rightStickPos.x) > ControlsManager.m_rStickDeadzone)
            pad->PCTempJoyState.RightStickX = (int32)(rightStickPos.x * 128.0f * ControlsManager.m_rStickSensX);

        if (Abs(rightStickPos.y) > ControlsManager.m_rStickDeadzone)
            pad->PCTempJoyState.RightStickY = (int32)(rightStickPos.y * 128.0f * ControlsManager.m_rStickSensY);
    }

    _psHandleVibration();
}

void joysChangeCB(int jid, int event)
{
    if (event == SDL_JOYDEVICEADDED && !IsThisJoystickBlacklisted(jid)) {
        if (PSGLOBAL(joy1id) == -1) {
            assert(gamepad1 == nullptr);
            gamepad1 = SDL_GameControllerOpen(jid);
            assert(gamepad1 != nullptr);
#ifdef DETECT_JOYSTICK_MENU
            strncpy(gSelectedJoystickName, SDL_JoystickNameForIndex(jid), sizeof(gSelectedJoystickName));
#endif
            // This is behind LOAD_INI_SETTINGS, because otherwise the Init call below will destroy/overwrite your bindings.
#ifdef LOAD_INI_SETTINGS
            SDL_Joystick* joy = SDL_GameControllerGetJoystick(gamepad1);
            int count = SDL_JoystickNumButtons(joy);
            ControlsManager.InitDefaultControlConfigJoyPad(count);
            PSGLOBAL(joy1id) = SDL_JoystickInstanceID(joy);	// needed to get right ID for remove event
            // Do not close the controller handle, it is done in the device remove handler (below)
#endif
        } else if (PSGLOBAL(joy2id) == -1) {
            assert(gamepad2 == nullptr);
            gamepad2 = SDL_GameControllerOpen(jid);
            assert(gamepad2 != nullptr);
            SDL_Joystick* joy = SDL_GameControllerGetJoystick(gamepad2);
            PSGLOBAL(joy2id) = SDL_JoystickInstanceID(joy);	// needed to get right ID for remove event
        }

    } else if (event == SDL_JOYDEVICEREMOVED) {
        if (PSGLOBAL(joy1id) == jid) {
            assert(gamepad1 != nullptr);
            SDL_GameControllerClose(gamepad1);
            PSGLOBAL(joy1id) = -1;
            gamepad1 = nullptr;
        } else if (PSGLOBAL(joy2id) == jid) {
            assert(gamepad2 != nullptr);
            SDL_GameControllerClose(gamepad2);
            PSGLOBAL(joy2id) = -1;
            gamepad2 = nullptr;
        }
    }
}

#endif
