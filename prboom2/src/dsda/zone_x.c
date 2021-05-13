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

#include <stdlib.h>

#include "lprintf.h"
#include "z_zone.h"

#include "zone_x.h"

extern memblock_t *blockbytag[PU_MAX];

typedef struct thinker_memblock {
  struct thinker_memblock *prev;
  struct thinker_memblock *next;
} thinker_memblock_t;

static thinker_memblock_t *thinker_block_start;

#define THINKER_HEADER_SIZE sizeof(thinker_memblock_t)

void *dsda_MallocThinker(size_t size) {
  thinker_memblock_t *block = NULL;

  if (!size)
    return NULL;

  while (!(block = (malloc)(size + THINKER_HEADER_SIZE))) {
    if (!blockbytag[PU_CACHE])
      I_Error ("dsda_MallocThinker: Failure trying to allocate %lu bytes", (unsigned long) size);
    Z_FreeTag(PU_CACHE);
  }

  if (!thinker_block_start)
  {
    thinker_block_start = block;
    block->next = block->prev = block;
  }
  else
  {
    thinker_block_start->prev->next = block;
    block->prev = thinker_block_start->prev;
    block->next = thinker_block_start;
    thinker_block_start->prev = block;
  }

  return (thinker_memblock_t *)((char *) block + THINKER_HEADER_SIZE);
}

void dsda_FreeThinker(void *p)
{
  thinker_memblock_t *block;

  if (!p)
    return;

  block = (thinker_memblock_t *)((char *) p - THINKER_HEADER_SIZE);

  if (block == block->next)
    thinker_block_start = NULL;
  else
    if (thinker_block_start == block)
      thinker_block_start = block->next;
  block->prev->next = block->next;
  block->next->prev = block->prev;

  (free)(block);
}

void dsda_FreeThinkers(void) {
  thinker_memblock_t *block, *end_block;

  block = thinker_block_start;
  if (!block)
    return;
  end_block = block->prev;
  while (1) {
    thinker_memblock_t *next = block->next;
    dsda_FreeThinker((char *) block + THINKER_HEADER_SIZE);
    if (block == end_block)
      break;
    block = next;
  }
}
