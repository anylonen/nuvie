#ifndef __Magic_h__
#define __Magic_h__

/*
 *  Magic.h
 *  Nuvie
 *
 *  Created by Pieter Luteijn on Mon Nov 15 2004,
 *  Copyright (c) 2004. All rights reserved.
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

#include "Event.h"
#include "CallBack.h"

#define REAGENT_U6_MANDRAKE_ROOT 0x01
#define REAGENT_U6_NIGHTSHADE    0x02
#define REAGENT_U6_BLACK_PEARL   0x04
#define REAGENT_U6_BLOOD_MOSS    0x08
#define REAGENT_U6_SPIDER_SILK   0x10
#define REAGENT_U6_GARLIC        0x20
#define REAGENT_U6_GINSENG       0x40
#define REAGENT_U6_SULFUROUS_ASH 0x80

#define MAX_SCRIPT_LENGTH 65000
#define MAX_TOKEN_LENGTH 255 
#define MAX_STACK_DEPTH 255 

#define MAGIC_STATE_READY 	   0x00
#define MAGIC_STATE_SELECT_SPELL   0x01
#define MAGIC_STATE_PROCESS_SCRIPT 0x02
#define MAGIC_STATE_ACQUIRE_TARGET 0x03
#define MAGIC_STATE_ACQUIRE_INPUT  0x04
#define MAGIC_STATE_ACQUIRE_DIRECTION  0x05
#define MAGIC_STATE_ACQUIRE_INV_OBJ  0x06
#define MAGIC_STATE_TALK_TO_ACTOR 0x07
#define MAGIC_STATE_ACQUIRE_SPELL  0x08
#define MAGIC_STATE_ACQUIRE_OBJ    0x09

#define MAGIC_ALL_SPELLS 255

class ScriptThread;
class NuvieIOFileRead;

class Spell {
  public: /* saves using dumb get / set functions */
	uint8 num;
	char * name; // eg. "Heal" 
	char * invocation; // eg. "im" 
	uint8 reagents; // reagents used
	
	Spell(uint8 new_num, const char *new_name="undefined name",const char *new_invocation="kawo", uint8 new_reagents=255) {
	  num=new_num;
	  name=strdup(new_name);
	  invocation=strdup(new_invocation);
	  reagents=new_reagents;
	};
	~Spell(){
	  free(name);
	  free(invocation);
	}
};

class Magic : public CallBack {
  private:
    Spell *spell[256]; // spell list;
    char cast_buffer_str[26]; // buffer for spell syllables typed.
    uint8 cast_buffer_len; // how many characters typed in the spell buffer.
    Event *event;
    //Actor *target_actor;
    Obj *target_object;
    uint8 state;
    
    ScriptThread *magic_script;
    Obj *spellbook_obj;
    /* TODO
     * add a register array, or a pointer to a list of variables?
     */
  public: 
    Magic();
    ~Magic();
    bool init(Event *evt);
    
    bool read_spell_list();
    void clear_cast_buffer() { cast_buffer_str[0] = '\0'; cast_buffer_len = 0; }
    bool start_new_spell();
    Obj *book_equipped();
    bool cast();
    void cast_spell_directly(uint8 spell_num);

    uint16 callback(uint16 msg, CallBack *caller, void *data = NULL);
    bool process_script_return(uint8 ret);
    bool resume(MapCoord location);
    bool resume(uint8 dir);
    bool resume_with_spell_num(uint8 spell_num);
    bool resume(Obj *obj);
    bool resume();
    bool is_waiting_for_location() { if(magic_script && state == MAGIC_STATE_ACQUIRE_TARGET) return true; else return false; }
    bool is_waiting_for_direction() { if(magic_script && state == MAGIC_STATE_ACQUIRE_DIRECTION) return true; else return false; }
    bool is_waiting_for_inventory_obj() { if(magic_script && state == MAGIC_STATE_ACQUIRE_INV_OBJ) return true; else return false; }
    bool is_waiting_for_obj() { if(magic_script && state == MAGIC_STATE_ACQUIRE_OBJ) return true; else return false; }
    bool is_waiting_to_talk() { if(state == MAGIC_STATE_TALK_TO_ACTOR) return true; else return false; }
    bool is_waiting_for_spell() { if(magic_script && state == MAGIC_STATE_ACQUIRE_SPELL) return true; else return false; }
    bool is_selecting_spell() { if(magic_script && state == MAGIC_STATE_SELECT_SPELL) return true; else return false; }

    bool is_waiting_to_resume() { if(magic_script) return true; else return false; }

    Spell *get_spell(uint8 spell_num) { return spell[spell_num]; }
    Obj *get_spellbook_obj() { return spellbook_obj; }

    Actor *get_actor_from_script();
    void show_spell_description(uint8 index);
private:
    bool spellbook_has_spell(Obj *book, uint8 spell_index);
    void display_ingredients(uint8 index);
    void display_spell_incantation(uint8 index);

};


#endif /* __Magic_h__ */
