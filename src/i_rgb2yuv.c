/*
	RGB2YUV conversion functions
	taken from SDL test program testoverlay.c

*/

#include <SDL.h>

static inline void RGBtoYUV(Uint8 *rgb, int *yuv)
{
#if 1
	yuv[0] = 0.299*rgb[0] + 0.587*rgb[1] + 0.114*rgb[2];
	yuv[1] = (rgb[2]-yuv[0])*0.565 + 128;
	yuv[2] = (rgb[0]-yuv[0])*0.713 + 128;
#else
	yuv[0] = (77*rgb[0] + 150*rgb[1] +29*rgb[2])>>8;
	yuv[1] = ((rgb[2]-yuv[0])*145 + 256*128)>>8;
	yuv[2] = ((rgb[0]-yuv[0])*182 + 256*128)>>8;
#endif
}

void I_RGBtoYV12(SDL_Surface *s, SDL_Overlay *o)
{
	int x,y;
	int yuv[3];
	Uint8 *p,*op[3];

	SDL_LockSurface(s);
	SDL_LockYUVOverlay(o);

	/* Black initialization */
	/*
	memset(o->pixels[0],0,o->pitches[0]*o->h);
	memset(o->pixels[1],128,o->pitches[1]*((o->h+1)/2));
	memset(o->pixels[2],128,o->pitches[2]*((o->h+1)/2));
	*/

	/* Convert */
	for(y=0; y<s->h && y<o->h; y++)
	{
		p=((Uint8 *) s->pixels)+s->pitch*y;
		op[0]=o->pixels[0]+o->pitches[0]*y;
		op[1]=o->pixels[1]+o->pitches[1]*(y/2);
		op[2]=o->pixels[2]+o->pitches[2]*(y/2);
		for(x=0; x<s->w && x<o->w; x++)
		{
			RGBtoYUV(p, yuv);
			*(op[0]++)=yuv[0];
			if(x%2==0 && y%2==0)
			{
				*(op[1]++)=yuv[2];
				*(op[2]++)=yuv[1];
			}
			p+=s->format->BytesPerPixel;
		}
	}

	SDL_UnlockYUVOverlay(o);
	SDL_UnlockSurface(s);
}
 
void I_RGBtoIYUV(SDL_Surface *s, SDL_Overlay *o)
{
	int x,y;
	int yuv[3];
	Uint8 *p,*op[3];

	SDL_LockSurface(s);
	SDL_LockYUVOverlay(o);

	/* Black initialization */
	/*
	memset(o->pixels[0],0,o->pitches[0]*o->h);
	memset(o->pixels[1],128,o->pitches[1]*((o->h+1)/2));
	memset(o->pixels[2],128,o->pitches[2]*((o->h+1)/2));
	*/

	/* Convert */
	for(y=0; y<s->h && y<o->h; y++)
	{
		p=((Uint8 *) s->pixels)+s->pitch*y;
		op[0]=o->pixels[0]+o->pitches[0]*y;
		op[1]=o->pixels[1]+o->pitches[1]*(y/2);
		op[2]=o->pixels[2]+o->pitches[2]*(y/2);
		for(x=0; x<s->w && x<o->w; x++)
		{
			RGBtoYUV(p,yuv);
			*(op[0]++)=yuv[0];
			if(x%2==0 && y%2==0)
			{
				*(op[1]++)=yuv[1];
				*(op[2]++)=yuv[2];
			}
			p+=s->format->BytesPerPixel;
		}
	}

	SDL_UnlockYUVOverlay(o);
	SDL_UnlockSurface(s);
}

void I_RGBtoUYVY(SDL_Surface *s, SDL_Overlay *o)
{
	int x,y;
	int yuv[3];
	Uint8 *p,*op;

	SDL_LockSurface(s);
	SDL_LockYUVOverlay(o);

	for(y=0; y<s->h && y<o->h; y++)
	{
		p=((Uint8 *) s->pixels)+s->pitch*y;
		op=o->pixels[0]+o->pitches[0]*y;
		for(x=0; x<s->w && x<o->w; x++)
		{
			RGBtoYUV(p, yuv);
			if(x%2==0)
			{
				*(op++)=yuv[1];
				*(op++)=yuv[0];
				*(op++)=yuv[2];
			}
			else
				*(op++)=yuv[0];

			p+=s->format->BytesPerPixel;
		}
	}

	SDL_UnlockYUVOverlay(o);
	SDL_UnlockSurface(s);
}

void I_RGBtoYVYU(SDL_Surface *s, SDL_Overlay *o)
{
	int x,y;
	int yuv[3];
	Uint8 *p,*op;

	SDL_LockSurface(s);
	SDL_LockYUVOverlay(o);

	for(y=0; y<s->h && y<o->h; y++)
	{
		p=((Uint8 *) s->pixels)+s->pitch*y;
		op=o->pixels[0]+o->pitches[0]*y;
		for(x=0; x<s->w && x<o->w; x++)
		{
			RGBtoYUV(p,yuv);
			if(x%2==0)
			{
				*(op++)=yuv[0];
				*(op++)=yuv[2];
				op[1]=yuv[1];
			}
			else
			{
				*op=yuv[0];
				op+=2;
			}

			p+=s->format->BytesPerPixel;
		}
	}

	SDL_UnlockYUVOverlay(o);
	SDL_UnlockSurface(s);
}

void I_RGBtoYUY2(SDL_Surface *s, SDL_Overlay *o)
{
	int x,y;
	int yuv[3];
	Uint8 *p,*op;

	SDL_LockSurface(s);
	SDL_LockYUVOverlay(o);

	for(y=0; y<s->h && y<o->h; y++)
	{
		p=((Uint8 *) s->pixels)+s->pitch*y;
		op=o->pixels[0]+o->pitches[0]*y;
		for(x=0; x<s->w && x<o->w; x++)
		{
			RGBtoYUV(p,yuv);
			if(x%2==0)
			{
				*(op++)=yuv[0];
				*(op++)=yuv[1];
				op[1]=yuv[2];
			}
			else
			{
				*op=yuv[0];
				op+=2;
			}

			p+=s->format->BytesPerPixel;
		}
	}

	SDL_UnlockYUVOverlay(o);
	SDL_UnlockSurface(s);
}
