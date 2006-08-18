#ifndef __SOUND__
#define __SOUND__

#define SND_TICRATE             140             // tic rate for updating sound
#define SND_MAXSONGS    40              // max number of songs in game
#define SND_SAMPLERATE  11025   // sample rate of sound effects

void I_SetSfxVolume(int volume);
int I_GetSfxLumpNum(sfxinfo_t *sound);
int I_StartSound (int id, /*void *data,*/ int vol, int sep, int pitch, int priority);
void I_StopSound(int handle);
int I_SoundIsPlaying(int handle);
void I_UpdateSoundParams(int handle, int vol, int sep, int pitch);
void I_sndArbitrateCards(void);
void I_StartupSound (void);
void I_ShutdownSound (void);
void I_SetChannels(int channels);
void I_InitSound(void);
void I_UpdateSounds(void);
void*
I_LoadSfx
( char*         sfxname,
  int*          len );

void I_UpdateSound(void *unused, Uint8 *stream, int len);

extern int snd_MusicVolume;

#endif
