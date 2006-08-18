// W_wad.c

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include "doomdef.h"

#ifndef O_BINARY
#define O_BINARY	0
#endif

//===============
//   TYPES
//===============


typedef struct
{
	char		identification[4];		// should be IWAD
	int			numlumps;
	int			infotableofs;
} wadinfo_t;


typedef struct
{
	int			filepos;
	int			size;
	char		name[8];
} filelump_t;


//=============
// GLOBALS
//=============

#define DEFAULT_LUMPINFO_SIZE 16

lumpinfo_t	*lumpinfo=NULL;		// location of each lump on disk
int			numlumps;
static unsigned long lumpinfo_size=DEFAULT_LUMPINFO_SIZE;

void		**lumpcache;

//===================

#ifndef HAVE_STRUPR
void strupr (char *s)
{
	while (*s) {
		*s = toupper(*s);
		s++;
	}
}
#endif

/*
================
=
= filelength
=
================
*/

int filelength (int handle)
{
    struct stat	fileinfo;
    
    if (fstat (handle,&fileinfo) == -1)
	I_Error ("Error fstating");

    return fileinfo.st_size;
}


void ExtractFileBase (char *path, char *dest)
{
	char	*src;
	int		length;

	src = path + strlen(path) - 1;

//
// back up until a \ or the start
//
	while (src != path && *(src-1) != '\\' && *(src-1) != '/')
		src--;

//
// copy up to eight characters
//
	memset (dest,0,8);
	length = 0;
	while (*src && *src != '.')
	{
		if (++length == 9)
			I_Error ("Filename base of %s >8 chars",path);
		*dest++ = toupper((int)*src++);
	}
}

/*
============================================================================

						LUMP BASED ROUTINES

============================================================================
*/

/*
====================
=
= W_AddFile
=
= All files are optional, but at least one file must be found
= Files with a .wad extension are wadlink files with multiple lumps
= Other files are single lumps with the base filename for the lump name
=
====================
*/

void W_AddFile (char *filename)
{
	wadinfo_t		header;
	lumpinfo_t		*lump_p, *oldlump;
	unsigned		i;
	int				handle, length;
	int				startlump;
	filelump_t		*fileinfo, singleinfo, *f_info;
	int newsize;
	boolean islump;
	
//
// open the file and add to directory
//	
	if ( (handle = open (filename,O_RDONLY | O_BINARY)) == -1)
		return;

	startlump = numlumps;
	
	islump=false;
	if (strcasecmp (filename+strlen(filename)-3 , "wad" ) )
	{
	// single lump file
		fileinfo = &singleinfo;
		singleinfo.filepos = 0;
		singleinfo.size = LONG(filelength(handle));
		ExtractFileBase (filename, singleinfo.name);
		numlumps++;
		islump = true;
	}
	else 
	{
	// WAD file
		read (handle, &header, sizeof(header));
		if (strncmp(header.identification,"IWAD",4))
		{
			if (strncmp(header.identification,"PWAD",4))
				I_Error ("Wad file %s doesn't have IWAD or PWAD id\n"
				,filename);
		}
		header.numlumps = LONG(header.numlumps);
		header.infotableofs = LONG(header.infotableofs);
		length = header.numlumps*sizeof(filelump_t);
		fileinfo = Z_Malloc(length, PU_STATIC, NULL);
		lseek (handle, header.infotableofs, SEEK_SET);
		read (handle, fileinfo, length);
		numlumps += header.numlumps;
	}

//
// Fill in lumpinfo
//
	oldlump = lumpinfo;
	newsize = numlumps*sizeof(lumpinfo_t);
	lumpinfo = Z_Malloc(newsize, PU_STATIC, NULL);
	memcpy((void *)lumpinfo, (void *)oldlump, lumpinfo_size);
	Z_Free(oldlump);
	lumpinfo_size = newsize;

	if (!lumpinfo)
		I_Error ("Couldn't realloc lumpinfo");
	lump_p = &lumpinfo[startlump];
	
	f_info = fileinfo;
	for (i=startlump ; i<numlumps ; i++,lump_p++, f_info++)
	{
		lump_p->handle = handle;
		lump_p->position = LONG(f_info->filepos);
		lump_p->size = LONG(f_info->size);
		strncpy (lump_p->name, f_info->name, 8);
	}

	if (!islump)
		Z_Free(fileinfo);
}





/*
====================
=
= W_InitMultipleFiles
=
= Pass a null terminated list of files to use.
=
= All files are optional, but at least one file must be found
=
= Files with a .wad extension are idlink files with multiple lumps
=
= Other files are single lumps with the base filename for the lump name
=
= Lump names can appear multiple times. The name searcher looks backwards,
= so a later file can override an earlier one.
=
====================
*/

void W_InitMultipleFiles (char **filenames)
{	
	int		size;
	
//
// open all the files, load headers, and count lumps
//
	numlumps = 0;
	lumpinfo = Z_Malloc(DEFAULT_LUMPINFO_SIZE, PU_STATIC, NULL);	// will be realloced as lumps are added

	for ( ; *filenames ; filenames++)
		W_AddFile (*filenames);

	if (!numlumps)
		I_Error ("W_InitFiles: no files found");
		
//
// set up caching
//
	size = numlumps * sizeof(*lumpcache);
	lumpcache = Z_Malloc(size, PU_STATIC, NULL);
	if (!lumpcache)
		I_Error ("Couldn't allocate lumpcache");
	memset (lumpcache,0, size);
}



/*
====================
=
= W_InitFile
=
= Just initialize from a single file
=
====================
*/

void W_InitFile (char *filename)
{
	char	*names[2];

	names[0] = filename;
	names[1] = NULL;
	W_InitMultipleFiles (names);
}



/*
====================
=
= W_NumLumps
=
====================
*/

int	W_NumLumps (void)
{
	return numlumps;
}



/*
====================
=
= W_CheckNumForName
=
= Returns -1 if name not found
=
====================
*/

int	W_CheckNumForName (char *name)
{
	union {
		char	s[9];
		int	x[2];
	} name8;
	int		v1,v2;
	lumpinfo_t	*lump_p;

	// make the name into two integers for easy compares
	strncpy (name8.s,name,8);

	// in case the name was a fill 8 chars
	name8.s[8] = 0;

	// case insensitive
	strupr (name8.s);		

	v1 = name8.x[0];
	v2 = name8.x[1];

// scan backwards so patch lump files take precedence

	lump_p = lumpinfo + numlumps;

	while (lump_p-- != lumpinfo) {
		if ( *(int *)lump_p->name == v1 && *(int *)&lump_p->name[4] == v2)
			return lump_p - lumpinfo;
	}


	return -1;
}


/*
====================
=
= W_GetNumForName
=
= Calls W_CheckNumForName, but bombs out if not found
=
====================
*/

int	W_GetNumForName (char *name)
{
	int	i;

	i = W_CheckNumForName (name);
	if (i != -1)
		return i;

	I_Error ("W_GetNumForName: %s not found!",name);
	return -1;
}


/*
====================
=
= W_LumpLength
=
= Returns the buffer size needed to load the given lump
=
====================
*/

int W_LumpLength (int lump)
{
	if (lump >= numlumps)
		I_Error ("W_LumpLength: %i >= numlumps",lump);
	return lumpinfo[lump].size;
}


/*
====================
=
= W_ReadLump
=
= Loads the lump into the given buffer, which must be >= W_LumpLength()
=
====================
*/

void W_ReadLump (int lump, void *dest)
{
	int			c;
	lumpinfo_t	*l;
	
	if (lump >= numlumps)
		I_Error ("W_ReadLump: %i >= numlumps",lump);
	l = lumpinfo+lump;
	
	lseek (l->handle, l->position, SEEK_SET);
	c = read (l->handle, dest, l->size);
	if (c < l->size)
		I_Error ("W_ReadLump: only read %i of %i on lump %i",c,l->size,lump);	
}



/*
====================
=
= W_CacheLumpNum
=
====================
*/

void	*W_CacheLumpNum (int lump, int tag)
{
byte *ptr;

	if ((unsigned)lump >= numlumps)
		I_Error ("W_CacheLumpNum: %i >= numlumps",lump);
		
	if (!lumpcache[lump])
	{	// read the lump in
//printf ("cache miss on lump %i\n",lump);
		ptr = Z_Malloc (W_LumpLength (lump), tag, &lumpcache[lump]);
		W_ReadLump (lump, lumpcache[lump]);
	}
	else
	{
//printf ("cache hit on lump %i\n",lump);
		Z_ChangeTag (lumpcache[lump],tag);
	}
	
	return lumpcache[lump];
}


/*
====================
=
= W_CacheLumpName
=
====================
*/

void	*W_CacheLumpName (char *name, int tag)
{
	return W_CacheLumpNum (W_GetNumForName(name), tag);
}
