/*
===============================================================================

		MUSIC & SFX API

===============================================================================
*/

#include <stdlib.h>

#include <SDL.h>

#include "doomdata.h"
#include "doomdef.h"
#include "sounds.h"
#include "soundst.h"
#include "p_local.h"
#include "i_audio.h"
#include "i_sound.h"
#include "i_music.h"

extern void **lumpcache;

static channel_t channel[MAX_CHANNELS];

static int rs; //the current registered song.
int mus_song = -1;
int mus_lumpnum;
void *mus_sndptr;
byte *soundCurve;

extern sfxinfo_t S_sfx[];
extern musicinfo_t S_music[];

extern int snd_MaxVolume;
extern int snd_MusicVolume;
extern int snd_Channels;

extern int startepisode;
extern int startmap;

int AmbChan;

void S_Start(void)
{
	int i;

	S_StartSong((gameepisode-1)*9 + gamemap-1, true);

	//stop all sounds
	for(i=0; i < snd_Channels; i++)
	{
		if(channel[i].handle)
		{
			S_StopSound(channel[i].mo);
		}
	}
	memset(channel, 0, 8*sizeof(channel_t));
}

void S_StartSong(int song, boolean loop)
{
	if(song == mus_song)
	{ // don't replay an old song
		return;
	}
	if(rs)
	{
		I_StopSong(rs);
		I_UnRegisterSong(rs);
		Z_ChangeTag(lumpcache[mus_lumpnum], PU_CACHE);
	}
	if(song < mus_e1m1 || song > NUMMUSIC)
	{
		return;
	}
	mus_lumpnum = W_GetNumForName(S_music[song].name);
	mus_sndptr = W_CacheLumpNum(mus_lumpnum, PU_MUSIC);
	rs = I_RegisterSong(mus_sndptr, W_LumpLength(mus_lumpnum));
	I_PlaySong(rs, loop); //'true' denotes endless looping.
	mus_song = song;
}

void S_StartSound(mobj_t *origin, int sound_id)
{
	int dist, vol;
	int i;
//	int sound;
	int priority;
	int sep;
	int angle;
	int absx;
	int absy;
	sfxinfo_t *sfx;

	static int sndcount = 0;
	int chan;

	if (!sysaudio.enabled)
		return;

	if(sound_id==0 || snd_MaxVolume == 0)
		return;

	absx = absy = 0;
	if(origin != NULL)
	{
		absx = abs(origin->x-players[consoleplayer].mo->x);
		absy = abs(origin->y-players[consoleplayer].mo->y);
	}

// calculate the distance before other stuff so that we can throw out
// sounds that are beyond the hearing range.
//	absx = abs(origin->x-players[consoleplayer].mo->x);
//	absy = abs(origin->y-players[consoleplayer].mo->y);
	dist = absx+absy-(absx > absy ? absy>>1 : absx>>1);
	dist >>= FRACBITS;
//  dist = P_AproxDistance(origin->x-viewx, origin->y-viewy)>>FRACBITS;

	if(dist >= MAX_SND_DIST)
	{
//      dist = MAX_SND_DIST - 1;
	  return; //sound is beyond the hearing range...
	}
	if(dist < 0)
	{
		dist = 0;
	}
	priority = S_sfx[sound_id].priority;
	priority *= (10 - (dist/160));
	if(!S_StopSoundID(sound_id, priority))
	{
		return; // other sounds have greater priority
	}
	i=0;
	if (origin) {
		for(i=0; i<snd_Channels; i++)
		{
			if(origin->player)
			{
				i = snd_Channels;
				break; // let the player have more than one sound.
			}
			if(origin == channel[i].mo)
			{ // only allow other mobjs one sound
				S_StopSound(channel[i].mo);
				break;
			}
		}
	}
	if(i >= snd_Channels)
	{
		if(sound_id >= sfx_wind)
		{
			if(AmbChan != -1 && S_sfx[sound_id].priority <=
				S_sfx[channel[AmbChan].sound_id].priority)
			{
				return; //ambient channel already in use
			}
			else
			{
				AmbChan = -1;
			}
		}
		for(i=0; i<snd_Channels; i++)
		{
			if(channel[i].mo == NULL)
			{
				break;
			}
		}
		if(i >= snd_Channels)
		{
			//look for a lower priority sound to replace.
			sndcount++;
			if(sndcount >= snd_Channels)
			{
				sndcount = 0;
			}
			for(chan=0; chan < snd_Channels; chan++)
			{
				i = (sndcount+chan)%snd_Channels;
				if(priority >= channel[i].priority)
				{
					chan = -1; //denote that sound should be replaced.
					break;
				}
			}
			if(chan != -1)
			{
				return; //no free channels.
			}
			else //replace the lower priority sound.
			{
				if(channel[i].handle)
				{
					if(I_SoundIsPlaying(channel[i].handle))
					{
						I_StopSound(channel[i].handle);
					}
					if(S_sfx[channel[i].sound_id].usefulness > 0)
					{
						S_sfx[channel[i].sound_id].usefulness--;
					}

					if(AmbChan == i)
					{
						AmbChan = -1;
					}
				}
			}
		}
	}
	sfx = &S_sfx[sound_id];
	if(sfx->lumpnum == 0)
	{
		if (sfx->link) {
			sfx->lumpnum = I_GetSfxLumpNum(sfx->link);
		} else {
			sfx->lumpnum = I_GetSfxLumpNum(sfx);
		}
	}
	if(sfx->snd_ptr == NULL) {
		int length;
		char *name;
		
		name = sfx->name;
		if (sfx->link) {
			name = (sfx->link)->name;
		}

		sfx->snd_ptr = I_LoadSfx(name, &length);
		sfx->length = length;
		sfx->usefulness=0;
	}
	sfx->usefulness++;

	// calculate the volume based upon the distance from the sound origin.
//      vol = (snd_MaxVolume*16 + dist*(-snd_MaxVolume*16)/MAX_SND_DIST)>>9;
	vol = soundCurve[dist];

	if ((origin==NULL) || (origin == players[consoleplayer].mo)) {
		sep = 128;
	} else {
		angle = R_PointToAngle2(players[consoleplayer].mo->x,
			players[consoleplayer].mo->y, origin->x, origin->y);
		angle = (angle-viewangle)>>24;
		sep = angle*2-128;
		if(sep < 64)
			sep = -sep;
		if(sep > 192)
			sep = 512-sep;
	}

	channel[i].pitch = (byte)(127+(M_Random()&7)-(M_Random()&7));
	channel[i].handle = I_StartSound(sound_id, /*S_sfx[sound_id].snd_ptr,*/ vol, sep, channel[i].pitch, 0);
	channel[i].mo = origin;
	channel[i].sound_id = sound_id;
	channel[i].priority = priority;
	if(sound_id >= sfx_wind)
	{
		AmbChan = i;
	}
}

void S_StartSoundAtVolume(mobj_t *origin, int sound_id, int volume)
{
	int i;
	sfxinfo_t *sfx;

	if (!sysaudio.enabled)
		return;

	if(sound_id == 0 || snd_MaxVolume == 0)
		return;
	if(origin == NULL) {
		origin = players[consoleplayer].mo;
	}

	if(volume == 0) {
		return;
	}
	volume = (volume*(snd_MaxVolume+1)*8)>>7;

// no priority checking, as ambient sounds would be the LOWEST.
	for(i=0; i<snd_Channels; i++)
	{
		if(channel[i].mo == NULL)
		{
			break;
		}
	}
	if(i >= snd_Channels)
	{
		return;
	}
	sfx = &S_sfx[sound_id];
	if(sfx->lumpnum == 0)
	{
		if (sfx->link) {
			sfx->lumpnum = I_GetSfxLumpNum(sfx->link);
		} else {
			sfx->lumpnum = I_GetSfxLumpNum(sfx);
		}
	}
	if(sfx->snd_ptr == NULL) {
		int length;
		char *name;

		name = sfx->name;
		if (sfx->link) {
			name = (sfx->link)->name;
		}

		sfx->snd_ptr = I_LoadSfx(name, &length);
		sfx->length = length;
		if (sfx->usefulness<0) {
			sfx->usefulness=0;
		}
	}
	sfx->usefulness++;

	channel[i].pitch = (byte)(127-(M_Random()&3)+(M_Random()&3));
	channel[i].handle = I_StartSound(sound_id, /*S_sfx[sound_id].snd_ptr,*/ volume, 128, channel[i].pitch, 0);
	channel[i].mo = origin;
	channel[i].sound_id = sound_id;
	channel[i].priority = 1; //super low priority.
}

boolean S_StopSoundID(int sound_id, int priority)
{
	int i;
	int lp; //least priority
	int found;

	if(S_sfx[sound_id].numchannels == -1)
	{
		return(true);
	}
	lp = -1; //denote the argument sound_id
	found = 0;
	for(i=0; i<snd_Channels; i++)
	{
		if(channel[i].sound_id == sound_id && channel[i].mo)
		{
			found++; //found one.  Now, should we replace it??
			if(priority >= channel[i].priority)
			{ // if we're gonna kill one, then this'll be it
				lp = i;
				priority = channel[i].priority;
			}
		}
	}
	if(found < S_sfx[sound_id].numchannels)
	{
		return(true);
	}
	else if(lp == -1)
	{
		return(false); // don't replace any sounds
	}
	if(channel[lp].handle)
	{
		if(I_SoundIsPlaying(channel[lp].handle))
		{
			I_StopSound(channel[lp].handle);
		}
		if(S_sfx[channel[i].sound_id].usefulness > 0)
		{
			S_sfx[channel[i].sound_id].usefulness--;
		}
		channel[lp].mo = NULL;
	}
	return(true);
}

void S_StopSound(mobj_t *origin)
{
	int i;

	for(i=0;i<snd_Channels;i++)
	{
		if(channel[i].mo == origin)
		{
			I_StopSound(channel[i].handle);
			if(S_sfx[channel[i].sound_id].usefulness > 0)
			{
				S_sfx[channel[i].sound_id].usefulness--;
			}
			channel[i].handle = 0;
			channel[i].mo = NULL;
			if(AmbChan == i)
			{
				AmbChan = -1;
			}
		}
	}
}

void S_SoundLink(mobj_t *oldactor, mobj_t *newactor)
{
	int i;

	for(i=0;i<snd_Channels;i++)
	{
		if(channel[i].mo == oldactor)
			channel[i].mo = newactor;
	}
}

void S_PauseSound(void)
{
	I_PauseSong(rs);
}

void S_ResumeSound(void)
{
	I_ResumeSong(rs);
}

static int nextcleanup;

void S_UpdateSounds(mobj_t *listener)
{
	int i, dist, vol;
	int angle;
	int sep;
	int priority;
	int absx;
	int absy;

	if (!sysaudio.enabled)
		return;

	listener = players[consoleplayer].mo;
	if(snd_MaxVolume == 0)
	{
		return;
	}
	if(nextcleanup < gametic)
	{
		I_UpdateSounds();
#if 0
		for(i=0; i < NUMSFX; i++)
		{
			if(S_sfx[i].usefulness == 0 && S_sfx[i].snd_ptr)
			{
				if(lumpcache[S_sfx[i].lumpnum])
				{
					if(((memblock_t *)((byte *)(lumpcache[S_sfx[i].lumpnum])-
						sizeof(memblock_t)))->id == 0x1d4a11)
					{ // taken directly from the Z_ChangeTag macro
						Z_ChangeTag2(lumpcache[S_sfx[i].lumpnum], PU_CACHE);
					}
				}
				S_sfx[i].usefulness = -1;
//				S_sfx[i].snd_ptr = NULL;
			}
		}
#endif
		nextcleanup = gametic+35; //CLEANUP DEBUG cleans every second
	}
	for(i=0;i<snd_Channels;i++)
	{
		if(!channel[i].handle || S_sfx[channel[i].sound_id].usefulness == -1)
		{
			continue;
		}
		if(!I_SoundIsPlaying(channel[i].handle))
		{
			if(S_sfx[channel[i].sound_id].usefulness > 0)
			{
				S_sfx[channel[i].sound_id].usefulness--;
			}
			channel[i].handle = 0;
			channel[i].mo = NULL;
			channel[i].sound_id = 0;
			if(AmbChan == i)
			{
				AmbChan = -1;
			}
		}
		if(channel[i].mo == NULL || channel[i].sound_id == 0
			|| channel[i].mo == players[consoleplayer].mo)
		{
			continue;
		}
		else
		{
			absx = abs(channel[i].mo->x-players[consoleplayer].mo->x);
			absy = abs(channel[i].mo->y-players[consoleplayer].mo->y);
			dist = absx+absy-(absx > absy ? absy>>1 : absx>>1);
			dist >>= FRACBITS;
//          dist = P_AproxDistance(channel[i].mo->x-listener->x, channel[i].mo->y-listener->y)>>FRACBITS;

			if(dist >= MAX_SND_DIST)
			{
				S_StopSound(channel[i].mo);
				continue;
			}
			if(dist < 0)
				dist = 0;

// calculate the volume based upon the distance from the sound origin.
//          vol = (*((byte *)W_CacheLumpName("SNDCURVE", PU_CACHE)+dist)*(snd_MaxVolume*8))>>7;
			vol = soundCurve[dist];

			angle = R_PointToAngle2(players[consoleplayer].mo->x,
				players[consoleplayer].mo->y, channel[i].mo->x, channel[i].mo->y);
			angle = (angle-viewangle)>>24;
			sep = angle*2-128;
			if(sep < 64)
				sep = -sep;
			if(sep > 192)
				sep = 512-sep;
			I_UpdateSoundParams(channel[i].handle, vol, sep, channel[i].pitch);
			priority = S_sfx[channel[i].sound_id].priority;
			priority *= (10 - (dist>>8));
			channel[i].priority = priority;
		}
	}
}

void S_Init(void)
{
	soundCurve = Z_Malloc(MAX_SND_DIST, PU_STATIC, NULL);
	if(snd_Channels > 8)
	{
		snd_Channels = 8;
	}
	I_SetChannels(snd_Channels);
	I_SetMusicVolume(snd_MusicVolume);
	S_SetMaxVolume(true);
}

void S_GetChannelInfo(SoundInfo_t *s)
{
	int i;
	ChanInfo_t *c;

	s->channelCount = snd_Channels;
	s->musicVolume = snd_MusicVolume;
	s->soundVolume = snd_MaxVolume;
	for(i = 0; i < snd_Channels; i++)
	{
		c = &s->chan[i];
		c->id = channel[i].sound_id;
		c->priority = channel[i].priority;
		c->name = S_sfx[c->id].name;
		c->mo = channel[i].mo;
		c->distance = P_AproxDistance(c->mo->x-viewx, c->mo->y-viewy)
			>>FRACBITS;
	}
}

void S_SetMaxVolume(boolean fullprocess)
{
	int i;

	if(!fullprocess)
	{
		soundCurve[0] = (*((byte *)W_CacheLumpName("SNDCURVE", PU_CACHE))*(snd_MaxVolume*8))>>7;
	}
	else
	{
		for(i = 0; i < MAX_SND_DIST; i++)
		{
			soundCurve[i] = (*((byte *)W_CacheLumpName("SNDCURVE", PU_CACHE)+i)*(snd_MaxVolume*8))>>7;
		}
	}
}

static boolean musicPaused;
void S_SetMusicVolume(void)
{
	I_SetMusicVolume(snd_MusicVolume);
	if(snd_MusicVolume == 0)
	{
		I_PauseSong(rs);
		musicPaused = true;
	}
	else if(musicPaused)
	{
		musicPaused = false;
		I_ResumeSong(rs);
	}
}

void S_ShutDown(void)
{
	I_StopSong(rs);
	I_UnRegisterSong(rs);
}

