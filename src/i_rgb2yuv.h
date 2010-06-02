/*
	RGB2YUV conversion functions
	taken from SDL test program testoverlay.c

*/

#ifndef I_RGB2YUV_H
#define I_RGB2YUV_H

#include <SDL.h>

void I_RGBtoYV12(SDL_Surface *s, SDL_Overlay *o);
void I_RGBtoIYUV(SDL_Surface *s, SDL_Overlay *o);
void I_RGBtoUYVY(SDL_Surface *s, SDL_Overlay *o);
void I_RGBtoYVYU(SDL_Surface *s, SDL_Overlay *o);
void I_RGBtoYUY2(SDL_Surface *s, SDL_Overlay *o);

#endif /* I_RGB2YUV_H */
