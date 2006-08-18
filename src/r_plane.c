// R_planes.c

#include <stdlib.h>

#include "doomdef.h"
#include "r_local.h"
#include "i_video.h"

planefunction_t		floorfunc, ceilingfunc;

//
// sky mapping
//
int			skyflatnum;
int			skytexture;
int			skytexturemid;
fixed_t		skyiscale;

//
// opening
//

visplane_t		visplanes[MAXVISPLANES], *lastvisplane;
visplane_t		*floorplane, *ceilingplane;

short			*openings=NULL, *lastopening;
static unsigned short *visplanesy=NULL;

//
// clip values are the solid pixel bounding the range
// floorclip starts out SCREENHEIGHT
// ceilingclip starts out -1
//
short		*floorclip=NULL;
short		*ceilingclip=NULL;

//
// spanstart holds the start of a plane span
// initialized to 0 at start
//
int			*spanstart=NULL;
int			*spanstop=NULL;

//
// texture mapping
//
lighttable_t	**planezlight;
fixed_t		planeheight;

fixed_t		*yslope=NULL;
fixed_t		*distscale=NULL;
fixed_t		basexscale, baseyscale;

fixed_t		*cachedheight=NULL;
fixed_t		*cacheddistance=NULL;
fixed_t		*cachedxstep=NULL;
fixed_t		*cachedystep=NULL;


/*
================
=
= R_InitSkyMap
=
= Called whenever the view size changes
=
================
*/

void R_InitSkyMap (void)
{
	skyflatnum = R_FlatNumForName ("F_SKY1");
	skytexturemid = SCREENHEIGHT*FRACUNIT;
}


/*
====================
=
= R_InitPlanes
=
= Only at game startup
====================
*/
#define	ALLOCATE_ARRAY(pointer, size, type) \
	if (pointer) {	\
		Z_Free(pointer); \
	}	\
	\
	pointer = Z_Malloc(sizeof(type)*size, PU_STATIC, NULL);

void R_InitPlanes (void)
{
	ALLOCATE_ARRAY(openings, MAXOPENINGS, short);
	ALLOCATE_ARRAY(floorclip, sysvideo.width, short);
	ALLOCATE_ARRAY(ceilingclip, sysvideo.width, short);
	ALLOCATE_ARRAY(spanstart, sysvideo.height, int);
	ALLOCATE_ARRAY(spanstop, sysvideo.height, int);
	ALLOCATE_ARRAY(yslope, sysvideo.height, fixed_t);
	ALLOCATE_ARRAY(distscale, sysvideo.width, fixed_t);
	ALLOCATE_ARRAY(cachedheight, sysvideo.height, fixed_t);
	ALLOCATE_ARRAY(cacheddistance, sysvideo.height, fixed_t);
	ALLOCATE_ARRAY(cachedxstep, sysvideo.height, fixed_t);
	ALLOCATE_ARRAY(cachedystep, sysvideo.height, fixed_t);
	ALLOCATE_ARRAY(visplanesy, ((sysvideo.width+2)<<1)*MAXVISPLANES, unsigned short);

	{
		int i;
		unsigned short *array;

		array = &visplanesy[1];
		for (i=0;i<MAXVISPLANES;i++) {
			visplanes[i].top = array;
			array += sysvideo.width+2;
			visplanes[i].bottom = array;
			array += sysvideo.width+2;
		}
	}
}


/*
================
=
= R_MapPlane
=
global vars:

planeheight
ds_source
basexscale
baseyscale
viewx
viewy

BASIC PRIMITIVE
================
*/

void R_MapPlane (int y, int x1, int x2)
{
	angle_t		angle;
	fixed_t		distance, length;
	unsigned	index;
	
#ifdef RANGECHECK
	if (x2 < x1 || x1<0 || x2>=viewwidth || (unsigned)y>viewheight)
		I_Error ("R_MapPlane: %i, %i at %i",x1,x2,y);
#endif

	if (planeheight != cachedheight[y])
	{
		cachedheight[y] = planeheight;
		distance = cacheddistance[y] = FixedMul (planeheight, yslope[y]);

		ds_xstep = cachedxstep[y] = FixedMul (distance,basexscale);
		ds_ystep = cachedystep[y] = FixedMul (distance,baseyscale);
	}
	else
	{
		distance = cacheddistance[y];
		ds_xstep = cachedxstep[y];
		ds_ystep = cachedystep[y];
	}
	
	length = FixedMul (distance,distscale[x1]);
	angle = (viewangle + xtoviewangle[x1])>>ANGLETOFINESHIFT;
	ds_xfrac = viewx + FixedMul(finecosine[angle], length);
	ds_yfrac = -viewy - FixedMul(finesine[angle], length);

	if (fixedcolormap)
		ds_colormap = fixedcolormap;
	else
	{
		index = distance >> LIGHTZSHIFT;
		if (index >= MAXLIGHTZ )
			index = MAXLIGHTZ-1;
		ds_colormap = planezlight[index];
	}
	
	ds_y = y;
	ds_x1 = x1;
	ds_x2 = x2;
	
	spanfunc ();		// high or low detail
}

//=============================================================================

/*
====================
=
= R_ClearPlanes
=
= At begining of frame
====================
*/

void R_ClearPlanes (void)
{
	int		i;
	angle_t	angle;
	
//
// opening / clipping determination
//	
	for (i=0 ; i<viewwidth ; i++)
	{
		floorclip[i] = viewheight;
		ceilingclip[i] = -1;
	}

	lastvisplane = visplanes;
	lastopening = openings;
	
//
// texture calculation
//
	memset (cachedheight, 0, sizeof(fixed_t)*sysvideo.height);	
	angle = (viewangle-ANG90)>>ANGLETOFINESHIFT;	// left to right mapping
	
	// scale will be unit scale at SCREENWIDTH/2 distance
	basexscale = FixedDiv (finecosine[angle],centerxfrac);
	baseyscale = -FixedDiv (finesine[angle],centerxfrac);
}



/*
===============
=
= R_FindPlane
=
===============
*/

visplane_t *R_FindPlane(fixed_t height, int picnum,
	int lightlevel, int special)
{
	visplane_t *check;

	if(picnum == skyflatnum)
	{
		// all skies map together
		height = 0;
		lightlevel = 0;
	}

	for(check = visplanes; check < lastvisplane; check++)
	{
		if(height == check->height
		&& picnum == check->picnum
		&& lightlevel == check->lightlevel
		&& special == check->special)
			break;
	}

	if(check < lastvisplane)
	{
		return(check);
	}

	if(lastvisplane-visplanes == MAXVISPLANES)
	{
		I_Error("R_FindPlane: no more visplanes");
	}

	lastvisplane++;
	check->height = height;
	check->picnum = picnum;
	check->lightlevel = lightlevel;
	check->special = special;
	check->minx = sysvideo.width;
	check->maxx = -1;
	memset(check->top,0xff,sizeof(unsigned short)*sysvideo.width);
	memset(check->bottom,0,sizeof(unsigned short)*sysvideo.width);
	return(check);
}

/*
===============
=
= R_CheckPlane
=
===============
*/

visplane_t *R_CheckPlane (visplane_t *pl, int start, int stop)
{
	int			intrl, intrh;
	int			unionl, unionh;
	int			x;
	
	if (start < pl->minx)
	{
		intrl = pl->minx;
		unionl = start;
	}
	else
	{
		unionl = pl->minx;
		intrl = start;
	}
	
	if (stop > pl->maxx)
	{
		intrh = pl->maxx;
		unionh = stop;
	}
	else
	{
		unionh = pl->maxx;
		intrh = stop;
	}

	for (x=intrl ; x<= intrh ; x++)
		if (pl->top[x] != 0xffff)
			break;

	if (x > intrh)
	{
		pl->minx = unionl;
		pl->maxx = unionh;
		return pl;			// use the same one
	}
	
// make a new visplane

	lastvisplane->height = pl->height;
	lastvisplane->picnum = pl->picnum;
	lastvisplane->lightlevel = pl->lightlevel;
	lastvisplane->special = pl->special;
	pl = lastvisplane++;
	pl->minx = start;
	pl->maxx = stop;
	memset (pl->top,0xff,sizeof(unsigned short)*sysvideo.width);
	memset (pl->bottom,0,sizeof(unsigned short)*sysvideo.width);
		
	return pl;
}



//=============================================================================

/*
================
=
= R_MakeSpans
=
================
*/

void R_MakeSpans (int x, int t1, int b1, int t2, int b2)
{
	while (t1 < t2 && t1<=b1)
	{
		R_MapPlane (t1,spanstart[t1],x-1);
		t1++;
	}
	while (b1 > b2 && b1>=t1)
	{
		R_MapPlane (b1,spanstart[b1],x-1);
		b1--;
	}
	
	while (t2 < t1 && t2<=b2)
	{
		spanstart[t2] = x;
		t2++;
	}
	while (b2 > b1 && b2>=t2)
	{
		spanstart[b2] = x;
		b2--;
	}
}



/*
================
=
= R_DrawPlanes
=
= At the end of each frame
================
*/
				
extern byte **ylookup;
extern int *columnofs;

void R_DrawPlanes (void)
{
	visplane_t	*pl;
	int			light;
	int			x, stop;
	int			angle;
	byte *tempSource;
	
	byte *dest;
	int count;
	fixed_t frac, fracstep;

#ifdef RANGECHECK
	if (ds_p - drawsegs > MAXDRAWSEGS)
		I_Error ("R_DrawPlanes: drawsegs overflow (%i)", ds_p - drawsegs);
	if (lastvisplane - visplanes > MAXVISPLANES)
		I_Error ("R_DrawPlanes: visplane overflow (%i)", lastvisplane - visplanes);
	if (lastopening - openings > MAXOPENINGS)
		I_Error ("R_DrawPlanes: opening overflow (%i)", lastopening - openings);
#endif

	for (pl = visplanes ; pl < lastvisplane ; pl++)
	{
		if (pl->minx > pl->maxx)
			continue;
	//
	// sky flat
	//
		if (pl->picnum == skyflatnum)
		{
			skyiscale = (FRACUNIT*SCREENHEIGHT)/sysvideo.height;

			dc_iscale = skyiscale;
			dc_colormap = colormaps;// sky is allways drawn full bright
			dc_texturemid = skytexturemid;
			for (x=pl->minx ; x <= pl->maxx ; x++)
			{
				dc_yl = pl->top[x];
				dc_yh = pl->bottom[x];
				if (dc_yl <= dc_yh)
				{
					angle = (viewangle + xtoviewangle[x])>>ANGLETOSKYSHIFT;
					dc_x = x;
					dc_source = R_GetColumn(skytexture, angle);

					count = dc_yh - dc_yl;
					if (count < 0)
						return;
				
#ifdef RANGECHECK
	if ((unsigned)dc_x >= sysvideo.width || dc_yl < 0 || dc_yh >= sysvideo.height)
		I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

					dest = ylookup[dc_yl] + columnofs[dc_x<<detailshift]; 
	
					fracstep = skyiscale;
					frac = dc_texturemid + (dc_yl-centery)*fracstep;		
					if (detailshift) {
						do
						{
							int color;
							
							color = dc_source[frac>>16];
							*dest++ = color;
							*dest++ = color;
							dest += sysvideo.pitch-2;
							frac += fracstep;
						} while (count--);
					} else {
						do
						{
							*dest = dc_source[frac>>16];
							dest += sysvideo.pitch;
							frac += fracstep;
						} while (count--);
					}
				}
			}
			continue;
		}
		
	//
	// regular flat
	//
		tempSource = W_CacheLumpNum(firstflat +
			flattranslation[pl->picnum], PU_STATIC);

		switch(pl->special)
		{
			case 25: case 26: case 27: case 28: case 29: // Scroll_North
				ds_source = tempSource;
				break;
			case 20: case 21: case 22: case 23: case 24: // Scroll_East
				ds_source = tempSource+((63-((leveltime>>1)&63))<<
					(pl->special-20)&63);
				//ds_source = tempSource+((leveltime>>1)&63);
				break;
			case 30: case 31: case 32: case 33: case 34: // Scroll_South
				ds_source = tempSource;
				break;
			case 35: case 36: case 37: case 38: case 39: // Scroll_West
				ds_source = tempSource;
				break;
			case 4: // Scroll_EastLavaDamage
				ds_source = tempSource+(((63-((leveltime>>1)&63))<<3)&63);
				break;
			default:
				ds_source = tempSource;
		}
		planeheight = abs(pl->height-viewz);
		light = (pl->lightlevel >> LIGHTSEGSHIFT)+extralight;
		if (light >= LIGHTLEVELS)
			light = LIGHTLEVELS-1;
		if (light < 0)
			light = 0;
		planezlight = zlight[light];

		pl->top[pl->maxx+1] = 0xffff;
		pl->bottom[pl->maxx+1] = 0;
		pl->top[pl->minx-1] = 0xffff;
		pl->bottom[pl->minx-1] = 0;
		
		stop = pl->maxx + 1;
		for (x=pl->minx ; x<= stop ; x++)
			R_MakeSpans (x,pl->top[x-1],pl->bottom[x-1]
			,pl->top[x],pl->bottom[x]);
		
		Z_ChangeTag (tempSource, PU_CACHE);
	}
}
