/*
 *  player.cpp
 *  Nuvie
 *
 *  Created by Eric Fry on Sun Mar 23 2003.
 *  Copyright (c) 2003. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "nuvieDefs.h"

#include "Configuration.h"
#include "U6misc.h"
#include "NuvieIO.h"
#include "ActorManager.h"
#include "Actor.h"
#include "ObjManager.h"
#include "MapWindow.h"
#include "GameClock.h"
#include "MsgScroll.h"
#include "Party.h"
#include "U6objects.h"
#include "Player.h"

Player::Player(Configuration *cfg)
{
 config = cfg;
 config->value("config/GameType",game_type);

 karma = 0;
 questf = 0;
}

bool Player::init(ObjManager *om, ActorManager *am, MapWindow *mw, GameClock *c, Party *p)
{

 clock = c;
 actor_manager = am;
 obj_manager = om;
 map_window = mw;
 party = p;
 
 current_weapon = -1;
 
 init();

 return true;
}

void Player::init()
{
 actor = NULL;

 party_mode = true;
 mapwindow_centered = true;

}

bool Player::load(NuvieIO *objlist)
{
 uint8 solo_member_num = 0xff;

 init();

 objlist->seek(0xf00);

 objlist->readToBuf((unsigned char *)name,14); // read in Player name.

 if(game_type == NUVIE_GAME_U6)
   {
    objlist->seek(0x1bf1); // U6 Quest Flag
    questf = objlist->read1();

    objlist->seek(0x1bf9); // Player Karma.
    karma = objlist->read1();

    objlist->seek(0x1c71); // Player Gender.
    gender = objlist->read1();

    objlist->seek(0x1c6a); //Party Mode = 0xff other wise it is solo mode party member number starting from 0.
    solo_member_num = objlist->read1();
   }

 if(game_type == NUVIE_GAME_MD)
   {
    objlist->seek(0x1d27); // Player Gender.
    gender = objlist->read1();
   }

 set_actor(find_actor());

 if(solo_member_num == 0xff)
   {
    party_mode = true;
    set_party_mode(actor);
   }
 else
    set_solo_mode(party->get_actor(solo_member_num));

 return true;
}

bool Player::save(NuvieIO *objlist)
{
 if(game_type == NUVIE_GAME_U6)
   {
    objlist->seek(0x1bf1); // U6 Quest Flag
    objlist->write1(questf);

    objlist->seek(0x1bf9); // Player Karma.
    objlist->write1(karma);

    objlist->seek(0x1c71); // Player Gender.
    objlist->write1(gender);

    objlist->seek(0x1c6a); // solo member num.
    if(party_mode)
      objlist->write1(0xff); // 0xff = party mode
    else
      objlist->write1(party->get_member_num(actor)); //write the party member number of the solo actor
   }

 if(game_type == NUVIE_GAME_MD)
   {
    objlist->seek(0x1d27); // Player Gender.
    objlist->write1(gender);
   }

 return true;
}

/* Returns the first actor set as Player.
 */
Actor *Player::find_actor()
{
    for(uint32 p = 0; p < 255; p++)
    {
        Actor *actor = actor_manager->get_actor(p);
        if(actor->get_worktype() == 0x02) // WT_U6_PLAYER
            return(actor);
    }
    return(actor_manager->get_actor(1)); // U6NPC_AVATAR
}


// keep MapWindow focused on Player actor, or remove focus
void Player::set_mapwindow_centered(bool state)
{
 uint16 x,y;
 uint8 z;

 mapwindow_centered = state;
 if(mapwindow_centered == false)
    return;
 map_window->centerMapOnActor(actor);

 get_location(&x,&y,&z);
 obj_manager->update(x,y,z); // spawn eggs when teleporting. eg red moongate.
}

void Player::set_actor(Actor *new_actor)
{
 actor = new_actor;
 actor->set_worktype(0x2); // WT_U6_PLAYER
 actor_manager->set_player(actor);
 map_window->centerMapOnActor(actor);
}

Actor *Player::get_actor()
{
 return actor;
}

void Player::get_location(uint16 *ret_x, uint16 *ret_y, uint8 *ret_level)
{
 actor->get_location(ret_x,ret_y,ret_level);
}

uint8 Player::get_location_level()
{
 return actor->z;
}

char *Player::get_name()
{
 return name;
}


/* Add to Player karma. Handle appropriately the karma min/max limits.
 */
void Player::add_karma(uint8 val)
{
    karma = ((karma + val) <= 99) ? karma + val : 99;
}


/* Subtract from Player karma. Handle appropriately the karma min/max limits.
 */
void Player::subtract_karma(uint8 val)
{
    karma = ((karma - val) >= 0) ? karma - val : 0;
}


char *Player::get_gender_title()
{
 if(gender == 0)
   return "milord";
 else
   return "milady";
}


// walk to adjacent square
void Player::moveRelative(sint16 rel_x, sint16 rel_y)
{
    uint16 x, y;
    uint8 z;
    actor->get_location(&x, &y, &z);

    // don't allow diagonal move between blocked tiles (player only)
    if(rel_x && rel_y && !actor->check_move(x + rel_x, y + 0, z, ACTOR_IGNORE_OTHERS)
                      && !actor->check_move(x + 0, y + rel_y, z, ACTOR_IGNORE_OTHERS))
        return;
    if(actor->is_immobile())
        return;
    if(!actor->moveRelative(rel_x,rel_y)) /**MOVE**/
    {
        ActorError *ret = actor->get_error();
        if(ret->err == ACTOR_BLOCKED_BY_ACTOR
           && party->contains_actor(ret->blocking_actor))
        {
            ret->blocking_actor->push(actor, ACTOR_PUSH_HERE);
        }
        if(!actor->moveRelative(rel_x,rel_y)) /**MOVE**/
           return;
    }
    // post-move
    actor->set_direction(rel_x, rel_y);
    if(party_mode && party->is_leader(actor)) // lead party
        party->follow(rel_x, rel_y);
    else if(actor->id_n == 0) // using vehicle; drag party along
    {
        MapCoord new_xyz = actor->get_location();
        party->move(new_xyz.x, new_xyz.y, new_xyz.z);
    }
    // update world around player
    actor_manager->updateActors(x, y, z);
    obj_manager->update(x, y, z); // remove temporary objs, hatch eggs
    clock->inc_move_counter();
}

// teleport-type move
void Player::move(sint16 new_x, sint16 new_y, uint8 new_level)
{
   if(actor->move(new_x, new_y, new_level, ACTOR_FORCE_MOVE))
   {
    //map_window->centerMapOnActor(actor);
    if(party->is_leader(actor)) // lead party
      {
       party->move(new_x, new_y, new_level); // center everyone first
       party->follow(0, 0); // then try to move them to correct positions
      }
    actor_manager->updateActors(new_x, new_y, new_level);
    obj_manager->update(new_x, new_y, new_level); // remove temporary objs, hatch eggs
   }
}

void Player::moveLeft()
{
 moveRelative(-1,0);
}

void Player::moveRight()
{
 moveRelative(1,0);
}

void Player::moveUp()
{
 moveRelative(0,-1);
}

void Player::moveDown()
{
 moveRelative(0,1);
}

void Player::pass()
{
 uint16 x = actor->x, y = actor->y;
 uint8 z = actor->z;

 // update world around player
 if(party_mode && party->is_leader(actor)) // lead party
   party->follow(0, 0);
 actor_manager->updateActors(x, y, z);
 clock->inc_move_counter_by_a_minute();
}



/* Enter party mode, with everyone following actor. (must be in the party)
 */
bool Player::set_party_mode(Actor *new_actor)
{
 MsgScroll *scroll = Game::get_game()->get_scroll();
 
    if(party->contains_actor(new_actor))
    {
        std::string prompt = get_party()->get_actor_name(0);
        prompt += ":\n>";
        scroll->set_prompt((char *)prompt.c_str());

        party_mode = true;
        actor = new_actor;
        return(true);
    }
    return(false);
}


/* Enter solo mode as actor. (must be in the party)
 */
bool Player::set_solo_mode(Actor *new_actor)
{
    if(party->contains_actor(new_actor))
    {
        party_mode = false;
        actor = new_actor;
        actor->delete_pathfinder();
        return(true);
    }
    return(false);
}


/* Returns the delay in continuous movement for the actor type we control.
 */
uint32 Player::get_walk_delay()
{
    if(actor->obj_n == OBJ_U6_BALLOON_BASKET)
        return(10); // 10x normal (wow!)
    else if(actor->obj_n == OBJ_U6_SHIP)
        return(20); // 5x normal
    else if(actor->obj_n == OBJ_U6_SKIFF)
        return(50); // 2x normal
    else if(actor->obj_n == OBJ_U6_RAFT)
        return(100); // normal
    else if(actor->obj_n == OBJ_U6_HORSE_WITH_RIDER)
        return(50); // 2x normal
    else
        return(125); // normal movement about 8 spaces per second
}


/* Returns true if it's time for the player to take another step.
 * (call during continuous movement)
 */
bool Player::check_walk_delay()
{
    static uint32 walk_delay = 0, // start with no delay
                  last_time = clock->get_ticks();
    uint32 this_time = clock->get_ticks();
    uint32 time_passed = this_time - last_time;

    // subtract time_passed until delay is 0
    if(sint32(walk_delay - time_passed) < 0)
        walk_delay = 0;
    else
        walk_delay -= time_passed;
    last_time = this_time; // set each call to get time_passed
    if(walk_delay == 0)
    {
        walk_delay = get_walk_delay(); // reset
        return(true);
    }
    return(false); // not time yet
}

bool Player::weapon_can_hit(uint16 x, uint16 y)
{   
 return actor->weapon_can_hit(actor->get_weapon(current_weapon), x, y);
}

void Player::attack_select_init()
{
 map_window->centerCursor();
 
 current_weapon = ACTOR_NO_READIABLE_LOCATION;
 
 if(attack_select_next_weapon() == false)
   attack_select_weapon_at_location(ACTOR_NO_READIABLE_LOCATION); // attack with hands.
 
 return;
}

bool Player::attack_select_next_weapon()
{
 sint8 i;

 for(i=current_weapon + 1; i < ACTOR_MAX_READIED_OBJECTS;i++)
   {
    if(attack_select_weapon_at_location(i) == true)
      return true;
   }

 return false;
}

bool Player::attack_select_weapon_at_location(sint8 location)
{
 const CombatType *weapon;
 MsgScroll *scroll = Game::get_game()->get_scroll();
 
 if(location == ACTOR_NO_READIABLE_LOCATION)
   {
    current_weapon = location;
    scroll->display_string("Attack with bare hands-");
    return true;
   }

 weapon = actor->get_weapon(location);
    
 if(weapon && weapon->attack > 0)
   {
    current_weapon = location;
    scroll->display_string("Attack with ");
    scroll->display_string(obj_manager->get_obj_name(weapon->obj_n));
    scroll->display_string("-");
    return true;
   }

 return false;
}

void Player::attack(Actor *a)
{
 MsgScroll *scroll = Game::get_game()->get_scroll();
 
 if(weapon_can_hit(a->x,a->y))
   actor->attack(current_weapon, a);
 else
   scroll->display_string("\nOut of range!\n");
   
 return;
}
