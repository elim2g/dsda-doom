//
// Copyright(C) 2021 by Ryan Krafnick
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	DSDA Zone Memory Management
//

#ifndef __DSDA_ZONE__
#define __DSDA_ZONE__

void *dsda_MallocThinker(size_t size);
void dsda_FreeThinker(void* th);
void dsda_FreeThinkers(void);

#endif
