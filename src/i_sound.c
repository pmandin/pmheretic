// I_SOUND.C

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <math.h>

#include <SDL.h>

#include "doomdef.h"
#include "sounds.h"
#include "i_audio.h"

// The number of internal mixing channels,
//  the samples calculated for each mixing step,
//  the size of the 16bit, 2 hardware channel (stereo)
//  mixing buffer, and the samplerate of the raw data.

// Needed for calling the actual sound output.
#define NUM_CHANNELS		8

extern sysaudio_t sysaudio;

int snd_MaxVolume, snd_Channels = NUM_CHANNELS;

// The actual lengths of all sound effects.
int 		lengths[NUMSFX];

// The global mixing buffer.
// Basically, samples from all active internal channels
//  are modifed and added, and stored in the buffer
//  that is submitted to the audio device.
static Sint32 *tmpMixBuffer = NULL;
static Sint16 *tmpMixBuffer2 = NULL;
static int tmpMixBuffLen = 0;


// The number of internal mixing channels,
//  the samples calculated for each mixing step,
//  the size of the 16bit, 2 hardware channel (stereo)
//  mixing buffer, and the samplerate of the raw data.

typedef struct {
	// The channel data pointers, start and end.
	unsigned char *startp, *end;

	unsigned long length, position;

	// The channel step amount...
	unsigned int step;
	// ... and a 0.16 bit remainder of last step.
	unsigned int stepremainder;	

	// Time/gametic that the channel started playing,
	//  used to determine oldest, which automatically
	//  has lowest priority.
	// In case number of active sounds exceeds
	//  available channels.
	int start;

	// The sound in channel handles,
	//  determined on registration,
	//  might be used to unregister/stop/modify,
	//  currently unused.
	int 		handle;

	// SFX id of the playing sound effect.
	// Used to catch duplicates (like chainsaw).
	int		id;			

	// Hardware left and right channel volume lookup.
	int*		leftvol_lookup;
	int*		rightvol_lookup;

} mixchannel_t;

static mixchannel_t	channels[NUM_CHANNELS];

// Pitch to stepping lookup, unused.
int		steptable[256];

// Volume lookups.
//int		vol_lookup[128*256];
int *vol_lookup=NULL;

static SDL_bool quit = SDL_FALSE;

//
// This function loads the sound data from the WAD lump,
//  for single sound.
//
void*
I_LoadSfx
( char*         sfxname,
  int*          len )
{
    unsigned char*      sfx;
    int                 size;
    int                 sfxlump;


    
    // Now, there is a severe problem with the
    //  sound handling, in it is not (yet/anymore)
    //  gamemode aware. That means, sounds from
    //  DOOM II will be requested even with DOOM
    //  shareware.
    // The sound list is wired into sounds.c,
    //  which sets the external variable.
    // I do not do runtime patches to that
    //  variable. Instead, we will use a
    //  default sound for replacement.
	if ( W_CheckNumForName(sfxname) < 0 ) {
		sfxlump = W_GetNumForName("gldhit");
	} else {
		sfxlump = W_GetNumForName(sfxname);
	}

	size = W_LumpLength( sfxlump );

    // Debug.
    // fprintf( stderr, "." );
//    fprintf( stderr, " -loading  %s (lump %d, %d bytes)\n",
//   	     sfxname, sfxlump, size );
    //fflush( stderr );
    
    sfx = (unsigned char*)W_CacheLumpNum( sfxlump, PU_STATIC );

    // Preserve padded length.
    *len = size - 8;

    // Return allocated padded data.
    return (void *) (sfx + 8);
}




//
// This function adds a sound to the
//  list of currently active sounds,
//  which is maintained as a given number
//  (eight, usually) of internal channels.
// Returns a handle.
//
int
addsfx
( int		sfxid,
  int		volume,
  int		step,
  int		seperation )
{
	static unsigned short	handlenums = 0;

	int		i;
	int		rc = -1;

	int		oldest = gametic;
	int		oldestnum = 0;
	int		slot;

	int		rightvol;
	int		leftvol;

	// Loop all channels to find oldest SFX.
	for (i=0; (i<NUM_CHANNELS) && (channels[i].startp); i++) {
		if (channels[i].start < oldest) {
			oldestnum = i;
			oldest = channels[i].start;
		}
	}

	// Tales from the cryptic.
	// If we found a channel, fine.
	// If not, we simply overwrite the first one, 0.
	// Probably only happens at startup.
	if (i == NUM_CHANNELS)
		slot = oldestnum;
	else
		slot = i;

	// Okay, in the less recent channel,
	//  we will handle the new SFX.
	// Set pointer to raw data.
	channels[slot].startp = ((unsigned char *) S_sfx[sfxid].snd_ptr);
	// Set pointer to end of raw data.
	channels[slot].end = channels[slot].startp + S_sfx[sfxid].length;

	channels[slot].position = 0;
	channels[slot].length = S_sfx[sfxid].length;

	// Reset current handle number, limited to 0..100.
	if (!handlenums)
		handlenums = 100;

	// Assign current handle number.
	// Preserved so sounds could be stopped (unused).
	channels[slot].handle = rc = handlenums++;

	// Set stepping???
	// Kinda getting the impression this is never used.
	channels[slot].step = step;
	// ???
	channels[slot].stepremainder = 0;
	// Should be gametic, I presume.
	channels[slot].start = gametic;

	// Separation, that is, orientation/stereo.
	//  range is: 1 - 256
	seperation += 1;

	// Per left/right channel.
	//  x^2 seperation,
	//  adjust volume properly.
	leftvol = volume - ((volume*seperation*seperation) >> 16); ///(256*256);
	seperation = seperation - 257;
	rightvol = volume - ((volume*seperation*seperation) >> 16);	

	// Sanity check, clamp volume.
	if (rightvol < 0) rightvol = 0;
	if ( rightvol > 127) rightvol = 127;

	if (leftvol < 0) leftvol =0;
	if (leftvol > 127) leftvol = 127;

	// Get the proper lookup table piece
	//  for this volume level???
	channels[slot].leftvol_lookup = &vol_lookup[leftvol*256];
	channels[slot].rightvol_lookup = &vol_lookup[rightvol*256];

	// Preserve sound SFX id,
	//  e.g. for avoiding duplicates of chainsaw.
	channels[slot].id = sfxid;

	// You tell me.
	return rc;
}

void I_UpdateSounds(void)
{
	int i;

	for (i=0;i<NUM_CHANNELS;i++) {
		int sfxid;

		sfxid = channels[i].id;
		if ((S_sfx[sfxid].usefulness <= 0) && S_sfx[sfxid].snd_ptr) {
		    Z_ChangeTag(S_sfx[sfxid].snd_ptr - 8, PU_CACHE);
			S_sfx[sfxid].usefulness = 0;
		    S_sfx[sfxid].snd_ptr = NULL;
	    }
	}
}




//
// SFX API
// Note: this was called by S_Init.
// However, whatever they did in the
// old DPMS based DOS version, this
// were simply dummies in the Linux
// version.
// See soundserver initdata().
//
void I_SetChannels()
{
	// Init internal lookups (raw data, mixing buffer, channels).
	// This function sets up internal lookups used during
	//  the mixing process. 
	int		i;
	int		j;

	// Okay, reset internal mixing channels to zero.
	for (i=0; i<NUM_CHANNELS; i++)
	{
		channels[i].startp = NULL;
	}

	// This table provides step widths for pitch parameters.
	// I fail to see that this is currently used.
	for (i=-128 ; i<128 ; i++) {
		int newstep;

		newstep = (int)(pow(2.0, (i/64.0))*65536.0);
		/* FIXME: are all samples 11025Hz ? */
		newstep = (newstep*11025)/sysaudio.obtained.freq;
		steptable[i+128] = newstep;
	}


	// Generates volume lookup tables
	//  which also turn the unsigned samples
	//  into signed samples.
	vol_lookup = Z_Malloc(128*256*sizeof(int), PU_STATIC, NULL);

	for (i=0 ; i<128 ; i++)
		for (j=0 ; j<256 ; j++)
			vol_lookup[i*256+j] = (i*(j-128)*256)/127;
}	

 
void I_SetSfxVolume(int volume)
{
  // Identical to DOS.
  // Basically, this should propagate
  //  the menu/config file setting
  //  to the state variable used in
  //  the mixing.
  snd_MaxVolume = volume;
}


//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    return W_GetNumForName(sfx->name);
}

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//
int
I_StartSound
( int		id,
  int		vol,
  int		sep,
  int		pitch,
  int		priority )
{
	// UNUSED
	priority = 0;

	// Returns a handle (not used).
	id = addsfx( id, vol, steptable[pitch], sep );

	return id;
}



void I_StopSound (int handle)
{
	// You need the handle returned by StartSound.
	// Would be looping all channels,
	//  tracking down the handle,
	//  an setting the channel to zero.

	// UNUSED.
	handle = 0;
}


int I_SoundIsPlaying(int handle)
{
	// Ouch.
	return gametic < handle;
}




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
void I_UpdateSound(void *unused, Uint8 *stream, int len)
{
	int i, chan, srclen;
	boolean mixToFinal = false;
	Sint32 *source;
	Sint16 *dest;

	if (quit) {
		return;
	}

	memset(tmpMixBuffer, 0, tmpMixBuffLen);
 	srclen = len;
 	if (sysaudio.convert) {
 		srclen = (int) (len / sysaudio.audioCvt.len_ratio);
 	}

	/* Add each channel to tmp mix buffer */
	for ( chan = 0; chan < NUM_CHANNELS; chan++ ) {
		// Check channel, if active.
		if (!channels[ chan ].startp) {
			continue;
		}

		source = tmpMixBuffer;
		Uint8 *sample = channels[chan].startp;
		Uint32 position = channels[ chan ].position;
		Uint32 stepremainder = channels[chan].stepremainder;
		Uint32 step = channels[chan].step;
		int *leftvol = channels[chan].leftvol_lookup;
		int *rightvol = channels[chan].rightvol_lookup;

		Uint32 maxlen = FixedDiv(channels[chan].length-position, step);
		SDL_bool end_of_sample = SDL_FALSE;
		if ((srclen>>2) <= maxlen) {
			maxlen = srclen>>2;
		} else {
			end_of_sample = SDL_TRUE;
		}

		{
#if defined(__GNUC__) && (defined(__M68000__) || defined(__M68020__))
			Uint32	step_int = step>>16;
			Uint32	step_frac = step<<16;
#endif
			for (i=0; i<maxlen; i++) {
				unsigned int val;

				// Get the raw data from the channel. 
				val = sample[position];

				// Add left and right part
				//  for this channel (sound)
				//  to the current data.
				// Adjust volume accordingly.
				*source++ += leftvol[val];
				*source++ += rightvol[val];

#if defined(__GNUC__) && (defined(__M68000__) || defined(__M68020__))
				__asm__ __volatile__ (
						"addl	%3,%1\n"	\
					"	addxl	%2,%0"	\
				 	: /* output */
						"=d"(position), "=d"(stepremainder)
				 	: /* input */
						"d"(step_int), "r"(step_frac)
				 	: /* clobbered registers */
				 		"cc"
				);
#else
				// Increment index ???
				stepremainder += step;

				// MSB is next sample???
				position += stepremainder >> 16;

				// Limit to LSB???
				stepremainder &= 65536-1;
#endif
			}
		}

		if (end_of_sample) {
			channels[ chan ].startp = NULL;
			S_sfx[channels[chan].id].usefulness--;
		}
		channels[ chan ].position = position;
		channels[ chan ].stepremainder = stepremainder;
	}

	/* Now clip values for final buffer */
	source = tmpMixBuffer;	
	if (sysaudio.convert) {
		dest = (Sint16 *) tmpMixBuffer2;
	} else {
		dest = (Sint16 *) stream;
#ifdef ENABLE_SDLMIXER
		mixToFinal = true;
#endif
	}

	if (mixToFinal) {
		for (i=0; i<srclen>>2; i++) {
			Sint32 dl, dr;

			dl = *source++ + dest[0];
			dr = *source++ + dest[1];

			if (dl > 0x7fff)
				dl = 0x7fff;
			else if (dl < -0x8000)
				dl = -0x8000;

			*dest++ = dl;

			if (dr > 0x7fff)
				dr = 0x7fff;
			else if (dr < -0x8000)
				dr = -0x8000;

			*dest++ = dr;
		}
	} else {
		for (i=0; i<srclen>>2; i++) {
			Sint32 dl, dr;

			dl = *source++;
			dr = *source++;

			if (dl > 0x7fff)
				dl = 0x7fff;
			else if (dl < -0x8000)
				dl = -0x8000;

			*dest++ = dl;

			if (dr > 0x7fff)
				dr = 0x7fff;
			else if (dr < -0x8000)
				dr = -0x8000;

			*dest++ = dr;
		}
	}

	/* Conversion if needed */
	if (sysaudio.convert) {
		sysaudio.audioCvt.buf = (Uint8 *) tmpMixBuffer2;
		sysaudio.audioCvt.len = srclen;
		SDL_ConvertAudio(&sysaudio.audioCvt);

		SDL_MixAudio(stream, sysaudio.audioCvt.buf, len, SDL_MIX_MAXVOLUME);
	}
}


void
I_UpdateSoundParams
( int	handle,
  int	vol,
  int	sep,
  int	pitch)
{
  // I fail too see that this is used.
  // Would be using the handle to identify
  //  on which channel the sound might be active,
  //  and resetting the channel parameters.

  // UNUSED.
  handle = vol = sep = pitch = 0;
}

void I_ShutdownSound(void)
{    
	// Wait till all pending sounds are finished.
	int done = 0;
	int i;

	quit = SDL_TRUE;

	while ( !done ) {
		for( i=0 ; i<8 && !(channels[i].startp) ; i++);

		// FIXME. No proper channel output.
		//if (i==8)
			done=1;
	}

	if (tmpMixBuffer) {
		Z_Free(tmpMixBuffer);
		tmpMixBuffer = NULL;
	}

 	if (tmpMixBuffer2) {
 		Z_Free(tmpMixBuffer2);
 		tmpMixBuffer2=NULL;
 	}
}

void I_StartupSound(void)
{ 
#if 0
	int i, j;

	// Initialize external data (all sounds) at start, keep static.
	printf("I_InitSound: [                ]\x8");
	printf("\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8");

	j=0;
	for (i=1 ; i<NUMSFX ; i++) {
		if ((((i-1)*16)/(NUMSFX-2))>j) {
			j++;
			printf(".");
		}

		// Alias? Example is the chaingun sound linked to pistol.
		if (!S_sfx[i].link) {
			// Load data from WAD file.
			S_sfx[i].snd_ptr = getsfx( S_sfx[i].name, &lengths[i] );
		} else {
			// Previously loaded already?
			S_sfx[i].snd_ptr = S_sfx[i].link->snd_ptr;
			lengths[i] = lengths[(S_sfx[i].link - S_sfx)/sizeof(sfxinfo_t)];
		}
	}

	printf("\n");
#endif
}

void I_InitSound(void)
{
	tmpMixBuffLen = sysaudio.obtained.samples * 2 * sizeof(Sint32);
	tmpMixBuffer = Z_Malloc(tmpMixBuffLen, PU_STATIC, 0);
 	if (sysaudio.convert) {
 		tmpMixBuffer2 = Z_Malloc(tmpMixBuffLen>>1, PU_STATIC, 0);
 	}

	I_StartupSound();
}
