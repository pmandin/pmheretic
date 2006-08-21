// R_draw.c

#include "doomdef.h"
#include "r_local.h"
#include "i_video.h"

/*

All drawing to the view buffer is accomplished in this file.  The other refresh
files only know about ccordinates, not the architecture of the frame buffer.

*/

byte *viewimage;
int viewwidth, scaledviewwidth, viewheight, viewwindowx, viewwindowy;
byte **ylookup=NULL;
int *columnofs=NULL;
byte translations[3][256]; // color tables for different players
byte *tinttable; // used for translucent sprites

/*
==================
=
= R_DrawColumn
=
= Source is the top of the column to scale
=
==================
*/

lighttable_t	*dc_colormap;
int				dc_x;
int				dc_yl;
int				dc_yh;
fixed_t			dc_iscale;
fixed_t			dc_texturemid;
byte			*dc_source;		// first pixel in a column (possibly virtual)

int				dccount;		// just for profiling

void R_DrawColumn (void)
{
	int		count;
	byte		*dest;
	fixed_t		frac, fracstep;	

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
				
#ifdef RANGECHECK
	if ((unsigned)dc_x >= sysvideo.width || dc_yl < 0 || dc_yh >= sysvideo.height)
		I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	dest = ylookup[dc_yl] + columnofs[dc_x]; 
	
	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

#if defined(__GNUC__) && (defined(__M68000__) || defined(__M68020__))
    __asm__ __volatile__ (
	"moveql	#127,d0\n"
"	swap	%1\n"
"	swap	%2\n"
"	andw	d0,%2\n"
"	moveql	#0,d1\n"
"	movew	%0,d2\n"	/* d2 = 3-(count&3) */
"	notw	d2\n"
"	andw	#3,d2\n"
"	lea		R_DrawColumn_loop,a0\n"
"	muluw	#R_DrawColumn_loop1-R_DrawColumn_loop,d2\n"
"	lsrw	#2,%0\n"
"	move	#4,ccr\n"
"	jmp		a0@(0,d2:w)\n"

"R_DrawColumn_loop:\n"
"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%4@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	d1,%5@\n"
"	addw	%6,%5\n"

"R_DrawColumn_loop1:\n"
"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%4@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	d1,%5@\n"
"	addw	%6,%5\n"

"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%4@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	d1,%5@\n"
"	addw	%6,%5\n"

"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%4@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	d1,%5@\n"
"	addw	%6,%5\n"

/*"	subqw	#1,%0\n"
"	bpls	R_DrawColumn_loop"*/
"	dbra	%0,R_DrawColumn_loop"
	 	: /* no return value */
	 	: /* input */
	 		"d"(count), "d"(fracstep), "d"(frac), "a"(dc_source),
			"a"(dc_colormap), "a"(dest), "a"(sysvideo.pitch)
	 	: /* clobbered registers */
	 		"d0", "d1", "d2", "d3", "a0", "cc", "memory" 
	);
#else
# define RENDER_PIXEL	\
	{	\
		*dest = dc_colormap[dc_source[(frac>>FRACBITS)&127]];	\
		dest += sysvideo.pitch; 	\
		frac += fracstep;	\
	}

	{
		int n = count>>2;
		switch (count & 3) {
			case 3: do {
					RENDER_PIXEL;
			case 2:		RENDER_PIXEL;
			case 1:		RENDER_PIXEL;
			case 0:		RENDER_PIXEL;
				} while (--n>=0);
		}
	}
#undef RENDER_PIXEL
#endif
}

void R_DrawColumn060 (void) 
{ 
	int	count, rshift;
	byte	*dest, *dest_end;
	fixed_t	frac, fracstep;

	// Zero length, column does not exceed a pixel.
	if (dc_yh < dc_yl) 
		return; 
				 
#ifdef RANGECHECK 
	if ((unsigned)dc_x >= sysvideo.width
		|| dc_yl < 0 || dc_yh >= sysvideo.height) 
		I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x); 
#endif 

	count = dc_yh - dc_yl; 

	// Framebuffer destination address.
	// Use ylookup LUT to avoid multiply with ScreenWidth.
	// Use columnofs LUT for subwindows? 
	dest = ylookup[dc_yl] + columnofs[dc_x];  
	dest_end = &dest[count*sysvideo.pitch];

	// Determine scaling,
	//  which is the only mapping to be done.
	fracstep = dc_iscale; 
	frac = dc_texturemid + (dc_yl-centery)*fracstep; 

	fracstep <<= 16-7;
	frac <<= 16-7;
	rshift = 32-7;

#if defined(__GNUC__) && (defined(__M68000__) || defined(__M68020__))
    __asm__ __volatile__ (
"	moveql	#0,d1\n"
"	movel	%1,d0\n"
"	lsrl	%2,d0\n"

"R_DrawColumn060_loop:\n"
"	moveb	%3@(0,d0:w),d1\n"
"	addl	%0,%1\n"
"	moveb	%4@(0,d1:l),d1\n"
"	movel	%1,d0\n"
"	lsrl	%2,d0\n"
"	moveb	d1,%5@\n"
"	cmpal	%7,%5\n"
"	addw	%6,%5\n"

"	bnes	R_DrawColumn060_loop"
	 	: /* no return value */
	 	: /* input */
	 		"d"(fracstep), "d"(frac), "d"(rshift),
			"a"(dc_source), "a"(dc_colormap), "a"(dest),
			"a"(sysvideo.pitch), "a"(dest_end)
	 	: /* clobbered registers */
	 		"d0", "d1", "cc", "memory" 
	);
#endif
} 

void R_DrawColumnLow (void)
{
	int		n,count;
	byte		*dest;
	fixed_t		frac, fracstep;	

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
				
#ifdef RANGECHECK
	if ((unsigned)dc_x >= sysvideo.width || dc_yl < 0 || dc_yh >= sysvideo.height)
		I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
//	dccount++;
#endif

	dest = ylookup[dc_yl] + columnofs[dc_x<<1]; 
	
	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

#define RENDER_PIXEL	\
	{	\
		int spot;	\
		spot = dc_colormap[dc_source[(frac>>FRACBITS)&127]];	\
		*(unsigned short *)dest = spot|(spot<<8);	\
		dest += sysvideo.pitch; 	\
		frac += fracstep;	\
	}	

	n = count>>2;
	switch (count & 3) {
		case 3: do {
				RENDER_PIXEL;
		case 2:		RENDER_PIXEL;
		case 1:		RENDER_PIXEL;
		case 0:		RENDER_PIXEL;
			} while (--n>=0);
	}
#undef RENDER_PIXEL
}

void R_DrawFuzzColumn (void)
{
	int			n,count;
	byte		*dest;
	fixed_t		frac, fracstep;	

	if (!dc_yl)
		dc_yl = 1;
	if (dc_yh == viewheight-1)
		dc_yh = viewheight - 2;
		
	count = dc_yh - dc_yl;
	if (count < 0)
		return;
				
#ifdef RANGECHECK
	if ((unsigned)dc_x >= sysvideo.width || dc_yl < 0 || dc_yh >= sysvideo.height)
		I_Error ("R_DrawFuzzColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	dest = ylookup[dc_yl] + columnofs[dc_x];

	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

#define RENDER_PIXEL	\
	{	\
		*dest = tinttable[((*dest)<<8)+dc_colormap[dc_source[(frac>>FRACBITS)&127]]];	\
		dest += sysvideo.pitch;	\
		frac += fracstep;	\
	}

	n = count>>2;
	switch (count & 3) {
		case 3: do {
				RENDER_PIXEL;
		case 2:		RENDER_PIXEL;
		case 1:		RENDER_PIXEL;
		case 0:		RENDER_PIXEL;
			} while (--n>=0);
	}
#undef RENDER_PIXEL
}

void R_DrawFuzzColumnLow (void)
{
	int			n,count;
	byte		*dest;
	fixed_t		frac, fracstep;	

	if (!dc_yl)
		dc_yl = 1;
	if (dc_yh == viewheight-1)
		dc_yh = viewheight - 2;
		
	count = dc_yh - dc_yl;
	if (count < 0)
		return;
				
#ifdef RANGECHECK
	if ((unsigned)dc_x >= sysvideo.width || dc_yl < 0 || dc_yh >= sysvideo.height)
		I_Error ("R_DrawFuzzColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	dest = ylookup[dc_yl] + columnofs[dc_x<<1];

	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

#define RENDER_PIXEL	\
	{	\
		int color;	\
		\
		color = tinttable[((*dest)<<8)+dc_colormap[dc_source[(frac>>FRACBITS)&127]]];	\
		*((unsigned short *) dest) = color|(color<<8);	\
		dest += sysvideo.pitch;	\
		frac += fracstep;	\
	}

	n = count>>2;
	switch (count & 3) {
		case 3: do {
				RENDER_PIXEL;
		case 2:		RENDER_PIXEL;
		case 1:		RENDER_PIXEL;
		case 0:		RENDER_PIXEL;
			} while (--n>=0);
	}
#undef RENDER_PIXEL
}

/*
========================
=
= R_DrawTranslatedColumn
=
========================
*/

byte *dc_translation;
byte *translationtables;

void R_DrawTranslatedColumn (void)
{
	int			count;
	byte		*dest;
	fixed_t		frac, fracstep;	

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
				
#ifdef RANGECHECK
	if ((unsigned)dc_x >= sysvideo.width || dc_yl < 0 || dc_yh >= sysvideo.height)
		I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	dest = ylookup[dc_yl] + columnofs[dc_x];
	
	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

#if defined(__GNUC__) && (defined(__M68000__) || defined(__M68020__))
    __asm__ __volatile__ (
	"moveql	#127,d0\n"
"	swap	%1\n"
"	swap	%2\n"
"	andw	d0,%2\n"
"	moveql	#0,d1\n"
"	movew	%0,d2\n"	/* d2 = 3-(count&3) */
"	notw	d2\n"
"	andw	#3,d2\n"
"	lea		R_DrawTranslatedColumn_loop,a0\n"
"	muluw	#R_DrawTranslatedColumn_loop1-R_DrawTranslatedColumn_loop,d2\n"
"	lsrw	#2,%0\n"
"	move	#4,ccr\n"
"	jmp		a0@(0,d2:w)\n"

"R_DrawTranslatedColumn_loop:\n"
"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%7@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	%4@(0,d1:l),%5@\n"
"	addw	%6,%5\n"

"R_DrawTranslatedColumn_loop1:\n"
"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%7@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	%4@(0,d1:l),%5@\n"
"	addw	%6,%5\n"

"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%7@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	%4@(0,d1:l),%5@\n"
"	addw	%6,%5\n"

"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%7@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	%4@(0,d1:l),%5@\n"
"	addw	%6,%5\n"

/*"	subqw	#1,%0\n"
"	bpls	R_DrawTranslatedColumn_loop"*/
"	dbra	%0,R_DrawTranslatedColumn_loop"
	 	: /* no return value */
	 	: /* input */
	 		"d"(count), "d"(fracstep), "d"(frac), "a"(dc_source),
			"a"(dc_colormap), "a"(dest), "a"(sysvideo.pitch),
			"a"(dc_translation)
	 	: /* clobbered registers */
	 		"d0", "d1", "d2", "d3", "a0", "cc", "memory" 
	);
#else
#define RENDER_PIXEL	\
	*dest = dc_colormap[dc_translation[dc_source[(frac>>FRACBITS)&127]]];	\
	dest += sysvideo.pitch; 	\
	frac += fracstep;

	{
		int n = count>>2;
		switch (count & 3) {
			case 3: do {
					RENDER_PIXEL;
			case 2:		RENDER_PIXEL;
			case 1:		RENDER_PIXEL;
			case 0:		RENDER_PIXEL;
				} while (--n>=0);
		}
	}
#undef RENDER_PIXEL
#endif
}

void R_DrawTranslatedColumnLow (void)
{
	int			count;
	byte		*dest;
	fixed_t		frac, fracstep;	

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
				
#ifdef RANGECHECK
	if ((unsigned)dc_x >= sysvideo.width || dc_yl < 0 || dc_yh >= sysvideo.height)
		I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	dest = ylookup[dc_yl] + columnofs[dc_x<<1];
	
	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

# define RENDER_PIXEL	\
	{	\
		int spot;	\
		\
		spot = dc_colormap[dc_translation[dc_source[(frac>>FRACBITS)&127]]]; 	\
		\
		*((unsigned short *) dest) = spot|(spot<<8);	\
		\
		dest += sysvideo.pitch;	\
		frac += fracstep;	\
	}

	{
		int n = count>>2;
		switch (count & 3) {
			case 3: do {
					RENDER_PIXEL;
			case 2:		RENDER_PIXEL;
			case 1:		RENDER_PIXEL;
			case 0:		RENDER_PIXEL;
				} while (--n>=0);
		}
	}
#undef RENDER_PIXEL
}

void R_DrawTranslatedFuzzColumn (void)
{
	int			count;
	byte		*dest;
	fixed_t		frac, fracstep;	

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
				
#ifdef RANGECHECK
	if ((unsigned)dc_x >= sysvideo.width || dc_yl < 0 || dc_yh >= sysvideo.height)
		I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	dest = ylookup[dc_yl] + columnofs[dc_x<<1];
	
	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

#define RENDER_PIXEL	\
	{	\
		*dest =  tinttable[((*dest)<<8)	\
			+dc_colormap[dc_translation[dc_source[frac>>FRACBITS]]]];	\
		dest += sysvideo.pitch; 	\
		frac += fracstep;	\
	}

	{
		int n = count>>2;
		switch (count & 3) {
			case 3: do {
					RENDER_PIXEL;
			case 2:		RENDER_PIXEL;
			case 1:		RENDER_PIXEL;
			case 0:		RENDER_PIXEL;
				} while (--n>=0);
		}
	}
#undef RENDER_PIXEL
}

void R_DrawTranslatedFuzzColumnLow (void)
{
	int			count;
	byte		*dest;
	fixed_t		frac, fracstep;	

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
				
#ifdef RANGECHECK
	if ((unsigned)dc_x >= sysvideo.width || dc_yl < 0 || dc_yh >= sysvideo.height)
		I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	dest = ylookup[dc_yl] + columnofs[dc_x<<1];
	
	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

# define RENDER_PIXEL	\
	{	\
		int spot;	\
		\
		spot = tinttable[((*dest)<<8)	\
			+dc_colormap[dc_translation[dc_source[frac>>FRACBITS]]]];	\
		\
		*((unsigned short *) dest) = spot|(spot<<8);	\
		\
		dest += sysvideo.pitch;	\
		frac += fracstep;	\
	}

	{
		int n = count>>2;
		switch (count & 3) {
			case 3: do {
					RENDER_PIXEL;
			case 2:		RENDER_PIXEL;
			case 1:		RENDER_PIXEL;
			case 0:		RENDER_PIXEL;
				} while (--n>=0);
		}
	}
#undef RENDER_PIXEL
}

//--------------------------------------------------------------------------
//
// PROC R_InitTranslationTables
//
//--------------------------------------------------------------------------

void R_InitTranslationTables (void)
{
	int i;

	// Load tint table
	tinttable = W_CacheLumpName("TINTTAB", PU_STATIC);

	// Allocate translation tables
	translationtables = Z_Malloc(256*3+255, PU_STATIC, 0);
	translationtables = (byte *)(( (int)translationtables + 255 )& ~255);

	// Fill out the translation tables
	for(i = 0; i < 256; i++)
	{
		if(i >= 225 && i <= 240)
		{
			translationtables[i] = 114+(i-225); // yellow
			translationtables[i+256] = 145+(i-225); // red
			translationtables[i+512] = 190+(i-225); // blue
		}
		else
		{
			translationtables[i] = translationtables[i+256] 
			= translationtables[i+512] = i;
		}
	}
}

/*
================
=
= R_DrawSpan
=
================
*/

int				ds_y;
int				ds_x1;
int				ds_x2;
lighttable_t	*ds_colormap;
fixed_t			ds_xfrac;
fixed_t			ds_yfrac;
fixed_t			ds_xstep;
fixed_t			ds_ystep;
byte			*ds_source;		// start of a 64*64 tile image

int				dscount;		// just for profiling

void R_DrawSpan (void)
{
	fixed_t		xfrac, yfrac;
	byte		*dest;
	int			count;
	
#ifdef RANGECHECK
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2>=sysvideo.width 
	|| (unsigned)ds_y>sysvideo.height)
		I_Error ("R_DrawSpan: %i to %i at %i",ds_x1,ds_x2,ds_y);
//	dscount++;
#endif
	
	xfrac = ds_xfrac;
	yfrac = ds_yfrac;
	
	dest = ylookup[ds_y] + columnofs[ds_x1];	
	count = ds_x2 - ds_x1;

#if defined(__GNUC__) && (defined(__M68000__) || defined(__M68020__))
	{
		long uv, uvstep;

		uv = (ds_yfrac >> 6) & 0xffffUL;
		uv |= (ds_xfrac<<10) & 0xffff0000UL;

		uvstep = (ds_ystep>>6) & 0xffffUL;
		uvstep |= (ds_xstep<<10) & 0xffff0000UL;

    __asm__ __volatile__ (
	"moveql	#0,d1\n"
"	moveql	#10,d2\n"
"	moveql	#6,d3\n"
"	movel	%5,d0\n"

"	movew	%0,d4\n"
"	notw	d4\n"
"	andw	#3,d4\n"
"	lea		R_DrawSpan_loop,a0\n"
"	muluw	#R_DrawSpan_loop1-R_DrawSpan_loop,d4\n"
"	lsrw	#2,%0\n"
"	jmp		a0@(0,d4:w)\n"

"R_DrawSpan_loop:\n"
"	lsrw	d2,d0\n"
"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"

"R_DrawSpan_loop1:\n"
"	lsrw	d2,d0\n"
"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"

"	lsrw	d2,d0\n"
"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"

"	lsrw	d2,d0\n"
"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"

"	dbra	%0,R_DrawSpan_loop"
	 	: /* no return value */
	 	: /* input */
	 		"d"(count), "a"(ds_source), "d"(uvstep), "a"(ds_colormap),
			"a"(dest), "d"(uv)
	 	: /* clobbered registers */
	 		"d0", "d1", "d2", "d3", "d4", "a0", "cc", "memory" 
	);
	}
#else
# define RENDER_PIXEL	\
	{	\
	    int spot;	\
		\
		/* Current texture index in u,v. */	\
		spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63); \
		\
		/* Lookup pixel from flat texture tile, */ \
		/*  re-index using light/colormap. */ \
		*dest++ = ds_colormap[ds_source[spot]];	\
		\
		/* Next step in u,v. */ \
		xfrac += ds_xstep;	\
		yfrac += ds_ystep;	\
	}

	{
		fixed_t xfrac = ds_xfrac, yfrac = ds_yfrac;

		int n = count>>2;
		switch (count & 3) {
			case 3: do {
					RENDER_PIXEL;
			case 2:		RENDER_PIXEL;
			case 1:		RENDER_PIXEL;
			case 0:		RENDER_PIXEL;
				} while (--n>=0);
		}
	}
#undef RENDER_PIXEL
#endif
}

void R_DrawSpanFlat (void) 
{ 
    byte*		dest; 
	 
#ifdef RANGECHECK 
    if (ds_x2 < ds_x1
	|| ds_x1<0
	|| ds_x2>=sysvideo.width  
	|| (unsigned)ds_y>sysvideo.height)
    {
	I_Error( "R_DrawSpanFlat: %i to %i at %i",
		 ds_x1,ds_x2,ds_y);
    }
#endif 
  
	dest = ylookup[ds_y] + columnofs[ds_x1];

	memset(dest, ds_colormap[*ds_source], ds_x2 - ds_x1 +1);
} 

void R_DrawSpanLow (void)
{
	fixed_t		xfrac, yfrac;
	byte		*dest;
	int			count;
	
#ifdef RANGECHECK
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2>=sysvideo.width 
	|| (unsigned)ds_y>sysvideo.height)
		I_Error ("R_DrawSpan: %i to %i at %i",ds_x1,ds_x2,ds_y);
//	dscount++;
#endif
	
	xfrac = ds_xfrac;
	yfrac = ds_yfrac;
	
	dest = ylookup[ds_y] + columnofs[ds_x1<<1];
	count = ds_x2 - ds_x1;

#if defined(__GNUC__) && (defined(__M68000__) || defined(__M68020__))
	{
		long uv, uvstep;

		uv = (ds_yfrac >> 6) & 0xffffUL;
		uv |= (ds_xfrac<<10) & 0xffff0000UL;

		uvstep = (ds_ystep>>6) & 0xffffUL;
		uvstep |= (ds_xstep<<10) & 0xffff0000UL;

    __asm__ __volatile__ (
	"moveql	#0,d1\n"
"	moveql	#10,d2\n"
"	moveql	#6,d3\n"
"	movel	%5,d0\n"
"	lsrw	d2,d0\n"

"	movew	%0,d4\n"
"	notw	d4\n"
"	andw	#3,d4\n"
"	lea		R_DrawSpanLow_loop,a0\n"
"	muluw	#R_DrawSpanLow_loop1-R_DrawSpanLow_loop,d4\n"
"	lsrw	#2,%0\n"
"	jmp		a0@(0,d4:w)\n"

"R_DrawSpanLow_loop:\n"
"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"
"	lsrw	d2,d0\n"
"	moveb	d1,%4@+\n"

"R_DrawSpanLow_loop1:\n"
"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"
"	lsrw	d2,d0\n"
"	moveb	d1,%4@+\n"

"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"
"	lsrw	d2,d0\n"
"	moveb	d1,%4@+\n"

"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"
"	lsrw	d2,d0\n"
"	moveb	d1,%4@+\n"

"	dbra	%0,R_DrawSpanLow_loop"
	 	: /* no return value */
	 	: /* input */
	 		"d"(count), "a"(ds_source), "d"(uvstep), "a"(ds_colormap),
			"a"(dest), "d"(uv)
	 	: /* clobbered registers */
	 		"d0", "d1", "d2", "d3", "d4", "a0", "cc", "memory" 
	);
	}
#else
# define RENDER_PIXEL	\
	{	\
	    int spot;	\
		\
		/* Current texture index in u,v. */	\
		spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63); \
		spot = ds_colormap[ds_source[spot]];	\
		\
		/* Lookup pixel from flat texture tile, */ \
		/*  re-index using light/colormap. */ \
		*(unsigned short *)dest = spot|(spot<<8);	\
		dest += 2;	\
		\
		/* Next step in u,v. */ \
		xfrac += ds_xstep;	\
		yfrac += ds_ystep;	\
	}

	{
		fixed_t		xfrac = ds_xfrac, yfrac = ds_yfrac;
		int n = count>>2;
		switch (count & 3) {
			case 3: do {
					RENDER_PIXEL;
			case 2:		RENDER_PIXEL;
			case 1:		RENDER_PIXEL;
			case 0:		RENDER_PIXEL;
				} while (--n>=0);
		}
	}
#undef RENDER_PIXEL
#endif
}

void R_DrawSpanLowFlat (void) 
{ 
    byte*		dest; 
	 
#ifdef RANGECHECK 
    if (ds_x2 < ds_x1
	|| ds_x1<0
	|| ds_x2>=sysvideo.width  
	|| (unsigned)ds_y>sysvideo.height)
    {
	I_Error( "R_DrawSpan: %i to %i at %i",
		 ds_x1,ds_x2,ds_y);
    }
#endif 
	 
	dest = ylookup[ds_y] + columnofs[ds_x1<<1];
  
	memset(dest, ds_colormap[*ds_source], (ds_x2 - ds_x1 + 1)<<1);
}


/*
================
=
= R_InitBuffer
=
=================
*/

void R_InitBuffer (int width, int height)
{
	int		i;

	if (ylookup)
		Z_Free(ylookup);
	ylookup = Z_Malloc(sizeof(byte *)*sysvideo.height, PU_STATIC, NULL);
	if (columnofs)
		Z_Free(columnofs);
	columnofs = Z_Malloc(sizeof(int)*sysvideo.width, PU_STATIC, NULL);

	viewwindowx = (sysvideo.width-width) >> 1;
	for (i=0 ; i<width ; i++)
		columnofs[i] = viewwindowx + i;
	if (width == sysvideo.width)
		viewwindowy = 0;
	else
		viewwindowy = (sysvideo.height-((SBARHEIGHT*sysvideo.height)/SCREENHEIGHT)-height) >> 1;
	for (i=0 ; i<height ; i++)
		ylookup[i] = screen + (i+viewwindowy)*sysvideo.pitch;
}


/*
==================
=
= R_DrawViewBorder
=
= Draws the border around the view for different size windows
==================
*/

boolean BorderNeedRefresh;

void R_DrawViewBorder (void)
{
	byte	*src, *dest;
	int		x,y;
	
	if (scaledviewwidth == sysvideo.width)
		return;

	if(shareware)
	{
		src = W_CacheLumpName ("FLOOR04", PU_CACHE);
	}
	else
	{
		src = W_CacheLumpName ("FLAT513", PU_CACHE);
	}
	dest = screen;
	
	for (y=0 ; y<sysvideo.height-((SBARHEIGHT*sysvideo.height)/SCREENHEIGHT) ; y++)
	{
		byte *dest_line = dest;

		for (x=0 ; x<sysvideo.width/64 ; x++)
		{
			memcpy (dest_line, src+((y&63)<<6), 64);
			dest_line += 64;
		}
		if (sysvideo.width&63)
		{
			memcpy (dest_line, src+((y&63)<<6), sysvideo.width&63);
		}

		dest += sysvideo.pitch;
	}

	for(x=viewwindowx; x < viewwindowx+(viewwidth<<detailshift); x += 16)
	{
		V_DrawPatchDirect(x, viewwindowy-4, W_CacheLumpName("bordt", PU_CACHE));
		V_DrawPatchDirect(x, viewwindowy+viewheight, W_CacheLumpName("bordb", 
			PU_CACHE));
	}
	for(y=viewwindowy; y < viewwindowy+viewheight; y += 16)
	{
		V_DrawPatchDirect(viewwindowx-4, y, W_CacheLumpName("bordl", PU_CACHE));
		V_DrawPatchDirect(viewwindowx+(viewwidth<<detailshift), y, W_CacheLumpName("bordr", 
			PU_CACHE));
	}
	V_DrawPatchDirect(viewwindowx-4, viewwindowy-4, W_CacheLumpName("bordtl", 
		PU_CACHE));
	V_DrawPatchDirect(viewwindowx+(viewwidth<<detailshift), viewwindowy-4, 
		W_CacheLumpName("bordtr", PU_CACHE));
	V_DrawPatchDirect(viewwindowx+(viewwidth<<detailshift), viewwindowy+viewheight, 
		W_CacheLumpName("bordbr", PU_CACHE));
	V_DrawPatchDirect(viewwindowx-4, viewwindowy+viewheight, 
		W_CacheLumpName("bordbl", PU_CACHE));
}

/*
==================
=
= R_DrawTopBorder
=
= Draws the top border around the view for different size windows
==================
*/

boolean BorderTopRefresh;

void R_DrawTopBorder (void)
{
	byte	*src, *dest;
	int		x,y;
	
	if (scaledviewwidth == sysvideo.width)
		return;

	if(shareware)
	{
		src = W_CacheLumpName ("FLOOR04", PU_CACHE);
	}
	else
	{
		src = W_CacheLumpName ("FLAT513", PU_CACHE);
	}
	dest = screen;
	
	for (y=0 ; y<30 ; y++)
	{
		byte *dest_line = dest;
	
		for (x=0 ; x<sysvideo.width/64 ; x++)
		{
			memcpy (dest_line, src+((y&63)<<6), 64);
			dest_line += 64;
		}
		if (sysvideo.width&63)
		{
			memcpy (dest_line, src+((y&63)<<6), sysvideo.width&63);
		}
		dest += sysvideo.pitch;
	}

	if(viewwindowy < 25)
	{
		for(x=viewwindowx; x < viewwindowx+viewwidth; x += 16)
		{
			V_DrawPatchDirect(x, viewwindowy-4, W_CacheLumpName("bordt", PU_CACHE));
		}
		V_DrawPatchDirect(viewwindowx-4, viewwindowy, W_CacheLumpName("bordl", 
			PU_CACHE));
		V_DrawPatchDirect(viewwindowx+viewwidth, viewwindowy, 
			W_CacheLumpName("bordr", PU_CACHE));
		V_DrawPatchDirect(viewwindowx-4, viewwindowy+16, W_CacheLumpName("bordl", 
			PU_CACHE));
		V_DrawPatchDirect(viewwindowx+viewwidth, viewwindowy+16, 
			W_CacheLumpName("bordr", PU_CACHE));

		V_DrawPatchDirect(viewwindowx-4, viewwindowy-4, W_CacheLumpName("bordtl", 
			PU_CACHE));
		V_DrawPatchDirect(viewwindowx+viewwidth, viewwindowy-4, 
			W_CacheLumpName("bordtr", PU_CACHE));
	}
}
