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
//
//-----------------------------------------------------------------------------

#include "i_system.h"
#include "i_net.h"

#include "doomdef.h"

sysnetwork_t	sysnetwork={NETWORK_UNIX};

void	(*netget) (void);
void	(*netsend) (void);

void	(*I_InitNetwork) (void) = I_InitNetwork_unix;
void	(*I_ShutdownNetwork) (void) = I_ShutdownNetwork_unix;

void I_NetCmd (void)
{
	if (!netgame)
		I_Error ("I_NetCmd when not in netgame");

	if (doomcom->command == CMD_SEND) {
		netsend ();
	} else if (doomcom->command == CMD_GET) {
		netget ();
	} else
		I_Error ("Bad net cmd: %i\n",doomcom->command);
}

