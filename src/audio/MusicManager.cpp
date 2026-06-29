#include "common.h"
#include <time.h>
#include "soundlist.h"
#include "MusicManager.h"
#include "AudioManager.h"
#include "ControllerConfig.h"
#include "Camera.h"
#include "Font.h"
#include "Hud.h"
#include "ModelIndices.h"
#include "Replay.h"
#include "Pad.h"
#include "Text.h"
#include "Timer.h"
#include "World.h"
#include "sampman.h"
#include "Stats.h"
#include "Script.h"
#include "ZoneCull.h"
#include "Weather.h"
#include "DMAudio.h"
#include "GenericGameStorage.h"

#ifdef GTA_PS2
#include <libcdvd.h>
extern "C" void WaitVBlank(void);
#endif

#if !defined FIX_BUGS && (defined RADIO_SCROLL_TO_PREV_STATION || defined RADIO_OFF_TEXT)
static_assert(false, "R*'s radio implementation is quite buggy, RADIO_SCROLL_TO_PREV_STATION and RADIO_OFF_TEXT won't work without FIX_BUGS");
#endif

enum
{
	MAX_RADIO_VOLUME = 100,
	MAX_RADIO_DIST = 55,
	MIN_RADIO_DIST = 10,

	MAX_WATER_DIST = 140,
	WATER_DIST_STEP = 50,
	MAX_AMBIENCE_VOLUME = 60,


	MAX_RIOT_DIST = 100,
	MID_RIOT_DIST = 65,
	MIN_RIOT_DIST = 20,
};

cMusicManager MusicManager;
int32 gNumRetunePresses;
int32 gRetuneCounter;
bool8 g_bAnnouncementReadPosAlready;
uint8 RadioStaticCounter = 5;
uint32 RadioStaticTimer;

CVector vecRiotPosition(300.7f, -322.0f, 12.0f);

uint32 NewGameRadioTimers[NUM_RADIOS] =
{
	948160,
	452150,
	2438150,
	3538230,
	3513100,
	4246050,
	1418050,
	3178240,
	471210,
	0
};

#ifdef GTA_PS2
#define SURROUND_PAN(LeftRight, FrontRear) LeftRight, FrontRear
#else
#define SURROUND_PAN(LeftRight, FrontRear) LeftRight
#endif

cMusicManager::cMusicManager()
{
	m_bIsInitialised = FALSE;
	m_bDisabled = FALSE;
	m_nFrontendTrack = NO_TRACK;
	m_nPlayingTrack = NO_TRACK;
	m_nUpcomingMusicMode = MUSICMODE_DISABLED;
	m_nMusicMode = MUSICMODE_DISABLED;
	m_bSetNextStation = FALSE;

	for (int i = 0; i < NUM_RADIOS; i++)
		aListenTimeArray[i] = 0.0f;

	m_nLastTrackServiceTime = 0.0f;
	m_nVolumeLatency = 0;
	m_nCurrentVolume = 0;
	m_nMaxVolume = 0;
	m_nAnnouncement = NO_TRACK;
	m_bAnnouncementInProgress = FALSE;
}

cMusicManager::~cMusicManager() {}

void
cMusicManager::ResetMusicAfterReload()
{
	float afRadioTime[NUM_RADIOS];

	m_bRadioSetByScript = FALSE;
	m_nRadioStationScript = 0;
	m_nRadioPosition = -1;
	m_nAnnouncement = NO_TRACK;
	m_bAnnouncementInProgress = FALSE;
	m_bSetNextStation = FALSE;
	RadioStaticTimer = 0;
	gNumRetunePresses = 0;
	gRetuneCounter = 0;
	m_nFrontendTrack = NO_TRACK;
	m_nPlayingTrack = NO_TRACK;
	m_FrontendLoopFlag = FALSE;
	m_bTrackChangeStarted = FALSE;
	m_nNextTrack = NO_TRACK;
	m_nNextLoopFlag = FALSE;
	m_bVerifyNextTrackStartedToPlay = FALSE;
	m_bGameplayAllowsRadio = FALSE;
	m_bRadioStreamReady = FALSE;
	nFramesSinceCutsceneEnded = -1;
	m_bUserResumedGame = FALSE;
	m_bMusicModeChangeStarted = FALSE;
	m_bEarlyFrontendTrack = FALSE;
	m_nVolumeLatency = 0;
	m_nCurrentVolume = 0;
	m_nMaxVolume = 0;

	bool8 bRadioWasEverListened = FALSE;

	for (int i = 0; i < NUM_RADIOS; i++) {
		afRadioTime[i] = CStats::GetFavoriteRadioStationList(i);
		if (!bRadioWasEverListened && afRadioTime[i] != 0.0f)
			bRadioWasEverListened = TRUE;
	}

	if (!bRadioWasEverListened) return;

	for (int i = 0; i < NUM_RADIOS; i++) {
		aListenTimeArray[i] = afRadioTime[i];
		int32 trackPos = GetSavedRadioStationPosition(i);
		if (trackPos != -1) {
			if (trackPos > m_aTracks[i].m_nLength) {
				debug("Radio Track %d saved position is %d, Length is only %d\n", i, trackPos, m_aTracks[i].m_nLength);
				trackPos %= m_aTracks[i].m_nLength;
			}
			m_aTracks[i].m_nPosition = trackPos;
			m_aTracks[i].m_nLastPosCheckTimer = CTimer::GetTimeInMillisecondsPauseMode();
		}
	}
}

void
cMusicManager::SetStartingTrackPositions(bool8 isNewGameTimer)
{
	int pos;

	if (m_bIsInitialised) {
#ifdef GTA_PS2
		sceCdCLOCK rtc;
		if(sceCdReadClock(&rtc) == 1){
			if(rtc.second == 0)
				rtc.second = AudioManager.m_anRandomTable[0];
			if(rtc.minute == 0)
				rtc.minute = AudioManager.m_anRandomTable[1];
			if(rtc.hour == 0)
				rtc.hour = AudioManager.m_anRandomTable[2];
			if(rtc.day == 0)
				rtc.day = AudioManager.m_anRandomTable[3];
			if(rtc.month == 0)
				rtc.month = AudioManager.m_anRandomTable[4];
			if(rtc.year == 0)
				rtc.year = AudioManager.m_anRandomTable[0];
			pos = rtc.year * rtc.month * rtc.day * rtc.hour *
				rtc.minute * rtc.minute *
				rtc.second * rtc.second;
		}else
			pos = AudioManager.m_anRandomTable[0];
#else
		time_t timevalue = time(0);
		if (timevalue == -1) {
			pos = AudioManager.m_anRandomTable[0];
		} else {
			tm* pTm = localtime(&timevalue);
			if (pTm->tm_sec == 0)
				pTm->tm_sec = AudioManager.m_anRandomTable[0];
			if (pTm->tm_min == 0)
				pTm->tm_min = AudioManager.m_anRandomTable[1];
			if (pTm->tm_hour == 0)
				pTm->tm_hour = AudioManager.m_anRandomTable[2];
			if (pTm->tm_mday == 0)
				pTm->tm_mday = AudioManager.m_anRandomTable[3];
			if (pTm->tm_mon == 0)
				pTm->tm_mon = AudioManager.m_anRandomTable[4];
			if (pTm->tm_year == 0)
				pTm->tm_year = AudioManager.m_anRandomTable[3];
			if (pTm->tm_wday == 0)
				pTm->tm_wday = AudioManager.m_anRandomTable[2];
			pos = pTm->tm_yday
				* pTm->tm_wday
				* pTm->tm_year
				* pTm->tm_mon
				* pTm->tm_mday
				* pTm->tm_hour * pTm->tm_hour
				* pTm->tm_min * pTm->tm_min
				* pTm->tm_sec * pTm->tm_sec * pTm->tm_sec * pTm->tm_sec;
		}
#endif

		for (tTrack i = 0; i < TOTAL_STREAMED_SOUNDS; i++) {
			m_aTracks[i].m_nLength = SampleManager.GetStreamedFileLength(i);

			if (i < STREAMED_SOUND_CITY_AMBIENT && isNewGameTimer)
				m_aTracks[i].m_nPosition = NewGameRadioTimers[i];
			else if (i < STREAMED_SOUND_ANNOUNCE_BRIDGE_CLOSED)
				m_aTracks[i].m_nPosition = (pos * AudioManager.m_anRandomTable[i % 5]) % m_aTracks[i].m_nLength;
			else
				m_aTracks[i].m_nPosition = 0;
			
			m_aTracks[i].m_nLastPosCheckTimer = CTimer::GetTimeInMillisecondsPauseMode();
		}
	}
}

bool8
cMusicManager::Initialise()
{
	if (!m_bIsInitialised) {
		m_bIsInitialised = TRUE;
		SetStartingTrackPositions(FALSE);
		m_bResetTimers = FALSE;
		m_nResetTime = 0;
		m_bRadioSetByScript = FALSE;
		m_nRadioStationScript = 0;
		m_nRadioPosition = -1;
		m_nRadioInCar = NO_TRACK;
		gRetuneCounter = 0;
		gNumRetunePresses = 0;
		m_nFrontendTrack = NO_TRACK;
		m_nPlayingTrack = NO_TRACK;
		m_nUpcomingMusicMode = MUSICMODE_DISABLED;
		m_nMusicMode = MUSICMODE_DISABLED;
		m_FrontendLoopFlag = FALSE;
		m_bTrackChangeStarted = FALSE;
		m_nNextTrack = NO_TRACK;
		m_nNextLoopFlag = FALSE;
		m_bVerifyNextTrackStartedToPlay = FALSE;
		m_bGameplayAllowsRadio = FALSE;
		m_bRadioStreamReady = FALSE;
		nFramesSinceCutsceneEnded = -1;
		m_bUserResumedGame = FALSE;
		m_bMusicModeChangeStarted = FALSE;
		m_nMusicModeToBeSet = MUSICMODE_DISABLED;
		m_bEarlyFrontendTrack = FALSE;
		m_nVolumeLatency = 0;
		m_nCurrentVolume = 0;
		m_nMaxVolume = 0;
	}
	return m_bIsInitialised;
}

void
cMusicManager::Terminate()
{
	if (m_bIsInitialised) {
		if (SampleManager.IsStreamPlaying()) {
			SampleManager.StopStreamedFile();
			m_nPlayingTrack = NO_TRACK;
		}
		m_bIsInitialised = FALSE;
	}
}

void
cMusicManager::SetRadioChannelByScript(tTrack station, int32 pos)
{
	if (m_bIsInitialised) {
#ifdef GTA_PC
		if (station == USERTRACK)
			station = RADIO_OFF;
#endif
		if (station <= STREAMED_SOUND_RADIO_POLICE) { // shouldn't this be NUM_RADIOS?
			m_bRadioSetByScript = TRUE;
			m_nRadioStationScript = station;
			m_nRadioPosition = pos != -1 ? pos % m_aTracks[station].m_nLength : -1;
		}
	}
}

tTrack
cMusicManager::GetRadioInCar(void)
{
#define RET_RADIO(radio) if (m_nRadioInCar != NO_TRACK) { \
			if (CReplay::IsPlayingBack() && !AudioManager.m_bIsPaused) \
				return radio; \
			return m_nRadioInCar; \
		} else { return radio; }

	if (m_bIsInitialised) {
		if (PlayerInCar()) {
			CVehicle* veh = AudioManager.FindVehicleOfPlayer();
			if (veh) {
				if (UsesPoliceRadio(veh) || UsesTaxiRadio(veh)) 
					RET_RADIO(STREAMED_SOUND_RADIO_POLICE)
				else
					return veh->m_nRadioStation;
			}
			else
				RET_RADIO(RADIO_OFF)
		}
		else
			RET_RADIO(RADIO_OFF)
	}
	return 0;

#undef RET_RADIO
}

void
cMusicManager::SetRadioInCar(tTrack station)
{
	if (m_bIsInitialised) {
		if (!PlayerInCar()) {
			m_nRadioInCar = station;
			return;
		}
		CVehicle* veh = AudioManager.FindVehicleOfPlayer();
		if (veh == nil) return;
		if (UsesPoliceRadio(veh) || UsesTaxiRadio(veh))
			m_nRadioInCar = station;
		else
			veh->m_nRadioStation = station;
	}
}

void
cMusicManager::ChangeMusicMode(uint8 mode)
{
	if (m_bIsInitialised) {
		switch (mode)
		{
		case MUSICMODE_FRONTEND:
			m_nUpcomingMusicMode = MUSICMODE_FRONTEND;

#ifdef PAUSE_RADIO_IN_FRONTEND
			// rewind those streams we weren't listening right now
			for( tTrack i = STREAMED_SOUND_RADIO_WILD; i < STREAMED_SOUND_CUTSCENE_ASS_1; i++ ) {
				m_aTracks[i].m_nPosition = GetTrackStartPos(i);
				m_aTracks[i].m_nLastPosCheckTimer = CTimer::GetTimeInMillisecondsPauseMode();
			}
#endif

			break;
		case MUSICMODE_GAME: m_nUpcomingMusicMode = MUSICMODE_GAME; break;
		case MUSICMODE_CUTSCENE:
			m_nUpcomingMusicMode = MUSICMODE_CUTSCENE;
			if (SampleManager.IsStreamPlaying()) {
				if (m_nPlayingTrack != NO_TRACK) {
					RecordRadioStats();
					m_aTracks[m_nPlayingTrack].m_nPosition = SampleManager.GetStreamedFilePosition();
					m_aTracks[m_nPlayingTrack].m_nLastPosCheckTimer = CTimer::GetTimeInMillisecondsPauseMode();
				}
			}
			SampleManager.StopStreamedFile();
			while (SampleManager.IsStreamPlaying()) {
				SampleManager.StopStreamedFile();
#ifdef GTA_PS2
				WaitVBlank();
#endif
			}
			m_nMusicMode = m_nUpcomingMusicMode;
			m_bMusicModeChangeStarted = FALSE;
			m_bTrackChangeStarted = FALSE;
			m_nNextTrack = NO_TRACK;
			m_nNextLoopFlag = FALSE;
			m_bVerifyNextTrackStartedToPlay = FALSE;
			m_nPlayingTrack = NO_TRACK;
			m_nFrontendTrack = NO_TRACK;
			m_bAnnouncementInProgress = FALSE;
			m_nAnnouncement = NO_TRACK;
			g_bAnnouncementReadPosAlready = FALSE;
			break;
		case MUSICMODE_DISABLE: m_nUpcomingMusicMode = MUSICMODE_DISABLED; break;
		default: return;
		}
	}
}

void
cMusicManager::ResetTimers(uint32 time)
{
	m_bResetTimers = TRUE;
	m_nResetTime = time;
}

void
cMusicManager::Service()
{
	if (m_bResetTimers) {
		m_bResetTimers = FALSE;
		m_nLastTrackServiceTime = m_nResetTime;
	}

	static bool8 bRadioStatsRecorded = FALSE;

	if (!m_bIsInitialised || m_bDisabled) return;

	if (!m_bMusicModeChangeStarted)
		m_nMusicModeToBeSet = m_nUpcomingMusicMode;
	if (m_nMusicModeToBeSet != m_nMusicMode) {
		m_bMusicModeChangeStarted = TRUE;
		if (!m_bUserResumedGame && !AudioManager.m_bIsPaused && AudioManager.m_bWasPaused)
			m_bUserResumedGame = TRUE;

		if (AudioManager.m_FrameCounter % 4 != 0) return;

		gNumRetunePresses = 0;
		gRetuneCounter = 0;
		m_bSetNextStation = FALSE;
		if (SampleManager.IsStreamPlaying()) {
			if (m_nPlayingTrack != NO_TRACK && !bRadioStatsRecorded) {
				RecordRadioStats();
				m_aTracks[m_nPlayingTrack].m_nPosition = SampleManager.GetStreamedFilePosition();
				m_aTracks[m_nPlayingTrack].m_nLastPosCheckTimer = CTimer::GetTimeInMillisecondsPauseMode();
				bRadioStatsRecorded = TRUE;
			}
			SampleManager.StopStreamedFile();
		} else {
			bRadioStatsRecorded = FALSE;
			m_nMusicMode = m_nMusicModeToBeSet;
			m_bMusicModeChangeStarted = FALSE;
			m_bTrackChangeStarted = FALSE;
			m_nNextTrack = NO_TRACK;
			m_nNextLoopFlag = FALSE;
			m_bVerifyNextTrackStartedToPlay = FALSE;
			m_nPlayingTrack = NO_TRACK;
			if (!m_bEarlyFrontendTrack)
				m_nFrontendTrack = NO_TRACK;
			else
				m_bEarlyFrontendTrack = FALSE;
		}
	} else {
		if (AudioManager.m_bIsPaused && !AudioManager.m_bWasPaused && m_nMusicMode == MUSICMODE_FRONTEND) 
			m_nMusicMode = MUSICMODE_DISABLED;
		switch (m_nMusicMode)
		{
		case MUSICMODE_FRONTEND: ServiceFrontEndMode(); break;
		case MUSICMODE_GAME: ServiceGameMode(); break;
		case MUSICMODE_CUTSCENE: SampleManager.SetStreamedVolumeAndPan(MAX_VOLUME, SURROUND_PAN(63, 30), TRUE); break;
		}
	}
}

void
cMusicManager::ServiceFrontEndMode()
{
	static bool8 bRadioStatsRecorded = FALSE;

#ifdef PAUSE_RADIO_IN_FRONTEND
	// pause radio
	for (tTrack i = STREAMED_SOUND_RADIO_WILD; i < STREAMED_SOUND_CUTSCENE_ASS_1; i++)
		m_aTracks[i].m_nLastPosCheckTimer = CTimer::GetTimeInMillisecondsPauseMode();
#endif

	if (m_bAnnouncementInProgress) {
		SampleManager.StopStreamedFile();
		if (SampleManager.IsStreamPlaying())
			return;
		g_bAnnouncementReadPosAlready = FALSE;
		m_nAnnouncement = NO_TRACK;
		m_bAnnouncementInProgress = FALSE;
		m_nNextTrack = NO_TRACK;
		m_nFrontendTrack = NO_TRACK;
		m_nPlayingTrack = NO_TRACK;
	}

	if (AudioManager.m_FrameCounter % 4 != 0) return;

	if (!m_bTrackChangeStarted && !m_bVerifyNextTrackStartedToPlay) {
		m_nNextTrack = m_nFrontendTrack;
		m_nNextLoopFlag = m_FrontendLoopFlag;
	}

	if (m_nNextTrack != m_nPlayingTrack) {
		m_bTrackChangeStarted = TRUE;
		if (!m_bVerifyNextTrackStartedToPlay && SampleManager.IsStreamPlaying()) {
			if (m_nPlayingTrack != NO_TRACK && !bRadioStatsRecorded) {
				m_aTracks[m_nPlayingTrack].m_nPosition = SampleManager.GetStreamedFilePosition();
				m_aTracks[m_nPlayingTrack].m_nLastPosCheckTimer = CTimer::GetTimeInMillisecondsPauseMode();
				RecordRadioStats();
				bRadioStatsRecorded = TRUE;
			}
			SampleManager.SetStreamedVolumeAndPan(0, SURROUND_PAN(63, 30), FALSE);
			SampleManager.StopStreamedFile();
		} else {
			bRadioStatsRecorded = FALSE;
			if (SampleManager.IsStreamPlaying() || m_nNextTrack == NO_TRACK) {
				m_nPlayingTrack = m_nNextTrack;
				m_bVerifyNextTrackStartedToPlay = FALSE;
				m_bTrackChangeStarted = FALSE;
			} else {
				uint32 trackStartPos = (m_nNextTrack <= STREAMED_SOUND_RADIO_POLICE) ? GetTrackStartPos(m_nNextTrack) : 0;
				if (m_nNextTrack != NO_TRACK) {
					SampleManager.SetStreamedFileLoopFlag(m_nNextLoopFlag);
					SampleManager.StartStreamedFile(m_nNextTrack, trackStartPos);
					m_nVolumeLatency = 3;
					m_nCurrentVolume = 0;
					m_nMaxVolume = MAX_RADIO_VOLUME;
					SampleManager.SetStreamedVolumeAndPan(m_nCurrentVolume, SURROUND_PAN(63, 30), FALSE);
					if (m_nNextTrack < STREAMED_SOUND_CITY_AMBIENT)
						m_nLastTrackServiceTime = CTimer::GetTimeInMillisecondsPauseMode();
					m_bVerifyNextTrackStartedToPlay = TRUE;
				}
			}
		}
	} else {
		if (!SampleManager.IsStreamPlaying()) {
			switch (m_nPlayingTrack)
			{
			case STREAMED_SOUND_MISSION_COMPLETED:
				if (!AudioManager.m_bIsPaused)
					ChangeMusicMode(MUSICMODE_GAME);
				break;
#ifdef GTA_PC
			case STREAMED_SOUND_RADIO_MP3_PLAYER:
				SampleManager.StartStreamedFile(STREAMED_SOUND_RADIO_MP3_PLAYER, 0);
				break;
#endif
			default: break;
			}
		} else {
			if (m_nVolumeLatency > 0)
				m_nVolumeLatency--;
			else {
				if (m_nCurrentVolume < m_nMaxVolume)
					m_nCurrentVolume = Min(m_nMaxVolume, m_nCurrentVolume + 6);
				SampleManager.SetStreamedVolumeAndPan(m_nCurrentVolume, SURROUND_PAN(63, 30), FALSE);
			}
		} 
	} 
}

#ifdef GTA_PS2
#define RETUNE_TIME 30
#else
#define RETUNE_TIME 20
#endif

void
cMusicManager::ServiceGameMode()
{
	CPed *ped = FindPlayerPed();
	CVehicle *vehicle = AudioManager.FindVehicleOfPlayer();
	m_bRadioStreamReady = m_bGameplayAllowsRadio;
	m_bGameplayAllowsRadio = FALSE;

	switch (CGame::currArea)
	{
	case AREA_HOTEL:
	case AREA_MALL:
	case AREA_STRIP_CLUB:
	case AREA_DIRT:
	case AREA_BLOOD:
	case AREA_OVALRING:
	case AREA_MALIBU_CLUB:
		m_bGameplayAllowsRadio = FALSE;
		break;
	default:
		if (SampleManager.GetMusicVolume() == 0)
			m_bGameplayAllowsRadio = FALSE;
		else if (PlayerInCar())
			m_bGameplayAllowsRadio = TRUE;			
		break;
	}

	if (m_bGameplayAllowsRadio) {
		if (ped) {
			if(!ped->DyingOrDead()) {
#ifdef GTA_PC
				if (SampleManager.IsMP3RadioChannelAvailable()
#ifdef FIX_BUGS
					&& vehicle
#endif
					&& vehicle->m_nRadioStation < USERTRACK
					&& ControlsManager.GetIsKeyboardKeyJustDown(rsF9)
#ifndef FIX_BUGS
					&& vehicle // yeah right, totaly usefull check =P
#endif
					)
				{
					if (!UsesPoliceRadio(vehicle) && !UsesTaxiRadio(vehicle)) {
						gNumRetunePresses = 0;
						gRetuneCounter = RETUNE_TIME;
						RadioStaticCounter = 0;
						if (vehicle->m_nRadioStation < USERTRACK) {
							do
								++gNumRetunePresses;
							while (gNumRetunePresses + vehicle->m_nRadioStation < USERTRACK);
						}
					}
				}
#endif
				if (CPad::GetPad(0)->ChangeStationJustDown() && vehicle) {
					if (!UsesPoliceRadio(vehicle) && !UsesTaxiRadio(vehicle)) {
						gNumRetunePresses++;
						gRetuneCounter = RETUNE_TIME;
						RadioStaticCounter = 0;
					}
				}
#ifdef RADIO_SCROLL_TO_PREV_STATION
				else if(!CPad::GetPad(0)->ArePlayerControlsDisabled() && (CPad::GetPad(0)->GetMouseWheelDownJustDown() || CPad::GetPad(0)->GetMouseWheelUpJustDown()) && vehicle) {
					if(!UsesPoliceRadio(vehicle) && !UsesTaxiRadio(vehicle)) {
						int scrollNext = ControlsManager.GetControllerKeyAssociatedWithAction(VEHICLE_CHANGE_RADIO_STATION, MOUSE);
						int scrollPrev = scrollNext == rsMOUSEWHEELUPBUTTON ? rsMOUSEWHEELDOWNBUTTON
																			: scrollNext == rsMOUSEWHEELDOWNBUTTON ? rsMOUSEWHEELUPBUTTON : -1;

						if(scrollPrev != -1 && !ControlsManager.IsAnyVehicleActionAssignedToMouseKey(scrollPrev)) {
							gNumRetunePresses--;
							gRetuneCounter = RETUNE_TIME;
							RadioStaticCounter = 0;
							int track = gNumRetunePresses + vehicle->m_nRadioStation;
							while(track < 0) track += NUM_RADIOS + 1;
							while(track >= NUM_RADIOS + 1) track -= NUM_RADIOS + 1;
#ifdef GTA_PC
							if(!DMAudio.IsMP3RadioChannelAvailable() && track == USERTRACK) gNumRetunePresses--;
#endif
						}
					}
				}
#endif
			}
		}
	} else {
		nFramesSinceCutsceneEnded = -1;
		gNumRetunePresses = 0;
		gRetuneCounter = 0;
		m_bSetNextStation = FALSE;
	} 

	if (m_bUserResumedGame) {
		m_bRadioStreamReady = FALSE;
		m_bUserResumedGame = FALSE;
	}
	if (m_nPlayingTrack == NO_TRACK && m_nFrontendTrack == NO_TRACK)
		m_bRadioStreamReady = FALSE;

	if (m_bGameplayAllowsRadio) {
		if (m_bRadioStreamReady) {
			if (m_nAnnouncement < NO_TRACK) {
				if ((m_bAnnouncementInProgress || m_nFrontendTrack == m_nPlayingTrack) && ServiceAnnouncement()) {
					if (m_bAnnouncementInProgress) {
						m_bSetNextStation = FALSE;
						gNumRetunePresses = 0;
						gRetuneCounter = 0;
						return;
					}
					if (m_nAnnouncement == NO_TRACK) {
						m_nNextTrack = NO_TRACK;
						m_nFrontendTrack = GetCarTuning();
						m_bSetNextStation = FALSE;
						gRetuneCounter = 0;
						gNumRetunePresses = 0;
					}
				}
			}
#ifdef GTA_PC
			if (!m_bAnnouncementInProgress && m_nAnnouncement == NO_TRACK
				&& m_nPlayingTrack == STREAMED_SOUND_RADIO_MP3_PLAYER && !SampleManager.IsStreamPlaying())
				SampleManager.StartStreamedFile(STREAMED_SOUND_RADIO_MP3_PLAYER, 0);
#endif
			if (m_bRadioSetByScript) {
				if (UsesPoliceRadio(vehicle))
					m_nFrontendTrack = STREAMED_SOUND_RADIO_POLICE;
				else if (UsesTaxiRadio(vehicle))
					m_nFrontendTrack = STREAMED_SOUND_RADIO_TAXI;
				else {
					m_nFrontendTrack = m_nRadioStationScript;
					vehicle->m_nRadioStation = m_nRadioStationScript;
				}

				if (m_nRadioPosition != -1) {
					m_aTracks[m_nFrontendTrack].m_nPosition = m_nRadioPosition;
					m_aTracks[m_nFrontendTrack].m_nLastPosCheckTimer = CTimer::GetTimeInMillisecondsPauseMode();
				}

				gRetuneCounter = 0;
				gNumRetunePresses = 0;
				m_bSetNextStation = FALSE;
				m_bRadioSetByScript = FALSE;
			} else {
				// Because when you switch radio back and forth, gNumRetunePresses will be 0 but gRetuneCounter won't.
#ifdef RADIO_SCROLL_TO_PREV_STATION
				if(gRetuneCounter != 0) {
					if(gRetuneCounter > 1)
						gRetuneCounter--;
					else if(gRetuneCounter == 1) {
						m_bSetNextStation = TRUE;
						gRetuneCounter = 0;
					}
				}
#else
				if (gNumRetunePresses != 0) {
					if (--gRetuneCounter == 0) {
						m_bSetNextStation = TRUE;
						gRetuneCounter = 0;
					}
				}
#endif
				if (gRetuneCounter != 0) {
					int32 station = gNumRetunePresses + vehicle->m_nRadioStation;
#ifdef RADIO_SCROLL_TO_PREV_STATION
					while (station < 0) station += NUM_RADIOS + 1;
#endif
					while (station >= NUM_RADIOS + 1) station -= NUM_RADIOS + 1;

#ifdef GTA_PC
					// Scrolling back won't hit here, so increasing isn't problem
					if (!DMAudio.IsMP3RadioChannelAvailable() && station == USERTRACK) {
						++gNumRetunePresses;
						station = RADIO_OFF;
					}
#endif
					if (station == RADIO_OFF) {
						if (gRetuneCounter == RETUNE_TIME-1) { // One less then what switching radio sets, so runs right after turning off radio
							AudioManager.PlayOneShot(AudioManager.m_nFrontEndEntity, SOUND_FRONTEND_RADIO_TURN_OFF, 0.0f);
							RadioStaticCounter = 5;
						}
					} else {
#ifdef RADIO_SCROLL_TO_PREV_STATION
						if (vehicle->m_nRadioStation == RADIO_OFF && gRetuneCounter == RETUNE_TIME-1) // Right after turning on the radio
#else
						if (station == 0 && gRetuneCounter == RETUNE_TIME-1) // Right after turning on the radio
#endif
							AudioManager.PlayOneShot(AudioManager.m_nFrontEndEntity, SOUND_FRONTEND_RADIO_TURN_ON, 0.0f);
						AudioManager.DoPoliceRadioCrackle();
					}
				}
				if (RadioStaticCounter < 2 && CTimer::GetTimeInMilliseconds() > RadioStaticTimer + 800) {
					AudioManager.PlayOneShot(AudioManager.m_nFrontEndEntity, SOUND_RADIO_CHANGE, 0.0f);
					RadioStaticCounter++;
					RadioStaticTimer = CTimer::GetTimeInMilliseconds();
				}
				if (m_bSetNextStation)
					m_nFrontendTrack = GetNextCarTuning();
			}
			if (m_nFrontendTrack >= STREAMED_SOUND_CITY_AMBIENT && m_nFrontendTrack <= STREAMED_SOUND_AMBSIL_AMBIENT)
				SetUpCorrectAmbienceTrack();
			ServiceTrack(vehicle, ped);
			if (m_bSetNextStation)
				m_bSetNextStation = FALSE;
		} else {
			if (vehicle) {
				if(m_bRadioSetByScript) {
					if (UsesPoliceRadio(vehicle))
						m_nFrontendTrack = STREAMED_SOUND_RADIO_POLICE;
					else if (UsesTaxiRadio(vehicle))
						m_nFrontendTrack = STREAMED_SOUND_RADIO_TAXI;
					else {
						m_nFrontendTrack = m_nRadioStationScript;
						vehicle->m_nRadioStation = m_nRadioStationScript;
					}
					if(m_nRadioPosition != -1) {
						m_aTracks[m_nFrontendTrack].m_nPosition = m_nRadioPosition;
						m_aTracks[m_nFrontendTrack].m_nLastPosCheckTimer = CTimer::GetTimeInMillisecondsPauseMode();
					}
					m_bRadioSetByScript = FALSE;
				} else
					// This starts the radio when you enter the car.
					m_nFrontendTrack = GetCarTuning();
			} else
				m_nFrontendTrack = STREAMED_SOUND_RADIO_WAVE; // huh?
		}
	} else {
		if (m_bAnnouncementInProgress) {
			SampleManager.StopStreamedFile();
			if (SampleManager.IsStreamPlaying())
				return;
			g_bAnnouncementReadPosAlready = FALSE;
			m_nAnnouncement = NO_TRACK;
			m_bAnnouncementInProgress = FALSE;
			m_nNextTrack = NO_TRACK;
			m_nFrontendTrack = NO_TRACK;
			m_nPlayingTrack = NO_TRACK;
		}
		SetUpCorrectAmbienceTrack();
		ServiceTrack(nil, ped);
	}
}

void
cMusicManager::SetUpCorrectAmbienceTrack()
{
	switch (CGame::currArea)
	{
	case AREA_MAIN_MAP:
	case AREA_EVERYWHERE:
		if (CTheScripts::RiotIntensity > 0 && ((TheCamera.GetPosition() - vecRiotPosition).MagnitudeSqr() < SQR(MID_RIOT_DIST)))
			m_nFrontendTrack = STREAMED_SOUND_LAW4RIOT_AMBIENT;
		else if (TheCamera.DistanceToWater > (MAX_WATER_DIST - WATER_DIST_STEP)) {
			if ((CWeather::OldWeatherType == WEATHER_HURRICANE || CWeather::NewWeatherType == WEATHER_HURRICANE) && CWeather::Wind > 1.0f)
				m_nFrontendTrack = STREAMED_SOUND_HAVANA_CITY_AMBIENT;
			else
				m_nFrontendTrack = STREAMED_SOUND_CITY_AMBIENT;
		} else {
			if (CCullZones::bAtBeachForAudio) {
				if ((CWeather::OldWeatherType == WEATHER_HURRICANE || CWeather::NewWeatherType == WEATHER_HURRICANE) && CWeather::Wind > 1.0f)
					m_nFrontendTrack = STREAMED_SOUND_HAVANA_BEACH_AMBIENT;
				else
					m_nFrontendTrack = STREAMED_SOUND_BEACH_AMBIENT;
			} else {
				if ((CWeather::OldWeatherType == WEATHER_HURRICANE || CWeather::NewWeatherType == WEATHER_HURRICANE) && CWeather::Wind > 1.0f)
					m_nFrontendTrack = STREAMED_SOUND_HAVANA_WATER_AMBIENT;
				else
					m_nFrontendTrack = STREAMED_SOUND_WATER_AMBIENT;
			}

		}
		break;
	case AREA_STRIP_CLUB:
		m_nFrontendTrack = STREAMED_SOUND_STRIPCLUB_AMBIENT;
		break;
	case AREA_MALL:
		m_nFrontendTrack = STREAMED_SOUND_MALL_AMBIENT;
		break;
	case AREA_DIRT:
	case AREA_BLOOD:
	case AREA_OVALRING:
		m_nFrontendTrack = STREAMED_SOUND_DIRTRING_AMBIENT;
		break;
	case AREA_HOTEL:
		m_nFrontendTrack = STREAMED_SOUND_HOTEL_AMBIENT;
		break;
	case AREA_MALIBU_CLUB:
		m_nFrontendTrack = STREAMED_SOUND_MALIBU_AMBIENT;
		break;
	case AREA_MANSION:
	case AREA_BANK:
	case AREA_LAWYERS:
	case AREA_COFFEE_SHOP:
	case AREA_CONCERT_HALL:
	case AREA_STUDIO:
	case AREA_RIFLE_RANGE:
	case AREA_BIKER_BAR:
	case AREA_POLICE_STATION:
		m_nFrontendTrack = STREAMED_SOUND_AMBSIL_AMBIENT;
		break;
	}
}

void
cMusicManager::ServiceAmbience()
{
}

float
GetHeightScale()
{
	if (TheCamera.GetPosition().z <= 20.0f) return 1.0f;
	if (TheCamera.GetPosition().z >= 50.0f) return 0.0f;
	return 1.0f - (TheCamera.GetPosition().z - 20.0f) / 30.0f;
}

void
cMusicManager::ComputeAmbienceVol(bool8 reset, uint8 &outVolume)
{
	static float fVol = 0.0f;

	float fHeightScale = GetHeightScale();

	if (CTheScripts::RiotIntensity > 0) {
		float distToRiotSq = (TheCamera.GetPosition() - vecRiotPosition).MagnitudeSqr();
		if (distToRiotSq < SQR(MAX_RIOT_DIST)) {
			if (distToRiotSq < SQR(MID_RIOT_DIST)) {
				if (distToRiotSq < SQR(MIN_RIOT_DIST)) {
					outVolume = (CTheScripts::RiotIntensity * (MAX_VOLUME * fHeightScale)) / MAX_VOLUME;
					return;
				} else {
					outVolume = (CTheScripts::RiotIntensity * (1.0f - (Sqrt(distToRiotSq) - MIN_RIOT_DIST) / (MID_RIOT_DIST-MIN_RIOT_DIST)) * (MAX_VOLUME * fHeightScale)) / MAX_VOLUME;
					return;
				}
			} else {
				outVolume = (Sqrt(distToRiotSq) - MID_RIOT_DIST) / (MAX_RIOT_DIST-MID_RIOT_DIST) * (MAX_VOLUME * fHeightScale);
				return;
			}
		}
	}

	if (reset)
		fVol = 0.0f;
	else if (fVol < MAX_AMBIENCE_VOLUME) {
		if ((m_nPlayingTrack >= STREAMED_SOUND_HAVANA_CITY_AMBIENT) && (m_nPlayingTrack <= STREAMED_SOUND_HAVANA_BEACH_AMBIENT))
			fVol += 20.0f;
		else
			fVol += 1.0f;
		fVol = Min(fVol, MAX_AMBIENCE_VOLUME);
	}

	if ((m_nPlayingTrack >= STREAMED_SOUND_MALL_AMBIENT) && (m_nPlayingTrack <= STREAMED_SOUND_AMBSIL_AMBIENT)) {
		outVolume = fVol;
		return;
	}

	if (CWeather::OldWeatherType == WEATHER_HURRICANE || CWeather::NewWeatherType == WEATHER_HURRICANE) {
		if (CWeather::Wind > 1.0f) {
			outVolume = (CWeather::Wind - 1.0f) * fVol;
			return;
		}
		fVol = (1.0f - CWeather::Wind) * fVol;
	}
	
	if (TheCamera.DistanceToWater <= MAX_WATER_DIST) {
		if (TheCamera.DistanceToWater > (MAX_WATER_DIST-WATER_DIST_STEP))
			outVolume = ((TheCamera.DistanceToWater - (MAX_WATER_DIST-WATER_DIST_STEP)) / WATER_DIST_STEP * fVol * fHeightScale);
		else if (TheCamera.DistanceToWater > (MAX_WATER_DIST-WATER_DIST_STEP*2))
			outVolume = ((MAX_WATER_DIST-WATER_DIST_STEP) - fHeightScale) / WATER_DIST_STEP * fVol;
		else
			outVolume = fVol;
	} else
		outVolume = fVol;

}

bool8
cMusicManager::ServiceAnnouncement()
{
	if (m_bAnnouncementInProgress) {
		if (!SampleManager.IsStreamPlaying()) {
			if (m_nPlayingTrack != NO_TRACK) {
				m_nAnnouncement = NO_TRACK;
				m_bAnnouncementInProgress = FALSE;
				m_nPlayingTrack = NO_TRACK;
			}
		} else
			m_nPlayingTrack = m_nNextTrack;
	} else if (SampleManager.IsStreamPlaying()) {
		if (m_nPlayingTrack != NO_TRACK && !g_bAnnouncementReadPosAlready) {
			RecordRadioStats();
			m_aTracks[m_nPlayingTrack].m_nPosition = SampleManager.GetStreamedFilePosition();
			g_bAnnouncementReadPosAlready = TRUE;
			m_aTracks[m_nPlayingTrack].m_nLastPosCheckTimer = CTimer::GetTimeInMillisecondsPauseMode();
		}
		SampleManager.StopStreamedFile();
	} else {
		g_bAnnouncementReadPosAlready = FALSE;
		m_nPlayingTrack = NO_TRACK;
		m_nNextTrack = m_nAnnouncement;
		SampleManager.SetStreamedFileLoopFlag(FALSE);
		SampleManager.StartStreamedFile(m_nNextTrack, 0);
		SampleManager.SetStreamedVolumeAndPan(MAX_VOLUME, SURROUND_PAN(63, 30), FALSE);
		m_bAnnouncementInProgress = TRUE;
	}

	return TRUE;
}

void
cMusicManager::ServiceTrack(CVehicle *veh, CPed *ped)
{
	static bool8 bRadioStatsRecorded = FALSE;
	static bool8 bRadioStatsRecorded2 = FALSE;
	uint8 volume;
	if (!m_bTrackChangeStarted)
		m_nNextTrack = m_nFrontendTrack;
	if (gRetuneCounter != 0 || m_bSetNextStation) {
		if (SampleManager.IsStreamPlaying()) {
			if (m_nPlayingTrack != NO_TRACK && !bRadioStatsRecorded) {
				m_aTracks[m_nPlayingTrack].m_nPosition = SampleManager.GetStreamedFilePosition();
				m_aTracks[m_nPlayingTrack].m_nLastPosCheckTimer = CTimer::GetTimeInMillisecondsPauseMode();
				RecordRadioStats();
				bRadioStatsRecorded = TRUE;
			}
			SampleManager.SetStreamedVolumeAndPan(0, SURROUND_PAN(63, 30), FALSE);
			SampleManager.StopStreamedFile();
		}
		return;
	}

	if (bRadioStatsRecorded) {
		bRadioStatsRecorded = FALSE;
		m_nPlayingTrack = NO_TRACK;
	}

	if (m_nNextTrack != m_nPlayingTrack) {
		m_bTrackChangeStarted = TRUE;
		SampleManager.SetStreamedVolumeAndPan(0, SURROUND_PAN(63, 30), FALSE);

		if (AudioManager.m_FrameCounter % 2 != 0) return;

		if (!m_bVerifyNextTrackStartedToPlay && SampleManager.IsStreamPlaying()) {
			if (m_nPlayingTrack != NO_TRACK) {
				if (!bRadioStatsRecorded2) {
					m_aTracks[m_nPlayingTrack].m_nPosition = SampleManager.GetStreamedFilePosition();
					m_aTracks[m_nPlayingTrack].m_nLastPosCheckTimer = CTimer::GetTimeInMillisecondsPauseMode();
					bRadioStatsRecorded2 = TRUE;
					RecordRadioStats();
					if (m_nPlayingTrack >= STREAMED_SOUND_HAVANA_CITY_AMBIENT && m_nPlayingTrack <= STREAMED_SOUND_HAVANA_BEACH_AMBIENT) {
						if (m_nNextTrack >= STREAMED_SOUND_HAVANA_CITY_AMBIENT && m_nNextTrack <= STREAMED_SOUND_HAVANA_BEACH_AMBIENT)
							AudioManager.PlayOneShot(AudioManager.m_nFrontEndEntity, SOUND_FRONTEND_HURRICANE, 0.0);
					}
				}
			} else
				debug("m_nPlayingTrack == NO_TRACK, yet track playing - tidying up\n");
			SampleManager.SetStreamedVolumeAndPan(0, SURROUND_PAN(63, 30), FALSE);
			SampleManager.StopStreamedFile();
		} else {
			bRadioStatsRecorded2 = FALSE;
			if (SampleManager.IsStreamPlaying()) {
				m_nPlayingTrack = m_nNextTrack;
				m_bVerifyNextTrackStartedToPlay = FALSE;
				m_bTrackChangeStarted = FALSE;
				if (veh) {
#ifdef FIX_BUGS
					if (m_nPlayingTrack >= STREAMED_SOUND_CITY_AMBIENT && m_nPlayingTrack <= STREAMED_SOUND_AMBSIL_AMBIENT)
						veh->m_nRadioStation = RADIO_OFF;
					else if (m_nPlayingTrack < STREAMED_SOUND_CITY_AMBIENT)
						veh->m_nRadioStation = m_nPlayingTrack;
#else
					if (veh->m_nRadioStation >= STREAMED_SOUND_CITY_AMBIENT && veh->m_nRadioStation <= STREAMED_SOUND_AMBSIL_AMBIENT)
						veh->m_nRadioStation = RADIO_OFF;
					else
						veh->m_nRadioStation = m_nPlayingTrack;
#endif
				}
			} else {
				uint32 pos = GetTrackStartPos(m_nNextTrack);
				if (m_nNextTrack != NO_TRACK) {
					SampleManager.SetStreamedFileLoopFlag(TRUE);
					SampleManager.StartStreamedFile(m_nNextTrack, pos);
					if (m_nFrontendTrack >= STREAMED_SOUND_CITY_AMBIENT && m_nFrontendTrack <= STREAMED_SOUND_AMBSIL_AMBIENT) {
						ComputeAmbienceVol(TRUE, volume);
						SampleManager.SetStreamedVolumeAndPan(volume, SURROUND_PAN(63, 30), TRUE);
					} else {
						m_nVolumeLatency = 10;
						m_nCurrentVolume = 0;
						m_nMaxVolume = MAX_RADIO_VOLUME;
						SampleManager.SetStreamedVolumeAndPan(m_nCurrentVolume, SURROUND_PAN(63, 30), FALSE);
					} 
					if (m_nNextTrack < STREAMED_SOUND_CITY_AMBIENT)
						m_nLastTrackServiceTime = CTimer::GetTimeInMillisecondsPauseMode();
					m_bVerifyNextTrackStartedToPlay = TRUE;
				}
			}
		}
	} else if (m_nPlayingTrack >= STREAMED_SOUND_CITY_AMBIENT && m_nPlayingTrack <= STREAMED_SOUND_AMBSIL_AMBIENT) {
		ComputeAmbienceVol(FALSE, volume);
		SampleManager.SetStreamedVolumeAndPan(volume, SURROUND_PAN(63, 30), TRUE);
	} else { 
		if (CTimer::GetIsSlowMotionActive()) {
			if (TheCamera.pTargetEntity) {
				float DistToTargetSq = (TheCamera.pTargetEntity->GetPosition() - TheCamera.GetPosition()).MagnitudeSqr();
				if (DistToTargetSq < SQR(MAX_RADIO_DIST)) {
					if (DistToTargetSq < SQR(MIN_RADIO_DIST)) {
						if (AudioManager.ShouldDuckMissionAudio(0) || AudioManager.ShouldDuckMissionAudio(1))
							SampleManager.SetStreamedVolumeAndPan(m_nCurrentVolume, SURROUND_PAN(63, 30), FALSE);
						else if (gRetuneCounter != 0)
							SampleManager.SetStreamedVolumeAndPan(0, SURROUND_PAN(63, 30), FALSE);
						else
							SampleManager.SetStreamedVolumeAndPan(m_nCurrentVolume, SURROUND_PAN(63, 30), FALSE);
					} else {
						volume = ((MAX_RADIO_DIST - MIN_RADIO_DIST) - (Sqrt(DistToTargetSq) - MIN_RADIO_DIST)) / (MAX_RADIO_DIST - MIN_RADIO_DIST) * m_nCurrentVolume;
						uint8 pan;
						if (AudioManager.ShouldDuckMissionAudio(0) || AudioManager.ShouldDuckMissionAudio(1))
							volume >>= 2;
						if (volume > 0) {
							CVector panVec;
							AudioManager.TranslateEntity(&TheCamera.pTargetEntity->GetPosition(), &panVec);
							pan = AudioManager.ComputePan(MAX_RADIO_DIST, &panVec);
						} else
							pan = 0;
						if (gRetuneCounter != 0)
							volume = 0;
						SampleManager.SetStreamedVolumeAndPan(volume, SURROUND_PAN(pan, 30), FALSE);
					}
				} else
					SampleManager.SetStreamedVolumeAndPan(0, SURROUND_PAN(63, 30), FALSE);
			}
		} else {
			// regular gameplay
			if (AudioManager.ShouldDuckMissionAudio(0) || AudioManager.ShouldDuckMissionAudio(1)) { // some story character speaks important wisdom
				SampleManager.SetStreamedVolumeAndPan(Min(m_nCurrentVolume, (MAX_RADIO_VOLUME >> 2)), SURROUND_PAN(63, 30), FALSE);
				nFramesSinceCutsceneEnded = 0;
			} else {
				if (nFramesSinceCutsceneEnded != -1) {
					if (nFramesSinceCutsceneEnded < 20) {
						volume = Min(m_nCurrentVolume, (MAX_RADIO_VOLUME >> 2));
						nFramesSinceCutsceneEnded++;
					} else if (nFramesSinceCutsceneEnded < 40) {
						volume = Min(m_nCurrentVolume, 3 * (nFramesSinceCutsceneEnded - 20) + (MAX_RADIO_VOLUME >> 2));
						nFramesSinceCutsceneEnded++;
					} else {
						nFramesSinceCutsceneEnded = -1;
						volume = m_nCurrentVolume;
					}
				} else
					volume = m_nCurrentVolume;
				if (gRetuneCounter != 0)
					volume = 0;
				SampleManager.SetStreamedVolumeAndPan(volume, SURROUND_PAN(63, 30), FALSE);
			}
		}
		if (m_nVolumeLatency > 0)
			m_nVolumeLatency--;
		else if (m_nCurrentVolume < m_nMaxVolume)
			m_nCurrentVolume = Min(m_nMaxVolume, m_nCurrentVolume + 6);
	}
}

bool8
cMusicManager::ChangeRadioChannel()
{
	return TRUE;
}

void
cMusicManager::PreloadCutSceneMusic(tTrack track)
{
	if (m_bIsInitialised && !m_bDisabled && track < TOTAL_STREAMED_SOUNDS && m_nMusicMode == MUSICMODE_CUTSCENE) {
		AudioManager.ResetPoliceRadio();
		while (SampleManager.IsStreamPlaying()) {
			SampleManager.StopStreamedFile();
#ifdef GTA_PS2
			WaitVBlank();
#endif
		}
		SampleManager.PreloadStreamedFile(track);
		SampleManager.SetStreamedVolumeAndPan(MAX_VOLUME, SURROUND_PAN(63, 30), TRUE);
		m_nPlayingTrack = track;
	}
}

void
cMusicManager::PlayPreloadedCutSceneMusic(void)
{
	if (m_bIsInitialised && !m_bDisabled && m_nMusicMode == MUSICMODE_CUTSCENE) {
#ifdef GTA_PS2
		if (m_bAnnouncementInProgress) {
			g_bAnnouncementReadPosAlready = FALSE;
			m_nAnnouncement = NO_TRACK;
			m_bAnnouncementInProgress = FALSE;
			m_nNextTrack = NO_TRACK;
			m_nFrontendTrack = NO_TRACK;
			m_nPlayingTrack = NO_TRACK;
		}
#endif
		SampleManager.StartPreloadedStreamedFile();
	}
}

void
cMusicManager::StopCutSceneMusic(void)
{
	if (m_bIsInitialised && !m_bDisabled && m_nMusicMode == MUSICMODE_CUTSCENE) {
#ifdef GTA_PS2
		while (SampleManager.IsStreamPlaying()) {
			SampleManager.StopStreamedFile();
			WaitVBlank();
		}
#else
		SampleManager.StopStreamedFile();
#endif
		m_nPlayingTrack = NO_TRACK;
	}
}

void
cMusicManager::PlayFrontEndTrack(tTrack track, bool8 loopFlag)
{
	if (m_bIsInitialised && !m_bDisabled && track < TOTAL_STREAMED_SOUNDS && (m_nUpcomingMusicMode == MUSICMODE_FRONTEND || m_nMusicMode == MUSICMODE_FRONTEND)) {
		m_nFrontendTrack = track;
		m_FrontendLoopFlag = loopFlag;
		if (m_nMusicMode != MUSICMODE_FRONTEND)
			m_bEarlyFrontendTrack = TRUE;
	}
}

void
cMusicManager::StopFrontEndTrack()
{
	if (m_nUpcomingMusicMode == MUSICMODE_FRONTEND || m_nMusicMode == MUSICMODE_FRONTEND)
		m_nFrontendTrack = NO_TRACK;
}

// these two were empty, the code is assumed
void
cMusicManager::Disable()
{
#ifndef FINAL
	m_bDisabled = TRUE;
#endif
}

void
cMusicManager::Enable()
{
#ifndef FINAL
	m_bDisabled = FALSE;
#endif
}

void
cMusicManager::PlayAnnouncement(tTrack announcement)
{
	if (m_bIsInitialised && !m_bDisabled && !m_bAnnouncementInProgress)
		m_nAnnouncement = announcement;
}

uint32
cMusicManager::GetTrackStartPos(tTrack track)
{
	if (!m_bIsInitialised) return 0;

	uint32 pos = m_aTracks[track].m_nPosition;
	if (CTimer::GetTimeInMillisecondsPauseMode() > m_aTracks[track].m_nLastPosCheckTimer)
		pos += Min(CTimer::GetTimeInMillisecondsPauseMode() - m_aTracks[track].m_nLastPosCheckTimer, 270000);
	else
		m_aTracks[track].m_nLastPosCheckTimer = CTimer::GetTimeInMillisecondsPauseMode();

	if (pos > m_aTracks[track].m_nLength)
		pos %= m_aTracks[track].m_nLength;
	return pos;
}

bool8
cMusicManager::PlayerInCar()
{
	CVehicle *vehicle = AudioManager.FindVehicleOfPlayer();
	if (vehicle) {
		PedState State = FindPlayerPed()->m_nPedState;

		if(State == PED_DRAG_FROM_CAR || State == PED_EXIT_CAR || State == PED_ARRESTED)
			return FALSE;

		if (vehicle->GetStatus() == STATUS_WRECKED)
			return FALSE;

		return TRUE;
	}
	return FALSE;
}

tTrack
cMusicManager::GetCarTuning()
{
	CVehicle *veh = AudioManager.FindVehicleOfPlayer();
	if (veh) {
		if (UsesPoliceRadio(veh)) return STREAMED_SOUND_RADIO_POLICE;
		if (UsesTaxiRadio(veh)) return STREAMED_SOUND_RADIO_TAXI;
#ifdef GTA_PC
		if (veh->m_nRadioStation == USERTRACK && !SampleManager.IsMP3RadioChannelAvailable())
			veh->m_nRadioStation = AudioManager.m_anRandomTable[2] % USERTRACK;
#endif
		return veh->m_nRadioStation;
	}
	return RADIO_OFF;
}

tTrack
cMusicManager::GetNextCarTuning()
{
	CVehicle *veh = AudioManager.FindVehicleOfPlayer();
	if (veh) {
		if (UsesPoliceRadio(veh)) return STREAMED_SOUND_RADIO_POLICE;
		if (UsesTaxiRadio(veh)) return STREAMED_SOUND_RADIO_TAXI;
		if (gNumRetunePresses != 0) {
#ifdef RADIO_SCROLL_TO_PREV_STATION
			// m_nRadioStation is unsigned, so...
			int station = veh->m_nRadioStation + gNumRetunePresses;
			while(station < 0) station += NUM_RADIOS + 1;
			while(station >= NUM_RADIOS + 1) station -= NUM_RADIOS + 1;
			veh->m_nRadioStation = station;
#else
			veh->m_nRadioStation += gNumRetunePresses;
			while(veh->m_nRadioStation >= NUM_RADIOS + 1)
				veh->m_nRadioStation -= NUM_RADIOS + 1;
#endif
#ifdef GTA_PC
			DMAudio.IsMP3RadioChannelAvailable(); // woof, just call and do nothing =P they manipulate gNumRetunePresses on DisplayRadioStationName in this case
#endif
			gNumRetunePresses = 0;
		}
		return veh->m_nRadioStation;
	}
	return RADIO_OFF;
}

bool8
cMusicManager::UsesPoliceRadio(CVehicle *veh)
{
	switch (veh->GetModelIndex())
	{
	case MI_VCNMAV:
	case MI_POLMAV:
	case MI_COASTG:
	case MI_RHINO:
	case MI_BARRACKS:
		return TRUE;
	case MI_MRWHOOP:
	case MI_HUNTER:
		return FALSE;
	}
	return veh->UsesSiren();
}

bool8
cMusicManager::UsesTaxiRadio(CVehicle *veh)
{
	switch (veh->GetModelIndex())
	{
	case MI_KAUFMAN:
		return CTheScripts::bPlayerHasMetDebbieHarry;
	}
	return FALSE;
}

void
cMusicManager::RecordRadioStats()
{
	if (m_nPlayingTrack < NUM_RADIOS) {
		double time /*Rusty*/ = CTimer::GetTimeInMillisecondsPauseMode();
		if (time > m_nLastTrackServiceTime)
			aListenTimeArray[m_nPlayingTrack] += time - m_nLastTrackServiceTime;
	}
}

float*
cMusicManager::GetListenTimeArray()
{
	return aListenTimeArray;
}

uint32
cMusicManager::GetRadioPosition(tTrack station)
{
	if (station < NUM_RADIOS)
		return GetTrackStartPos(station);
	return 0;
}

tTrack
cMusicManager::GetFavouriteRadioStation()
{
	tTrack favstation = 0;

	for (tTrack i = 1; i < NUM_RADIOS; i++) {
		if (aListenTimeArray[i] > aListenTimeArray[favstation])
			favstation = i;
	}

	return favstation;
}

bool8
cMusicManager::CheckForMusicInterruptions()
{
	return (m_nPlayingTrack == STREAMED_SOUND_MISSION_COMPLETED) || (m_nPlayingTrack == STREAMED_SOUND_CUTSCENE_FINALE);
}

void
cMusicManager::SetMalibuClubTrackPos(uint8 scriptObject)
{
	if (!m_bIsInitialised)
		m_aTracks[STREAMED_SOUND_MALIBU_AMBIENT].m_nPosition = 8640;
	if (m_nNextTrack != STREAMED_SOUND_MALIBU_AMBIENT && m_nPlayingTrack != STREAMED_SOUND_MALIBU_AMBIENT) {
		switch (scriptObject)
		{
		case SCRIPT_SOUND_NEW_BUILDING_MALIBU_1:
			m_aTracks[STREAMED_SOUND_MALIBU_AMBIENT].m_nPosition = (AudioManager.m_anRandomTable[0] % 128) + 8640;
			break;
		case SCRIPT_SOUND_NEW_BUILDING_MALIBU_2:
			m_aTracks[STREAMED_SOUND_MALIBU_AMBIENT].m_nPosition = (AudioManager.m_anRandomTable[0] % 128) + 286720;
			break;
		case SCRIPT_SOUND_NEW_BUILDING_MALIBU_3:
			m_aTracks[STREAMED_SOUND_MALIBU_AMBIENT].m_nPosition = (AudioManager.m_anRandomTable[0] % 128) + 509120;
			break;
		}
		m_aTracks[STREAMED_SOUND_MALIBU_AMBIENT].m_nLastPosCheckTimer = CTimer::GetTimeInMillisecondsPauseMode();
	}
}

void
cMusicManager::SetStripClubTrackPos(uint8 scriptObject)
{
	if (!m_bIsInitialised)
		m_aTracks[STREAMED_SOUND_STRIPCLUB_AMBIENT].m_nPosition = 0;
	if (m_nNextTrack != STREAMED_SOUND_STRIPCLUB_AMBIENT && m_nPlayingTrack != STREAMED_SOUND_STRIPCLUB_AMBIENT)
	{
		switch (scriptObject)
		{
		case SCRIPT_SOUND_NEW_BUILDING_STRIP_1:
			m_aTracks[STREAMED_SOUND_STRIPCLUB_AMBIENT].m_nPosition = AudioManager.m_anRandomTable[0] % 128;
			break;
		case SCRIPT_SOUND_NEW_BUILDING_STRIP_2:
			m_aTracks[STREAMED_SOUND_STRIPCLUB_AMBIENT].m_nPosition = (AudioManager.m_anRandomTable[0] % 128) + 320200;
			break;
		case SCRIPT_SOUND_NEW_BUILDING_STRIP_3:
			m_aTracks[STREAMED_SOUND_STRIPCLUB_AMBIENT].m_nPosition = (AudioManager.m_anRandomTable[0] % 128) + 672000;
			break;
		}
		m_aTracks[STREAMED_SOUND_STRIPCLUB_AMBIENT].m_nLastPosCheckTimer = CTimer::GetTimeInMillisecondsPauseMode();
	}
}

void
cMusicManager::DisplayRadioStationName()
{
	uint8 gStreamedSound;
	static wchar *pCurrentStation = nil;
	static uint8 cDisplay = 0;

#ifdef FIX_BUGS
	// Always show station text after entering car. Same behaviour as III and SA.
	wchar *pOldCurrentStation = pCurrentStation;
	pCurrentStation = nil;
#endif

	if (CTimer::GetIsPaused() || TheCamera.m_WideScreenOn) return;
	if (!PlayerInCar()) return;
	if (CReplay::IsPlayingBack()) return;

#ifdef FIX_BUGS
	pCurrentStation = pOldCurrentStation;
#endif

	CVehicle *vehicle = AudioManager.FindVehicleOfPlayer();

	if (!vehicle) return;

	// Prev scroll needs it to be signed, and m_nFrontendTrack can be NO_TRACK thus FIX_BUGS
#if defined RADIO_SCROLL_TO_PREV_STATION || defined FIX_BUGS
	int track;
#else
	uint8 track;
#endif
	gStreamedSound = vehicle->m_nRadioStation;
	if (gStreamedSound >= STREAMED_SOUND_CITY_AMBIENT && gStreamedSound <= STREAMED_SOUND_AMBSIL_AMBIENT)
		gStreamedSound = RADIO_OFF;
	if (gNumRetunePresses != 0) {
		track = gNumRetunePresses + gStreamedSound;
#ifdef RADIO_SCROLL_TO_PREV_STATION
		while (track < 0) track += NUM_RADIOS + 1;
#endif
		while (track >= NUM_RADIOS + 1) track -= NUM_RADIOS + 1;

#ifdef GTA_PC
		// On scrolling back we handle this condition on key press. No need to change this.
		if (!DMAudio.IsMP3RadioChannelAvailable() && track == USERTRACK)
			gNumRetunePresses++;
#endif
	} else
#ifdef RADIO_OFF_TEXT
		track = GetCarTuning(); // gStreamedSound or veh->m_nRadioStation would also work, but these don't cover police/taxi radios
#else
		track = m_nFrontendTrack;
#endif
	wchar* string = nil;
	switch (track) {
	case WILDSTYLE: string = TheText.Get("FEA_FM0"); break;
	case FLASH_FM: string = TheText.Get("FEA_FM1"); break;
	case KCHAT: string = TheText.Get("FEA_FM2"); break;
	case FEVER: string = TheText.Get("FEA_FM3"); break;
	case V_ROCK: string = TheText.Get("FEA_FM4"); break;
	case VCPR: string = TheText.Get("FEA_FM5"); break;
	case RADIO_ESPANTOSO: string = TheText.Get("FEA_FM6"); break;
	case EMOTION: string = TheText.Get("FEA_FM7"); break;
	case WAVE: string = TheText.Get("FEA_FM8"); break;
#ifdef GTA_PC
	case USERTRACK:
		if (SampleManager.IsMP3RadioChannelAvailable())
			string = TheText.Get("FEA_MP3");
		else
			return;
		break;
#endif
#ifdef RADIO_OFF_TEXT
	case RADIO_OFF: {
		// Otherwise RADIO OFF will be seen after pausing-resuming game and Mission Complete text
		if (!m_bRadioStreamReady || !m_bGameplayAllowsRadio)
			return;

		extern wchar WideErrorString[];

		string = TheText.Get("FEA_NON");
		if (string == WideErrorString) {
			pCurrentStation = nil;
			return;
		}
		break;
	}
#endif
	default: return;
	};

	if (pCurrentStation == string) {
		if (cDisplay == 0) return;
#ifdef FIX_BUGS
		cDisplay -= CTimer::GetLogicalFramesPassed();
#else
		cDisplay--;
#endif
	} else {
		pCurrentStation = string;
		cDisplay = 60;
	}

	CFont::SetJustifyOff();
	CFont::SetBackgroundOff();
	CFont::SetScale(SCREEN_SCALE_X(0.8f), SCREEN_SCALE_Y(1.35f));
	CFont::SetPropOn();
	CFont::SetFontStyle(FONT_STANDARD);
	CFont::SetCentreOn();
	CFont::SetCentreSize(SCREEN_STRETCH_X(DEFAULT_SCREEN_WIDTH));
	CFont::SetColor(CRGBA(0, 0, 0, 255));
	CFont::PrintString(SCREEN_WIDTH / 2 + SCREEN_SCALE_X(2.0f), SCREEN_SCALE_Y(22.0f) + SCREEN_SCALE_Y(2.0f), pCurrentStation);

	if (gNumRetunePresses)
		CFont::SetColor(CRGBA(102, 133, 143, 255));
	else
		CFont::SetColor(CRGBA(147, 196, 211, 255));

	CFont::PrintString(SCREEN_WIDTH / 2, SCREEN_SCALE_Y(22.0f), pCurrentStation);
	CFont::DrawFonts();
}

