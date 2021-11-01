/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *   Ceiling aninmation (lowering, crushing, raising)
 *
 *-----------------------------------------------------------------------------*/

#include "doomstat.h"
#include "r_main.h"
#include "p_spec.h"
#include "p_tick.h"
#include "s_sound.h"
#include "sounds.h"
#include "e6y.h"//e6y

#include "hexen/p_acs.h"
#include "hexen/sn_sonix.h"

#include "dsda/map_format.h"

// the list of ceilings moving currently, including crushers
ceilinglist_t *activeceilings;

/////////////////////////////////////////////////////////////////
//
// Ceiling action routine and linedef type handler
//
/////////////////////////////////////////////////////////////////

//
// T_MoveCeiling
//
// Action routine that moves ceilings. Called once per tick.
//
// Passed a ceiling_t structure that contains all the info about the move.
// see P_SPEC.H for fields. No return.
//
// jff 02/08/98 all cases with labels beginning with gen added to support
// generalized line type behaviors.
//

static void Hexen_T_MoveCeiling(ceiling_t* ceiling);

void T_MoveCeiling (ceiling_t* ceiling)
{
  result_e  res;

  if (map_format.hexen) return Hexen_T_MoveCeiling(ceiling);

  switch(ceiling->direction)
  {
    case 0:
      // If ceiling in stasis, do nothing
      break;

    case 1:
      // Ceiling is moving up
      res = T_MovePlane
            (
              ceiling->sector,
              ceiling->speed,
              ceiling->topheight,
              false,
              1,
              ceiling->direction,
              false
            );

      // if not a silent crusher, make moving sound
      if (!(leveltime&7))
      {
        switch(ceiling->type)
        {
          case silentCrushAndRaise:
          case genSilentCrusher:
            break;
          default:
            S_StartSound((mobj_t *)&ceiling->sector->soundorg, g_sfx_stnmov);
            break;
        }
      }

      // handle reaching destination height
      if (res == pastdest)
      {
        switch(ceiling->type)
        {
          // plain movers are just removed
          case raiseToHighest:
          case genCeiling:
            P_RemoveActiveCeiling(ceiling);
            break;

          // movers with texture change, change the texture then get removed
          case genCeilingChgT:
          case genCeilingChg0:
            P_TransferSpecial(ceiling->sector, &ceiling->newspecial);
            // fallthrough
          case genCeilingChg:
            ceiling->sector->ceilingpic = ceiling->texture;
            P_RemoveActiveCeiling(ceiling);
            break;

          // crushers reverse direction at the top
          case silentCrushAndRaise:
            S_StartSound((mobj_t *)&ceiling->sector->soundorg,sfx_pstop);
            // fallthrough
          case genSilentCrusher:
          case genCrusher:
          case fastCrushAndRaise:
          case crushAndRaise:
            ceiling->direction = -1;
            break;

          default:
            break;
        }
      }
      break;

    case -1:
      // Ceiling moving down
      res = T_MovePlane
            (
              ceiling->sector,
              ceiling->speed,
              ceiling->bottomheight,
              ceiling->crush,
              1,
              ceiling->direction,
              false
            );

      // if not silent crusher type make moving sound
      if (!(leveltime&7))
      {
        switch(ceiling->type)
        {
          case silentCrushAndRaise:
          case genSilentCrusher:
            break;
          default:
            S_StartSound((mobj_t *)&ceiling->sector->soundorg, g_sfx_stnmov);
        }
      }

      // handle reaching destination height
      if (res == pastdest)
      {
        switch(ceiling->type)
        {
          // 02/09/98 jff change slow crushers' speed back to normal
          // start back up
          case genSilentCrusher:
          case genCrusher:
            if (ceiling->oldspeed<CEILSPEED*3)
              ceiling->speed = ceiling->oldspeed;
            ceiling->direction = 1; //jff 2/22/98 make it go back up!
            break;

          // make platform stop at bottom of all crusher strokes
          // except generalized ones, reset speed, start back up
          case silentCrushAndRaise:
            S_StartSound((mobj_t *)&ceiling->sector->soundorg,sfx_pstop);
            // fallthrough
          case crushAndRaise:
            ceiling->speed = CEILSPEED;
            // fallthrough
          case fastCrushAndRaise:
            ceiling->direction = 1;
            break;

          // in the case of ceiling mover/changer, change the texture
          // then remove the active ceiling
          case genCeilingChgT:
          case genCeilingChg0:
            P_TransferSpecial(ceiling->sector, &ceiling->newspecial);
            // fallthrough
          case genCeilingChg:
            ceiling->sector->ceilingpic = ceiling->texture;
            P_RemoveActiveCeiling(ceiling);
            break;

          // all other case, just remove the active ceiling
          case lowerAndCrush:
          case lowerToFloor:
          case lowerToLowest:
          case lowerToMaxFloor:
          case genCeiling:
            P_RemoveActiveCeiling(ceiling);
            break;

          default:
            break;
        }
      }
      else // ( res != pastdest )
      {
        // handle the crusher encountering an obstacle
        if (res == crushed)
        {
          switch(ceiling->type)
          {
            //jff 02/08/98 slow down slow crushers on obstacle
            case genCrusher:
            case genSilentCrusher:
              if (ceiling->oldspeed < CEILSPEED*3)
                ceiling->speed = CEILSPEED / 8;
              break;
            case silentCrushAndRaise:
            case crushAndRaise:
            case lowerAndCrush:
                ceiling->speed = CEILSPEED / 8;
              break;

            default:
              break;
          }
        }
      }
      break;
  }
}


//
// EV_DoCeiling
//
// Move a ceiling up/down or start a crusher
//
// Passed the linedef activating the function and the type of function desired
// returns true if a thinker started
//
int EV_DoCeiling
( line_t* line,
  ceiling_e type )
{
  int   secnum;
  int   rtn;
  sector_t* sec;
  ceiling_t*  ceiling;

  secnum = -1;
  rtn = 0;

  if (ProcessNoTagLines(line, &sec, &secnum)) {if (zerotag_manual) goto manual_ceiling; else {return rtn;}};//e6y
  // Reactivate in-stasis ceilings...for certain types.
  // This restarts a crusher after it has been stopped
  switch(type)
  {
    case fastCrushAndRaise:
    case silentCrushAndRaise:
    case crushAndRaise:
      //jff 4/5/98 return if activated
      rtn = P_ActivateInStasisCeiling(line->tag); // heretic_note: rtn not set in heretic
    default:
      break;
  }

  // affects all sectors with the same tag as the linedef
  while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
  {
    sec = &sectors[secnum];

manual_ceiling://e6y
    // if ceiling already moving, don't start a second function on it
    if (P_SectorActive(ceiling_special,sec)) { //jff 2/22/98
      if (!zerotag_manual) continue; else {return rtn;}};//e6y

    // create a new ceiling thinker
    rtn = 1;
    ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVEL, 0);
    memset(ceiling, 0, sizeof(*ceiling));
    P_AddThinker (&ceiling->thinker);
    sec->ceilingdata = ceiling;               //jff 2/22/98
    ceiling->thinker.function = T_MoveCeiling;
    ceiling->sector = sec;
    ceiling->crush = false;

    // setup ceiling structure according to type of function
    switch(type)
    {
      case fastCrushAndRaise:
        ceiling->crush = true;
        ceiling->topheight = sec->ceilingheight;
        ceiling->bottomheight = sec->floorheight + (8*FRACUNIT);
        ceiling->direction = -1;
        ceiling->speed = CEILSPEED * 2;
        break;

      case silentCrushAndRaise:
      case crushAndRaise:
        ceiling->crush = true;
        ceiling->topheight = sec->ceilingheight;
        // fallthrough
      case lowerAndCrush:
      case lowerToFloor:
        ceiling->bottomheight = sec->floorheight;
        if (type != lowerToFloor)
          ceiling->bottomheight += 8*FRACUNIT;
        ceiling->direction = -1;
        ceiling->speed = CEILSPEED;
        break;

      case raiseToHighest:
        ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
        ceiling->direction = 1;
        ceiling->speed = CEILSPEED;
        break;

      case lowerToLowest:
        ceiling->bottomheight = P_FindLowestCeilingSurrounding(sec);
        ceiling->direction = -1;
        ceiling->speed = CEILSPEED;
        break;

      case lowerToMaxFloor:
        ceiling->bottomheight = P_FindHighestFloorSurrounding(sec);
        ceiling->direction = -1;
        ceiling->speed = CEILSPEED;
        break;

      default:
        break;
    }

    // add the ceiling to the active list
    ceiling->tag = sec->tag;
    ceiling->type = type;
    P_AddActiveCeiling(ceiling);
    if (zerotag_manual) return rtn; //e6y
  }
  return rtn;
}

//////////////////////////////////////////////////////////////////////
//
// Active ceiling list primitives
//
/////////////////////////////////////////////////////////////////////

// jff 2/22/98 - modified Lee's plat code to work for ceilings
//
// The following were all rewritten by Lee Killough
// to use the new structure which places no limits
// on active ceilings. It also avoids spending as much
// time searching for active ceilings. Previously a
// fixed-size array was used, with NULL indicating
// empty entries, while now a doubly-linked list
// is used.

//
// P_ActivateInStasisCeiling()
//
// Reactivates all stopped crushers with the right tag
//
// Passed the line reactivating the crusher
// Returns true if a ceiling reactivated
//
//jff 4/5/98 return if activated
int P_ActivateInStasisCeiling(int tag)
{
  ceilinglist_t *cl;
  int rtn=0;

  for (cl=activeceilings; cl; cl=cl->next)
  {
    ceiling_t *ceiling = cl->ceiling;
    if (ceiling->tag == tag && ceiling->direction == 0)
    {
      ceiling->direction = ceiling->olddirection;
      ceiling->thinker.function = T_MoveCeiling;
      //jff 4/5/98 return if activated
      rtn=1;
    }
  }
  return rtn;
}

//
// EV_CeilingCrushStop()
//
// Stops all active ceilings with the right tag
//
// Passed the linedef stopping the ceilings
// Returns true if a ceiling put in stasis
//
int EV_CeilingCrushStop(line_t* line)
{
  int rtn=0;

  ceilinglist_t *cl;
  for (cl=activeceilings; cl; cl=cl->next)
  {
    ceiling_t *ceiling = cl->ceiling;
    if (ceiling->direction != 0 && ceiling->tag == line->tag)
    {
      ceiling->olddirection = ceiling->direction;
      ceiling->direction = 0;
      ceiling->thinker.function = NULL;
      rtn=1;
    }
  }
  return rtn;
}

//
// P_AddActiveCeiling()
//
// Adds a ceiling to the head of the list of active ceilings
//
// Passed the ceiling motion structure
// Returns nothing
//
void P_AddActiveCeiling(ceiling_t* ceiling)
{
  ceilinglist_t *list = malloc(sizeof *list);
  list->ceiling = ceiling;
  ceiling->list = list;
  if ((list->next = activeceilings))
    list->next->prev = &list->next;
  list->prev = &activeceilings;
  activeceilings = list;
}

//
// P_RemoveActiveCeiling()
//
// Removes a ceiling from the list of active ceilings
//
// Passed the ceiling motion structure
// Returns nothing
//
void P_RemoveActiveCeiling(ceiling_t* ceiling)
{
  ceilinglist_t *list = ceiling->list;
  ceiling->sector->ceilingdata = NULL;  //jff 2/22/98
  P_RemoveThinker(&ceiling->thinker);
  P_TagFinished(ceiling->sector->tag);
  if ((*list->prev = list->next))
    list->next->prev = list->prev;
  free(list);
}

//
// P_RemoveAllActiveCeilings()
//
// Removes all ceilings from the active ceiling list
//
// Passed nothing, returns nothing
//
void P_RemoveAllActiveCeilings(void)
{
  while (activeceilings)
  {
    ceilinglist_t *next = activeceilings->next;
    free(activeceilings);
    activeceilings = next;
  }
}

// hexen

static void Hexen_T_MoveCeiling(ceiling_t * ceiling)
{
    result_e res;

    switch (ceiling->direction)
    {
//              case 0:         // IN STASIS
//                      break;
        case 1:                // UP
            res = T_MovePlane(ceiling->sector, ceiling->speed,
                              ceiling->topheight, false, 1,
                              ceiling->direction, true);
            if (res == pastdest)
            {
                SN_StopSequence((mobj_t *) & ceiling->sector->soundorg);
                switch (ceiling->type)
                {
                    case CLEV_CRUSHANDRAISE:
                        ceiling->direction = -1;
                        ceiling->speed = ceiling->speed * 2;
                        break;
                    default:
                        P_RemoveActiveCeiling(ceiling);
                        break;
                }
            }
            break;
        case -1:               // DOWN
            res = T_MovePlane(ceiling->sector, ceiling->speed,
                              ceiling->bottomheight, ceiling->crush, 1,
                              ceiling->direction, true);
            if (res == pastdest)
            {
                SN_StopSequence((mobj_t *) & ceiling->sector->soundorg);
                switch (ceiling->type)
                {
                    case CLEV_CRUSHANDRAISE:
                    case CLEV_CRUSHRAISEANDSTAY:
                        ceiling->direction = 1;
                        ceiling->speed = ceiling->speed / 2;
                        break;
                    default:
                        P_RemoveActiveCeiling(ceiling);
                        break;
                }
            }
            else if (res == crushed)
            {
                switch (ceiling->type)
                {
                    case CLEV_CRUSHANDRAISE:
                    case CLEV_LOWERANDCRUSH:
                    case CLEV_CRUSHRAISEANDSTAY:
                        //ceiling->speed = ceiling->speed/4;
                        break;
                    default:
                        break;
                }
            }
            break;
    }
}

int Hexen_EV_CeilingCrushStop(line_t * line, byte * args)
{
    ceilinglist_t *cl;
    for (cl=activeceilings; cl; cl=cl->next)
    {
        ceiling_t *ceiling = cl->ceiling;
        if (ceiling->tag == args[0])
        {
            SN_StopSequence((mobj_t *) & ceiling->sector->soundorg);
            P_RemoveActiveCeiling(ceiling);

            return 1;
        }
    }
    return 0;
}

static void P_SpawnZDoomCeiling(sector_t *sec, ceiling_e type, line_t *line, int tag,
                                fixed_t speed, fixed_t speed2, fixed_t height, int crush,
                                int silent, int change, crushmode_e crushmode)
{
  ceiling_t *ceiling;
  fixed_t targheight = 0;

  ceiling = Z_Malloc(sizeof(*ceiling), PU_LEVEL, 0);
  memset(ceiling, 0, sizeof(*ceiling));
  P_AddThinker(&ceiling->thinker);
  sec->ceilingdata = ceiling;
  ceiling->thinker.function = T_MoveCeiling;
  ceiling->sector = sec;
  ceiling->speed = speed;
  ceiling->oldspeed = speed;
  ceiling->speed2 = speed2;
  ceiling->silent = (silent & ~4);
  ceiling->texture = NO_TEXTURE;

  switch (type)
  {
    case ceilCrushAndRaise:
    case ceilCrushRaiseAndStay:
      ceiling->topheight = sec->ceilingheight;
    case ceilLowerAndCrush:
      targheight = sec->floorheight + height;
      ceiling->bottomheight = targheight;
      ceiling->direction = -1;
      break;
    case ceilRaiseToHighest:
      targheight = P_FindHighestCeilingSurrounding(sec);
      ceiling->topheight = targheight;
      ceiling->direction = 1;
      break;
    case ceilLowerByValue:
      targheight = sec->ceilingheight - height;
      ceiling->bottomheight = targheight;
      ceiling->direction = -1;
      break;
    case ceilRaiseByValue:
      targheight = sec->ceilingheight + height;
      ceiling->topheight = targheight;
      ceiling->direction = 1;
      break;
    case ceilMoveToValue:
      {
        fixed_t diff = height - sec->ceilingheight;

        targheight = height;
        if (diff < 0)
        {
          ceiling->bottomheight = height;
          ceiling->direction = -1;
        }
        else
        {
          ceiling->topheight = height;
          ceiling->direction = 1;
        }
      }
      break;
    case ceilLowerToHighestFloor:
      targheight = P_FindHighestFloorSurrounding(sec) + height;
      ceiling->bottomheight = targheight;
      ceiling->direction = -1;
      break;
    case ceilRaiseToHighestFloor:
      targheight = P_FindHighestFloorSurrounding(sec);
      ceiling->topheight = targheight;
      ceiling->direction = 1;
      break;
    case ceilLowerInstant:
      targheight = sec->ceilingheight - height;
      ceiling->bottomheight = targheight;
      ceiling->direction = -1;
      ceiling->speed = height;
      break;
    case ceilRaiseInstant:
      targheight = sec->ceilingheight + height;
      ceiling->topheight = targheight;
      ceiling->direction = 1;
      ceiling->speed = height;
      break;
    case ceilLowerToNearest:
      targheight = P_FindNextLowestCeiling(sec, sec->ceilingheight);
      ceiling->bottomheight = targheight;
      ceiling->direction = -1;
      break;
    case ceilRaiseToNearest:
      targheight = P_FindNextHighestCeiling(sec, sec->ceilingheight);
      ceiling->topheight = targheight;
      ceiling->direction = 1;
      break;
    case ceilLowerToLowest:
      targheight = P_FindLowestCeilingSurrounding(sec);
      ceiling->bottomheight = targheight;
      ceiling->direction = -1;
      break;
    case ceilRaiseToLowest:
      targheight = P_FindLowestCeilingSurrounding(sec);
      ceiling->topheight = targheight;
      ceiling->direction = 1;
      break;
    case ceilLowerToFloor:
      targheight = sec->floorheight + height;
      ceiling->bottomheight = targheight;
      ceiling->direction = -1;
      break;
    case ceilRaiseToFloor:
      targheight = sec->floorheight + height;
      ceiling->topheight = targheight;
      ceiling->direction = 1;
      break;
    case ceilLowerToHighest:
      targheight = P_FindHighestCeilingSurrounding(sec);
      ceiling->bottomheight = targheight;
      ceiling->direction = -1;
      break;
    case ceilLowerByTexture:
      targheight = sec->ceilingheight - P_FindShortestUpperAround(sec->iSectorID);
      ceiling->bottomheight = targheight;
      ceiling->direction = -1;
      break;
    case ceilRaiseByTexture:
      targheight = sec->ceilingheight + P_FindShortestUpperAround(sec->iSectorID);
      ceiling->topheight = targheight;
      ceiling->direction = 1;
      break;
    default:
      break;
  }

  ceiling->tag = tag;
  ceiling->type = type;
  ceiling->crush = crush;
  ceiling->crushmode = crushmode;

  // Don't make noise for instant movement ceilings
  if (ceiling->direction < 0)
  {
    if (ceiling->speed >= sec->ceilingheight - ceiling->bottomheight)
      if (silent & 4)
        ceiling->silent = 2;
  }
  else
  {
    if (ceiling->speed >= ceiling->topheight - sec->ceilingheight)
      if (silent & 4)
        ceiling->silent = 2;
  }

  // set texture/type change properties
  if (change & 3) // if a texture change is indicated
  {
    if (change & 4) // if a numeric model change
    {
      sector_t *modelsec;

      // jff 5/23/98 find model with floor at target height if target is a floor type
      modelsec = (type == ceilRaiseToFloor || type == ceilLowerToFloor) ?
                 P_FindModelFloorSector(targheight, sec->iSectorID) :
                 P_FindModelCeilingSector(targheight, sec->iSectorID);

      if (modelsec != NULL)
      {
        ceiling->texture = modelsec->ceilingpic;
        switch (change & 3)
        {
          case 0:
            break;
          case 1: // type is zeroed
            P_ResetTransferSpecial(&ceiling->newspecial);
            ceiling->type = genCeilingChg0;
            break;
          case 2: // type is copied
            P_CopyTransferSpecial(&ceiling->newspecial, sec);
            ceiling->type = genCeilingChgT;
            break;
          case 3: // type is left alone
            ceiling->type = genCeilingChg;
            break;
        }
      }
    }
    else if (line)  // else if a trigger model change
    {
      ceiling->texture = line->frontsector->ceilingpic;
      switch (change & 3)
      {
        case 0:
          break;
        case 1: // type is zeroed
          P_ResetTransferSpecial(&ceiling->newspecial);
          ceiling->type = genCeilingChg0;
          break;
        case 2: // type is copied
          P_CopyTransferSpecial(&ceiling->newspecial, line->frontsector);
          ceiling->type = genCeilingChgT;
          break;
        case 3: // type is left alone
          ceiling->type = genCeilingChg;
          break;
      }
    }
  }

  P_AddActiveCeiling(ceiling);

  return;
}

int EV_DoZDoomCeiling(ceiling_e type, line_t *line, byte tag, fixed_t speed, fixed_t speed2,
                      fixed_t height, int crush, int silent, int change, crushmode_e crushmode)
{
  sector_t *sec;
  int secnum = -1;
  int retcode = 0;

  height *= FRACUNIT;

  // check if a manual trigger, if so do just the sector on the backside
  if (tag == 0)
  {
    if (!line || !(sec = line->backsector))
      return 0;

    secnum = sec - sectors;
    // [RH] Hack to let manual crushers be retriggerable, too
    tag ^= secnum | 0x1000000;
    P_ActivateInStasisCeiling(tag);

    if (sec->ceilingdata)
      return 0;

    P_SpawnZDoomCeiling(sec, type, line, tag, speed, speed2,
                        height, crush, silent, change, crushmode);
    return 1;
  }

  // Reactivate in-stasis ceilings...for certain types.
  // This restarts a crusher after it has been stopped
  if (type == ceilCrushAndRaise)
  {
    P_ActivateInStasisCeiling(tag);
  }

  while ((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
  {
    sec = &sectors[secnum];
    if (sec->ceilingdata)
    {
      continue;
    }
    retcode = 1;
    P_SpawnZDoomCeiling(sec, type, line, tag, speed, speed2,
                        height, crush, silent, change, crushmode);
  }

  return retcode;
}

int Hexen_EV_DoCeiling(line_t * line, byte * arg, ceiling_e type)
{
    int secnum, rtn;
    sector_t *sec;
    ceiling_t *ceiling;

    secnum = -1;
    rtn = 0;

    while ((secnum = P_FindSectorFromTag(arg[0], secnum)) >= 0)
    {
        sec = &sectors[secnum];
        if (sec->floordata || sec->ceilingdata)
            continue;

        //
        // new door thinker
        //
        rtn = 1;
        ceiling = Z_Malloc(sizeof(*ceiling), PU_LEVEL, 0);
        memset(ceiling, 0, sizeof(*ceiling));
        P_AddThinker(&ceiling->thinker);
        sec->ceilingdata = ceiling;
        ceiling->thinker.function = T_MoveCeiling;
        ceiling->sector = sec;
        ceiling->crush = 0;
        ceiling->speed = arg[1] * (FRACUNIT / 8);
        switch (type)
        {
            case CLEV_CRUSHRAISEANDSTAY:
                ceiling->crush = arg[2];        // arg[2] = crushing value
                ceiling->topheight = sec->ceilingheight;
                ceiling->bottomheight = sec->floorheight + (8 * FRACUNIT);
                ceiling->direction = -1;
                break;
            case CLEV_CRUSHANDRAISE:
                ceiling->topheight = sec->ceilingheight;
            case CLEV_LOWERANDCRUSH:
                ceiling->crush = arg[2];        // arg[2] = crushing value
            case CLEV_LOWERTOFLOOR:
                ceiling->bottomheight = sec->floorheight;
                if (type != CLEV_LOWERTOFLOOR)
                {
                    ceiling->bottomheight += 8 * FRACUNIT;
                }
                ceiling->direction = -1;
                break;
            case CLEV_RAISETOHIGHEST:
                ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
                ceiling->direction = 1;
                break;
            case CLEV_LOWERBYVALUE:
                ceiling->bottomheight =
                    sec->ceilingheight - arg[2] * FRACUNIT;
                ceiling->direction = -1;
                break;
            case CLEV_RAISEBYVALUE:
                ceiling->topheight = sec->ceilingheight + arg[2] * FRACUNIT;
                ceiling->direction = 1;
                break;
            case CLEV_MOVETOVALUETIMES8:
                {
                    int destHeight = arg[2] * FRACUNIT * 8;

                    if (arg[3])
                    {
                        destHeight = -destHeight;
                    }
                    if (sec->ceilingheight <= destHeight)
                    {
                        ceiling->direction = 1;
                        ceiling->topheight = destHeight;
                        if (sec->ceilingheight == destHeight)
                        {
                            rtn = 0;
                        }
                    }
                    else if (sec->ceilingheight > destHeight)
                    {
                        ceiling->direction = -1;
                        ceiling->bottomheight = destHeight;
                    }
                    break;
                }
            default:
                rtn = 0;
                break;
        }
        ceiling->tag = sec->tag;
        ceiling->type = type;
        P_AddActiveCeiling(ceiling);
        if (rtn)
        {
            SN_StartSequence((mobj_t *) & ceiling->sector->soundorg,
                             SEQ_PLATFORM + ceiling->sector->seqType);
        }
    }
    return rtn;
}
