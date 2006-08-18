// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	System interface for sound.
//
//-----------------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <SDL.h>
#ifdef ENABLE_SDLMIXER
#include <SDL_mixer.h>
#endif

#include "doomdef.h"
#include "i_system.h"
#include "i_audio.h"
#include "i_music.h"
#include "i_sound.h"

sysaudio_t	sysaudio;

// The number of internal mixing channels,
//  the samples calculated for each mixing step,
//  the size of the 16bit, 2 hardware channel (stereo)
//  mixing buffer, and the samplerate of the raw data.

#define SAMPLECOUNT		512
#define SAMPLERATE		11025	// Hz

//
// This function loops all active (internal) sound
//  channels, retrieves a given number of samples
//  from the raw sound data, modifies it according
//  to the current (internal) channel parameters,
//  mixes the per channel samples into the global
//  mixbuffer, clamping it to the allowed range,
//  and sets up everything for transferring the
//  contents of the mixbuffer to the (two)
//  hardware channels (left and right, that is).
//
// This function currently supports only 16bit.
//

void I_UpdateAudio(void *unused, Uint8 *stream, int len)
{
	I_UpdateMusic(unused, stream, len);
	I_UpdateSound(unused, stream, len);
}

void I_ShutdownAudio(void)
{    
	if (!sysaudio.enabled)
		return;

	I_ShutdownSound();
	I_ShutdownMusic();

#ifdef ENABLE_SDLMIXER
	Mix_CloseAudio();
#else
	SDL_CloseAudio();
#endif
}

void I_InitAudio(void)
{ 
	sysaudio.obtained.freq = sysaudio.desired.freq =
		SAMPLERATE;
	sysaudio.obtained.format = sysaudio.desired.format =
		AUDIO_S16SYS;
	sysaudio.obtained.channels = sysaudio.desired.channels =
		2;
	sysaudio.obtained.samples = sysaudio.desired.samples =
		SAMPLECOUNT;
	sysaudio.obtained.size = sysaudio.desired.size =
		(SAMPLECOUNT*sysaudio.obtained.channels*((sysaudio.obtained.format&0xff)>>3));

	if (!sysaudio.enabled)
		return;

#ifdef ENABLE_SDLMIXER
	{
		int freq, channels;
		Uint16 format;

		if (Mix_OpenAudio(sysaudio.desired.freq, sysaudio.desired.format,
			sysaudio.desired.channels, sysaudio.desired.samples)<0) {
			I_Error("Could not open audio: %s\n", SDL_GetError());
		}

		if (!Mix_QuerySpec(&freq, &format, &channels)) {
			I_Error("Could not open audio: %s\n", SDL_GetError());
		}

		sysaudio.obtained.freq = freq;
		sysaudio.obtained.channels = channels;
		sysaudio.obtained.format = format;
		sysaudio.obtained.size = (SAMPLECOUNT*channels*((format&0xff)>>3));

		I_InitMusic();
		I_InitSound();

		Mix_SetPostMix(I_UpdateSound, NULL);
	}
#else
	sysaudio.desired.callback = I_UpdateAudio;
	sysaudio.desired.userdata = NULL;

	if (SDL_OpenAudio(&sysaudio.desired, &sysaudio.obtained)<0) {
		I_Error("Could not open audio: %s\n", SDL_GetError());
	}

	I_InitMusic();
	I_InitSound();

	SDL_PauseAudio(0);
#endif

	{
		char deviceName[32];

		if (SDL_AudioDriverName(deviceName, sizeof(deviceName))==NULL) {
			memset(deviceName, 0, sizeof(deviceName));
		}		
		deviceName[sizeof(deviceName)-1]='\0';
		
	fprintf(stderr, "Audio device: %s, %d Hz, %d bits, %d channels\n",
	/*, %d frames, %d bytes*/
		deviceName,
		sysaudio.obtained.freq,
		sysaudio.obtained.format & 0xff,
		sysaudio.obtained.channels/*,
		sysaudio.obtained.samples,
		sysaudio.obtained.size*/
	);
	}
}
