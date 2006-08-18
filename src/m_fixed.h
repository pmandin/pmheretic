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
//	Fixed point arithemtics, implementation.
//
//-----------------------------------------------------------------------------

#ifndef __M_FIXED__
#define __M_FIXED__

//
// Fixed point, 32bit as 16.16.
//
#define FRACBITS		16
#define FRACUNIT		(1<<FRACBITS)

typedef int fixed_t;

extern fixed_t (*FixedMul) (fixed_t a, fixed_t b);
extern fixed_t (*FixedDiv2) (fixed_t a, fixed_t b);

fixed_t FixedMulSoft (fixed_t a, fixed_t b);
fixed_t FixedDiv (fixed_t a, fixed_t b);
fixed_t FixedDiv2Soft (fixed_t a, fixed_t b);

#if defined(__GNUC__) && (defined(__M68000__) || defined(__M68020__))

#define m68k_InitFpu()	\
({	\
    __asm__ __volatile__ (	\
		"fmove%.l	fpcr,d0\n"	\
	"	andl	#~0x30,d0\n"	\
	"	orb		#0x20,d0\n"	\
	"	fmove%.l	d0,fpcr"	\
		: /* no return value */	\
		: /* no input */	\
		: /* clobbered registers */	\
			"d0", "cc"	\
	);	\
});

fixed_t FixedMul020 (fixed_t a, fixed_t b);
fixed_t FixedDiv2020 (fixed_t a, fixed_t b);

fixed_t FixedMul060 (fixed_t a, fixed_t b);
fixed_t FixedDiv2060 (fixed_t a, fixed_t b);
#endif

#endif
