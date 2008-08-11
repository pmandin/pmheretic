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
//	Fixed point implementation.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>

#include "doomdef.h"

// Fixme. __USE_C_FIXED__ or something.

fixed_t (*FixedMul) (fixed_t a, fixed_t b) = FixedMulSoft;
fixed_t (*FixedDiv2) (fixed_t a, fixed_t b) = FixedDiv2Soft;

fixed_t
FixedMulSoft
( fixed_t	a,
  fixed_t	b )
{
    return ((long long) a * (long long) b) >> FRACBITS;
}



//
// FixedDiv, C version.
//

fixed_t
FixedDiv
( fixed_t	a,
  fixed_t	b )
{
    if ( (abs(a)>>14) >= abs(b))
	return (a^b)<0 ? MININT : MAXINT;
    return FixedDiv2 (a,b);
}



fixed_t
FixedDiv2Soft
( fixed_t	a,
  fixed_t	b )
{
#if 0
    long long c;
    c = ((long long)a<<16) / ((long long)b);
    return (fixed_t) c;
#endif

    double c;

    c = ((double)a) / ((double)b) * FRACUNIT;

    if (c >= 2147483648.0 || c < -2147483648.0)
	I_Error("FixedDiv: divide by zero");
    return (fixed_t) c;
}

#if defined(__GNUC__) && (defined(__M68000__) || defined(__M68020__))

#define asm_mul_int(a,b) \
({	\
	register long retvalue __asm__("d0");	\
	\
    __asm__ __volatile__ (	\
		"mulsl	%2,%2:%1\n"	\
	"	movew	%2,%1\n"	\
	"	swap	%0"	\
	 	: /* return value */	\
			"=r" (retvalue)	\
	 	: /* input */	\
	 		"0"(a), "d"(b)	\
	 	: /* clobbered registers */	\
	 		"cc"	\
	);	\
	\
	retvalue;	\
});

fixed_t FixedMul020 (fixed_t a, fixed_t b)
{
	return asm_mul_int(a,b);
}

#define asm_div_int(a,b) \
({	\
	register long retvalue __asm__("d0");	\
	\
    __asm__ __volatile__ (	\
		"movel	%1,d2\n"	\
	"	swap	%1\n"	\
	"	movew	%1,d1\n"	\
	"	extl	d1\n"	\
	"	clrw	%1\n"	\
	"	divsl	%2,d1:%1"	\
	 	: /* return value */	\
			"=r" (retvalue)	\
	 	: /* input */	\
	 		"0"(a), "g"(b)	\
	 	: /* clobbered registers */	\
	 		"d1", "d2", "cc"	\
	);	\
	\
	retvalue;	\
});

fixed_t FixedDiv2020 (fixed_t a, fixed_t b)
{
	return asm_div_int(a,b);
}

#define asm_mul_float(a,b) \
({	\
	register long retvalue __asm__("d0");	\
	\
    __asm__ __volatile__ (	\
		"fmove%.l	%1,fp0\n"	\
	"	fmul%.l	%2,fp0\n"	\
	"	fmul%.s	&0f0.0000152587890625,fp0\n"	\
	"	fmove%.l	fp0,%0"	\
	 	: /* return value */	\
			"=r" (retvalue)	\
	 	: /* input */	\
	 		"g"(a), "g"(b) \
	 	: /* clobbered registers */	\
	 		"fp0", "cc"	\
	);	\
	\
	retvalue;	\
});

fixed_t FixedMul060 (fixed_t a, fixed_t b)
{
	return asm_mul_float(a,b);
}

#define asm_div_float(a,b) \
({	\
	register long retvalue __asm__("d0");	\
	\
    __asm__ __volatile__ (	\
		"fmove%.l	%1,fp0\n"	\
	"	fmove%.l	%2,fp1\n"	\
	"	fdiv%.x		fp1,fp0\n"	\
	"	fmul%.s		&0f65536.0,fp0\n"	\
	"	fmove%.l	fp0,d0"	\
	 	: /* return value */	\
			"=r" (retvalue)	\
	 	: /* input */	\
	 		"g"(a), "g"(b)	\
	 	: /* clobbered registers */	\
	 		"fp0", "fp1", "cc"	\
	);	\
	\
	retvalue;	\
});

fixed_t FixedDiv2060 (fixed_t a, fixed_t b)
{
	return asm_div_float(a,b);
}

#endif
