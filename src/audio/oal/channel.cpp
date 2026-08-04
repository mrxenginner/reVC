#include "common.h"

#ifdef AUDIO_OAL
#include "channel.h"
#include "sampman.h"

#ifndef _WIN32
#include <float.h>
#endif

extern bool IsFXSupported();
#ifdef FIX_BUGS
// Defined in sampman_oal.cpp behind the same guard, so without one here the Vanilla
// configuration references a symbol that was never emitted and fails at link (LNK2001).
extern size_t gPlayerTalkDataSize;
#endif

ALuint alSources[NUM_CHANNELS];
ALuint alFilters[NUM_CHANNELS];
ALuint alBuffers[NUM_CHANNELS];
bool bChannelsCreated = false;

uint8* tempStereoBuffer = nil;

void
CChannel::InitChannels()
{
	alGenSources(NUM_CHANNELS, alSources);
	alGenBuffers(NUM_CHANNELS, alBuffers);
	if (IsFXSupported())
		alGenFilters(NUM_CHANNELS, alFilters);

#ifdef FIX_BUGS
	tempStereoBuffer = new uint8[Max(PED_BLOCKSIZE, gPlayerTalkDataSize) * 2];
#else
	tempStereoBuffer = new uint8[PED_BLOCKSIZE * 2];
#endif
	bChannelsCreated = true;
}

void
CChannel::DestroyChannels()
{
	if (bChannelsCreated) 
	{
		alDeleteSources(NUM_CHANNELS, alSources);
		memset(alSources, 0, sizeof(alSources));
		alDeleteBuffers(NUM_CHANNELS, alBuffers);
		memset(alBuffers, 0, sizeof(alBuffers));
		if (IsFXSupported())
		{
			alDeleteFilters(NUM_CHANNELS, alFilters);
			memset(alFilters, 0, sizeof(alFilters));
		}
		delete[]tempStereoBuffer;
		bChannelsCreated = false;
	}
}


CChannel::CChannel()
{
	Data = nil;
	DataSize = 0;
	bIs2DDefault = bForce2D = false;
	SetDefault();
}

void CChannel::SetDefault()
{
	Pan = 63;
	Pitch = 1.0f;
	Gain = 1.0f;
	Mix = 0.0f;
		
	Position[0] = 0.0f; Position[1] = 0.0f; Position[2] = 0.0f;
	Distances[0] = 0.0f; Distances[1] = FLT_MAX;

	LoopPoints[0] = 0; LoopPoints[1] = -1;
	
	Frequency = MAX_FREQ;
	bForce2D = bIs2DDefault;
}

void CChannel::Reset()
{
	ClearBuffer();
	SetDefault();
}

void CChannel::Init(uint32 _id, bool Is2D)
{
	id = _id;
	if ( HasSource() )
	{
		alSourcei(alSources[id], AL_SOURCE_RELATIVE, AL_TRUE);
		if ( IsFXSupported() )
			alSource3i(alSources[id], AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
		
		if ( Is2D )
		{
			bIs2DDefault = bForce2D = true;
			alSource3f(alSources[id], AL_POSITION, 0.0f, 0.0f, 0.0f);
			alSourcef(alSources[id], AL_GAIN, 1.0f);
		}
	}
}

void CChannel::Term()
{
	Stop();
	Reset();
	if ( HasSource() )
	{
		if ( IsFXSupported() )
		{
			alSource3i(alSources[id], AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
		}
	}
}

void CChannel::Start()
{
	if ( !HasSource() ) return;
	if ( !Data ) return;

	if ( bForce2D )
	{
		// convert mono data to stereo
		int LVol = Pan <= 63 ? 128 : (127-Pan) / 64.0f * 128;
		int RVol = Pan >= 63 ? 128 : Pan / 64.0f * 128;

		int16 *monoData = (int16*)Data;
		int16 *stereoData = (int16*)tempStereoBuffer;
		for (size_t i = 0; i < DataSize / 2; i++)
		{
			*(stereoData++) = (*monoData * LVol) >> 7;
			*(stereoData++) = (*(monoData++) * RVol) >> 7;
		}
		alBufferData(alBuffers[id], AL_FORMAT_STEREO16, tempStereoBuffer, DataSize * 2, Frequency);
	}
	else
		alBufferData(alBuffers[id], AL_FORMAT_MONO16, Data, DataSize, Frequency);
	if ( LoopPoints[0] != 0 && LoopPoints[0] != -1 )
		alBufferiv(alBuffers[id], AL_LOOP_POINTS_SOFT, LoopPoints);
	alSourcei(alSources[id], AL_BUFFER, alBuffers[id]);
	alSourcePlay(alSources[id]);
}

void CChannel::Stop()
{
	if ( HasSource() )
		alSourceStop(alSources[id]);
}

bool CChannel::HasSource()
{
	return alSources[id] != AL_NONE;
}
	
bool CChannel::IsUsed()
{
	if ( HasSource() )
	{
		ALint sourceState;
		alGetSourcei(alSources[id], AL_SOURCE_STATE, &sourceState);
		return sourceState == AL_PLAYING;
	}
	return false;
}

void CChannel::SetPitch(float pitch)
{
	if ( !HasSource() ) return;
	alSourcef(alSources[id], AL_PITCH, pitch);
}

void CChannel::SetGain(float gain)
{
	if ( !HasSource() ) return;
	alSourcef(alSources[id], AL_GAIN, gain);
}
	
void CChannel::SetVolume(uint32 vol)
{
	SetGain(ALfloat(vol) / MAX_VOLUME);
}

void CChannel::SetSampleData(void *_data, size_t _DataSize, uint32 freq)
{
	Data = _data;
	DataSize = _DataSize;
	Frequency = freq;
}
	
void CChannel::SetCurrentFreq(uint32 freq)
{
	SetPitch(ALfloat(freq) / Frequency);
}

void CChannel::SetLoopCount(uint32 count)
{
	if ( !HasSource() ) return;

	// 0: loop indefinitely, 1: play one time, 2: play two times etc... in our case controlled by audio manager
	alSourcei(alSources[id], AL_LOOPING, count == 1 ? AL_FALSE : AL_TRUE);
}

void CChannel::SetLoopPoints(ALint start, ALint end)
{
	LoopPoints[0] = start;
	LoopPoints[1] = end;
}
	
void CChannel::SetPosition(float x, float y, float z)
{
	if ( !HasSource() ) return;
	alSource3f(alSources[id], AL_POSITION, x, y, z);
	bForce2D = false;
}
	
void CChannel::SetDistances(float max, float min)
{
	if ( !HasSource() ) return;
	alSourcef   (alSources[id], AL_MAX_DISTANCE,       max);
	alSourcef   (alSources[id], AL_REFERENCE_DISTANCE, min);
	alSourcef   (alSources[id], AL_MAX_GAIN, 1.0f);
	alSourcef   (alSources[id], AL_ROLLOFF_FACTOR, 1.0f);
	bForce2D = false;
}
	
void CChannel::SetPan(uint8 pan)
{
	if (IsUsed())
		debug("Channel %i changes pan during playback, we don't support that yet :(\n", id);

	// this is kinda pointless
	//SetPosition(((int)pan-63)/64.0f, 0.0f, -Sqrt(1.0f-SQR(((int)pan-63)/64.0f)));
	Pan = pan;
	bForce2D = true;
}

void CChannel::ClearBuffer()
{
	if ( !HasSource() ) return;
	alSourcei(alSources[id], AL_LOOPING, AL_FALSE);
	alSourcei(alSources[id], AL_BUFFER, AL_NONE);
	Data = nil;
	DataSize = 0;
}

void CChannel::SetReverbMix(ALuint slot, float mix)
{
	if ( !IsFXSupported() ) return;
	if ( !HasSource() ) return;
	if ( alFilters[id] == AL_FILTER_NULL ) return;
	
	Mix = mix;
	EAX3_SetReverbMix(alFilters[id], mix);
	alSource3i(alSources[id], AL_AUXILIARY_SEND_FILTER, slot, 0, alFilters[id]);
}

void CChannel::UpdateReverb(ALuint slot)
{
	if ( !IsFXSupported() ) return;
	if ( !HasSource() ) return;
	if ( alFilters[id] == AL_FILTER_NULL ) return;
	EAX3_SetReverbMix(alFilters[id], Mix);
	alSource3i(alSources[id], AL_AUXILIARY_SEND_FILTER, slot, 0, alFilters[id]);
}

#endif
