/***************************************************************************************
 *  Genesis Plus
 *  Savestate support
 *
 *  Copyright (C) 2007-2011  Eke-Eke (GCN/Wii port)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ****************************************************************************************/

#ifndef _STATE_H_
#define _STATE_H_

#ifndef NO_SCD
#include <scd/scd.h>
#define STATE_SIZE    0x48100 + sizeof(SegaCD)
#else
#define STATE_SIZE    0x48100
#endif
#define STATE_VERSION "GENPLUS-GX 1.5.3"

#define load_param(param, size) \
  memcpy(param, &state[bufferptr], size); \
  bufferptr+= size;

#define save_param(param, size) \
  memcpy(&state[bufferptr], param, size); \
  bufferptr+= size;

/* Function prototypes */
void state_load(const unsigned char *buffer);
int state_save(unsigned char *buffer);

#endif
