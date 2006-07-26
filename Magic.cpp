/*
 *  Magic.cpp
 *  Nuvie
 *
 *  Created by Eric Fry on Mon Nov 15 2004.
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

#include <cassert>
#include <cstring>
#include <iostream>
#include "SDL.h"
#include "nuvieDefs.h"
#include "U6misc.h"
#include "Party.h"
#include "MsgScroll.h"
#include "Configuration.h"
#include "NuvieIOFile.h"

#include "UseCode.h"

#include "GUI.h"
#include "GUI_YesNoDialog.h"

#include "Actor.h"
#include "ObjManager.h"
#include "U6objects.h"
#include "Magic.h"

using std::cerr;
using std::cin;
using std::cout;
using std::endl;

/* Syllable            Meaning     Syllable            Meaning
 * An .......... Negate/Dispel     Nox ................ Poison
 * Bet ................. Small     Ort ................. Magic
 * Corp ................ Death     Por ......... Move/Movement
 * Des ............ Lower/Down     Quas ............. Illusion
 * Ex ................ Freedom     Rel ................ Change
 * Flam ................ Flame     Sanct .. Protect/Protection
 * Grav ......... Energy/Field     Tym .................. Time
 * Hur .................. Wind     Uus .............. Raise/Up
 * In ...... Make/Create/Cause     Vas ................. Great
 * Jux ...... Danger/Trap/Harm     Wis ........ Know/Knowledge
 * Kal ......... Summon/Invoke     Xen .............. Creature
 * Lor ................. Light     Ylem ............... Matter
 * Mani ......... Life/Healing     Zu .................. Sleep
 */

const char* syllable[26]={"An ", "Bet ", "Corp ", "Des ", "Ex ", "Flam ", "Grav ", "Hur ", "In ", "Jux ", "Kal ", "Lor ", "Mani ", "Nox ", "Ort ", "Por ", "Quas ", "Rel ", "Sanct ", "Tym ", "Uus ", "Vas ", "Wis ", "Xen ", "Ylem ", "Zu "};
const char* reagent[8]={"mandrake root", "nightshade", "black pearl", "blood moss", "spider silk", "garlic", "ginseng", "sulfurous ash"}; // check names
const int obj_n_reagent[8]={OBJ_U6_MANDRAKE_ROOT, OBJ_U6_NIGHTSHADE, OBJ_U6_BLACK_PEARL, OBJ_U6_BLOOD_MOSS, OBJ_U6_SPIDER_SILK, OBJ_U6_GARLIC, OBJ_U6_GINSENG, OBJ_U6_SULFUROUS_ASH};


char* Stack::pop() {
  if (SP>0)
  { 
    strncpy(token,element[--SP],MAX_TOKEN_LENGTH); 
    free(element[SP]);
    return token;
  }
  throw "Stack underflow\n";
}
void Stack::push(const char *value) {
  element[SP++]=strdup(value);
  if (SP>MAX_STACK_DEPTH) 
  { 
    throw "Stack overflow\n"; 
  }
}
void Stack::pushptr(const void *ptr) {
  size_t ps=sizeof(ptr);
  element[SP]=(char*)malloc(ps+3); // get some space
  element[SP][0]='<';
  memcpy(element[SP]+1,&ptr,ps);
  element[SP][ps+1]='>';
  element[SP][ps+2]='\0';
  SP++;
  if (SP>MAX_STACK_DEPTH) 
  { 
    throw "Stack overflow\n"; 
  }
}
void * Stack::popptr() {
  if (SP>0)
  { 
    --SP;
    void *ptr;
    size_t ps=sizeof(ptr);
    memcpy(&ptr,element[SP]+1,ps);
    free(element[SP]);
    return ptr;
  }
  throw "Stack underflow\n";
}
void Stack::dup() {
  if (SP>0) {
    push(element[SP-1]);
    return;
  }
  throw "Stack underflow\n";
}
void Stack::swap() {
  if (SP>1) {
    char * hold=element[SP-1];
    element[SP-1]=element[SP-2];
    element[SP-2]=hold;
    return;
  }
  throw "Stack underflow\n";
}
void Stack::over() {
  if (SP>1) {
    push(element[SP-2]);
    return;
  }
  throw "Stack underflow\n";
}
void Stack::rot() {
  if (SP>2) {
    char * hold=element[SP-3];
    element[SP-3]=element[SP-2];
    element[SP-2]=element[SP-1];
    element[SP-1]=hold;
    return;
  }
  throw "Stack underflow\n";
}

void Stack::empty() {
  while (SP>0) {pop();}
}

void Stack::dump() {
  cerr<<endl<<"Stack holds:"<<endl;
  for (size_t TSP=0; TSP<SP; TSP++) cerr<<"<"<<TSP<<">"<<" "<<element[TSP]<<endl;
}

Magic::Magic()
{
 for (uint16 index=0;index<256;index++) spell[index]=NULL;
 clear_cast_buffer();
}

Magic::~Magic()
{
 for (uint16 index=0;index<256;index++) delete(spell[index]);
}

void Magic::init(Event *evt)
{
  state=MAGIC_STATE_READY;
  stack=new(Stack);
  event=evt;
  read_spell_list();
}

// FIXME This should be a member of the NuvieIOFileRead class.
bool Magic::read_line(NuvieIOFileRead *file, char *buf, size_t maxlen) {
  size_t buflen=0;

  for (buflen=0;buflen<maxlen;buflen++) 
  {
    buf[buflen]=char(file->read1());
    if (buf[buflen]=='\n') {
      if (buflen) // check for backslash
      {
	if (buf[buflen-1]=='\\')
	{
	  buf[--buflen]=' ';  // turn into a space, and ignore \n
	  continue;
	}
      }
      buf[buflen]='\0'; // end the line here
      return true; // no overflow.
    }
  }
  return false; // overflow.
}

void Magic::lc(char *str) // very crappy doesn't check much.
{
  for (char *conv=str;*conv!='\0';conv++)
  {
    if (*conv<'a') {*conv+=32;};
  }
}

void Magic::read_spell_list()
{
  uint16 index;
  bool complete=false;
  NuvieIOFileRead *spells=new(NuvieIOFileRead);
  char name[80]="";
  char invocation[26]="";
  uint8 reagents=0;
  char script[MAX_SCRIPT_LENGTH+1]="_end";
  char buf[MAX_SCRIPT_LENGTH+1];

  //read spell file from nuvie data directory.

  std::string datadir = GUI::get_gui()->get_data_dir();
  std::string spellfile;
  
  build_path(datadir, "spells.nsl", spellfile);
  
  if (!spells->open(spellfile)) 
  {
    throw "error opening spells.nsl";
  }
  
  /*read records into spell-array 1 by one.*/
  for (index=0;index<256;index++)
  {
    if (spell[index]) //clean up if something there already
    {
      delete spell[index]; 
    }
    complete=false;
    while (!complete)
    {
      /* read a line */ 
      if (!read_line(spells,buf,sizeof(buf)))
      {
	cerr<<"While reading spell "<<index<<":"<<endl;
	throw "Line too long reading spell-list\n";
      };
      /* parse the line */ // TODO far to quick and dirty...
      switch (buf[0]) 
      {
	case '~': // end of record, completed!
	  complete=true; break;
	case '#': // comment line, ignore
	  break;
	case 'n': // assume 'name: '
	  strncpy(name,buf+6,sizeof(name)-1);
	  break;
	case 'i': // assume 'invocation: '
	  strncpy(invocation,buf+12,sizeof(invocation)-1);
	  lc(invocation);
	  break;
	case 'r': // assume 'reagents: '
	    reagents=uint8(strtol(buf+10,(char **)NULL,0));
	  break;
	case 's': // assume 'script: '
	  strncpy(script,buf+8,sizeof(script)-1);
	  break;
      }
    }
    /* store the result */
    spell[index]=new Spell(name,invocation,script,reagents);
  }
  /* close the spell-list-file*/
  spells->close();
  delete spells;
}

bool Magic::handleSDL_KEYDOWN(const SDL_Event * sdl_event)
{
  if (state == MAGIC_STATE_SELECT_SPELL) {
    // TODO keys to handle escaping from this mode
    if (sdl_event->key.keysym.sym==SDLK_RETURN || sdl_event->key.keysym.sym==SDLK_KP_ENTER)
    {
      cast() ;

      event->scroll->display_string("\n");
      event->scroll->display_prompt();

      event->mode = MOVE_MODE;
      return true; // handled the event
    }
    if (sdl_event->key.keysym.sym>=SDLK_1 && sdl_event->key.keysym.sym<=SDLK_8)
    {
      cast(event->player->get_party()->get_actor(sdl_event->key.keysym.sym - 48-1));
      event->mode = MOVE_MODE;
      return true; // handled the event
    } 
    else if (sdl_event->key.keysym.sym>=SDLK_a && sdl_event->key.keysym.sym<=SDLK_z)
    {
      if (cast_buffer_len<25)
      { 
	cast_buffer_str[cast_buffer_len++]=sdl_event->key.keysym.sym;
	event->scroll->display_string(syllable[sdl_event->key.keysym.sym - SDLK_a]);
	return true; // handled the event
      }
      return true; // handled the event
    }
    else if (sdl_event->key.keysym.sym==SDLK_BACKSPACE)
    {
      if (cast_buffer_len>0)
      { 
	cast_buffer_len--; // back up a syllable FIXME, doesn't handle automatically inserted newlines, so we need to keep track more.
	size_t len=strlen(syllable[cast_buffer_str[cast_buffer_len]-SDLK_a]);
	while(len--) event->scroll->remove_char();
	event->scroll->Display(true);
	return true; // handled the event
      }
      return true; // handled the event
    }
  }
  return false; // didn't handle the event
} 

uint8 Magic::book_equipped()
{
  // book(s) equipped? Maybe should check all locations?
  Actor *caster=event->player->get_actor();
  Obj *right=caster->inventory_get_readied_object(ACTOR_ARM); 
  Obj *left=caster->inventory_get_readied_object(ACTOR_ARM_2); 
  uint8 books=0;

  if (right!=NULL && right->obj_n==OBJ_U6_SPELLBOOK) { books+=1; }; 
  if (left!=NULL && left->obj_n==OBJ_U6_SPELLBOOK) { books+=2; }; 
  
  return books;
}

bool Magic::start_new_spell() 
{
  if (book_equipped()) 
  {
    state=MAGIC_STATE_SELECT_SPELL;
    clear_cast_buffer();
    return true;
  }
  event->scroll->display_string("No spellbook is readied.\n"); // FIXME correct text
  state=MAGIC_STATE_READY;
  return false;
}


bool Magic::cast(Actor *Act)
{
  return cast();
}
bool Magic::cast(Obj *Obj)
{
  return cast();
}
bool Magic::cast()
{
  cast_buffer_str[cast_buffer_len]='\0';
  cerr<<"Trying to cast "<<cast_buffer_str<<endl;
  /* decode the invocation */
  uint16 index;
  for (index=0;index<256;index++) 
  {
    if (!strcmp(spell[index]->invocation,cast_buffer_str)) {
      break;
    }
  }
  if (index>=256) {
    cerr<<"didn't find spell in spell list"<<endl;
    event->scroll->display_string("\nThat spell is not in thy spellbook!\n"); // FIXME correct text
    return false;
  }
  event->scroll->display_string(spell[index]->name); // FIXME correct text
  event->scroll->display_string("\n");


  /* debug block */
  cerr<<"matched spell #"<<index<<endl;
  cerr<<"name: "<<spell[index]->name<<endl;
  cerr<<"reagents: ";
  char *comma="";
  for (uint8 shift=0;shift<8;shift++) 
  {
    if (1<<shift&spell[index]->reagents) {
      cerr<<comma<<reagent[shift];
      comma=", ";
    }
  }
  cerr<<endl;
  cerr<<"script: "<<spell[index]->script<<endl;
  /* end debug block */


  if(event->mode == WAIT_MODE)
    return false;
  
  // TODO "No magic at this time!" error

  // book(s) equipped? Maybe should check all locations?
  Actor *caster=event->player->get_actor();
  Obj *right=caster->inventory_get_readied_object(ACTOR_ARM); 
  Obj *left=caster->inventory_get_readied_object(ACTOR_ARM_2); 
  uint8 books=0;

  if (right!=NULL && right->obj_n==OBJ_U6_SPELLBOOK) { books+=1; }; 
  if (left!=NULL && left->obj_n==OBJ_U6_SPELLBOOK) { books+=2; }; 

  if (!books) { 
    event->scroll->display_string("No spellbook is readied.\n"); // FIXME correct text
    return false;
  }

  // TODO "No spells in the spellbook!" error
  
  // spell in (either) book, or spell 255 available

  bool spell_ready=false;  

  if ( 
      ((books&1) && (caster->inventory_get_object(OBJ_U6_SPELL,index,right,true,true) || caster->inventory_get_object(OBJ_U6_SPELL,255,right,true,true)))
      ||
      ((books&2) && (caster->inventory_get_object(OBJ_U6_SPELL,index,left,true,true) || caster->inventory_get_object(OBJ_U6_SPELL,255,left,true,true)))
     )
  {
    spell_ready=true;
  }
  
  if (!spell_ready) 
  {
    event->scroll->display_string("\nThat spell is not in thy spellbook!\n"); // FIXME correct text
    return false;
  }

  // level of caster sufficient
  uint8 spell_level=MIN(8,(index/16)+1); 
  if (caster->get_level()<spell_level)
  {
    event->scroll->display_string("\nYour level is not high enough.\n"); // FIXME correct text
    return false;
  }
  
  // enough Magic Points available
  if (caster->get_magic()<spell_level)
  {
    event->scroll->display_string("\nNot enough magic points.\n"); // FIXME correct text
    return false;
  }

  
  // reagents available
  for (uint8 shift=0;shift<8;shift++) 
  {
    if (1<<shift&spell[index]->reagents) {
      if (!caster->inventory_has_object(obj_n_reagent[shift],0,true))
      {
	cerr<<"Didn't have "<<reagent[shift]<<endl;
	event->scroll->display_string("\nNo reagents.\n"); // FIXME correct text
	return false;
      }
      cerr<<"Ok, has "<<reagent[shift]<<endl;
    }
  }
    
  /* TODO check all pre-requisites before continue */
  // 'spell failed' because of bad luck
  // anything else?
  
  // consume the reagents and magic points; we checked so they must be there.
  caster->set_magic(caster->get_magic()-spell_level); // add a MAX (0, here?
  
  for (uint8 shift=0;shift<8;shift++) 
  {
    if (1<<shift&spell[index]->reagents) {
      // FIXME Although we just checked, maybe something is messed up, so we
      // should probably check that we're not passing NULL to delete_obj
      Obj *obj= caster->inventory_get_object(obj_n_reagent[shift]);
      Obj *container=caster->inventory_get_obj_container(obj,NULL); 
      {
	caster->inventory_del_object(obj_n_reagent[shift],1,0,container);
      }
    }
  }
  

  if (!process_script(spell[index]->script,true))
  { 
    cerr<<"script failed to parse"<<endl;
  }

  return true;
}

bool Magic::next_token(char *token, size_t *IP, char *script)
{
  for (size_t index=0; index<MAX_TOKEN_LENGTH; index++)
  {
    if (*IP>strlen(script)) return false; // end of script reached
    token[index]=script[*IP];
    *IP+=1;
    if (token[index]==' ')
    {
      token[index]='\0'; // got token, now skip excess spaces
      for (;*IP<strlen(script) && script[*IP]==' ';*IP+=1); 
      return true;
    };
    if (*IP>=strlen(script)) 
    { 
      if (index<MAX_TOKEN_LENGTH) {
       token[index+1]='\0'; // last token
       return true;
      }
      else
      {
	return false; // token too long
      }
    }
  }
  return false; // token too long
}

bool Magic::call(char *function)
{
  /* look for support function to call*/ 
  // FIXME iterate of over proper function table here
  // (generated with ftbl.sh)
if (!strcmp(function,"add")) { return function_add(); }
if (!strcmp(function,"add_hp")) { return function_add_hp(); }
if (!strcmp(function,"add_mp")) { return function_add_mp(); }
if (!strcmp(function,"colon")) { return function_colon(); }
if (!strcmp(function,"define_spell")) { return function_define_spell(); }
if (!strcmp(function,"delete_obj"))  { return function_delete_obj() ; }
if (!strcmp(function,"display")) { return function_display(); }
if (!strcmp(function,"drop")) { return function_drop(); }
if (!strcmp(function,"dup")) { return function_dup(); }
if (!strcmp(function,"give_obj")) { return function_give_obj(); }
if (!strcmp(function,"input")) { return function_input(); }
if (!strcmp(function,"inv_to_spell")) { return function_inv_to_spell(); }
if (!strcmp(function,"join")) { return function_join(); }
if (!strcmp(function,"load")) { return function_load(); }
if (!strcmp(function,"mod")) { return function_mod(); }
if (!strcmp(function,"mul")) { return function_mul(); }
if (!strcmp(function,"new_obj")) { return function_new_obj(); }
if (!strcmp(function,"newline")) { return function_newline(); }
if (!strcmp(function,"null")) { return function_null(); }
if (!strcmp(function,"over")) { return function_over(); }
if (!strcmp(function,"random")) { return function_random(); }
if (!strcmp(function,"save_spell")) { return function_save_spell(); }
if (!strcmp(function,"space")) { return function_space(); }
if (!strcmp(function,"strcmp")) { return function_strcmp(); }
if (!strcmp(function,"string2npc")) { return function_string2npc(); }
if (!strcmp(function,"sub")) { return function_sub(); }
if (!strcmp(function,"swap")) { return function_swap(); }
if (!strcmp(function,"template")) { return function_template(); }
if (!strcmp(function,"underscore")) { return function_underscore(); }
  return false; // unknown function
}

bool Magic::jump(char *label,size_t *OIP,char * script)
{ 
  size_t IP=*OIP; 
  char token[MAX_TOKEN_LENGTH];
  while (next_token(token,&IP,script)) 
  {
    if (token[0]==':' && !strcmp(token+1,label) ) {*OIP=IP;return true;};
  }
  // if we got here, we didn't find the label yet, so rewind the script
  IP=0;
  while (next_token(token,&IP,script)) 
  { 
    if (token[0]==':' && !strcmp(token+1,label) ) {*OIP=IP;return true;}; 
  }
  return false; // label not found; 
}

bool Magic::process_script(char * a_script, bool from_start) 
{
  if (from_start) {
   IP=0;
  }
  char script[MAX_SCRIPT_LENGTH+1];
  char token[MAX_TOKEN_LENGTH+1];  //+1 for teminating \0
  
  strncpy(script,a_script,MAX_SCRIPT_LENGTH);
  stack->empty();

  while (1){
    if (!next_token(token,&IP,script)) 
    {
      return false; // script failed to parse.
    }
    if (token[0]==':') {
      // label, ignore
      continue;
    }
    if (token[0]=='_') {
      // handle primitive command
      if (!strcmp(token,"_end")) 
      {
	break; // and we're done
      }
      if (!strcmp(token,"_jmp"))
      { 
	if (jump(stack->pop(),&IP,script)) 
	{ continue; } else { return false; /* label not found */ }
      }
      if (!strcmp(token,"_jmptrue"))
      { 
        if (atoi(stack->pop()))
	{
	  if (jump(stack->pop(),&IP,script)) 
	  { continue; } else { return false; /* label not found */ }
	} else
	{
	  stack->pop(); // drop unused label
	}

      }
      if (!strcmp(token,"_jmpfalse"))
      { 
        if (!atoi(stack->pop()))
	{
	  if (jump(stack->pop(),&IP,script)) 
	  { continue; } else { return false; /* label not found */ }
	} else
	{
	  stack->pop(); // drop unused label
	}
      }
      if (!strcmp(token,"_call"))
      {
	call(stack->pop());
	continue;
      }
      // unknown command, strip _ and treat as call to that function:
      call(token+1);
      continue;
    } 
    // must be an argument, push it on the stack
    stack->push(token);
    continue;
  }
  return true; // script successfully parsed.
}

// Support functions for spells
//

bool Magic::function_string2npc()
{
  /*
   * Stack effect: victim -- *npc "success" OR "failure"
   * valid victims are 'caster' or an npc number.
   */
  char token[MAX_TOKEN_LENGTH+1];
  strncpy(token,stack->pop(),MAX_TOKEN_LENGTH); 
  
  if (!strcmp(token,"caster")) 
  {
    stack->pushptr(event->player->get_actor());
    stack->push("success");
  }

  if (!strcmp(token,"target")) 
  {
    stack->pushptr(target_actor);
    stack->push("success");
  }

  
    stack->push("failure");
    return true;
}

bool Magic::function_underscore()
{
  /*
   * Stack effect: an underscore is put on the stack
   */
  stack->push("_");
  return true;
}

bool Magic::function_colon()
{
  /*
   * Stack effect: an underscore is put on the stack
   */
  stack->push(":");
  return true;
}

bool Magic::function_null()
{
  /*
   * Stack effect: an empty is put on the stack
   */
  stack->push("");
  return true;
}

bool Magic::function_space()
{
  /*
   * Stack effect: a space is put on the stack
   */
  stack->push(" ");
  return true;
}

bool Magic::function_newline()
{
  /*
   * Stack effect: a newline is put on the stack
   */
  stack->push("\n");
  return true;
}

bool Magic::function_join()
{
  /*
   * Stack effect: 
   * joining string popped
   * delimiter popped. 
   * all arguments not equal to delimiter popped 
   * delimiter popped
   * string of arguments joined by joiner pushed.
   */
  char buffer[MAX_SCRIPT_LENGTH+1]; buffer[0]='\0';
  char buffer2[MAX_SCRIPT_LENGTH+1]; buffer2[0]='\0';
  char joiner[MAX_TOKEN_LENGTH+1]; 
  char delimiter[MAX_TOKEN_LENGTH+1];
  char * popped;
  
  strncpy(joiner,stack->pop(),MAX_TOKEN_LENGTH);
  strncpy(delimiter,stack->pop(),MAX_TOKEN_LENGTH);
  strncpy(buffer,stack->pop(),MAX_SCRIPT_LENGTH);
  
  if (!strcmp(delimiter,buffer)) // nothing to join;
  {
    stack->push("");
    return true;
  }
  
  for (popped=stack->pop();strcmp(delimiter,popped);popped=stack->pop())
  {
    strncpy(buffer2,buffer,MAX_SCRIPT_LENGTH);
    strncpy(buffer,popped,MAX_SCRIPT_LENGTH);
    strncat(buffer,joiner,MAX_SCRIPT_LENGTH);
    strncat(buffer,buffer2,MAX_SCRIPT_LENGTH);
  }
  stack->push(buffer);
  return true;
}
bool Magic::function_display()
{
  stack->push(" ");
  function_join();
  event->scroll->display_string(stack->pop());
  return true;
};

bool Magic::function_random()
{ 
  /*
   * Stack effect: 1 value popped, random number in range [0,value> pushed
   */
  char buffer[MAX_TOKEN_LENGTH+1];
  snprintf(buffer,MAX_TOKEN_LENGTH,"%d",(int)(rand()%(atoi(stack->pop()))));
  stack->push(buffer);
  return true;
};
bool Magic::function_mod()
{
  /*
   * Stack effect: 2 values popped, modulus of them pushed
   */
  char buffer[MAX_TOKEN_LENGTH+1];
  snprintf(buffer,MAX_TOKEN_LENGTH,"%d",(atoi(stack->pop())%atoi(stack->pop())));
  stack->push(buffer);
  return true;
};
bool Magic::function_mul()
{
  /*
   * Stack effect: 2 values popped, product of them pushed
   */
  char buffer[MAX_TOKEN_LENGTH+1];
  snprintf(buffer,MAX_TOKEN_LENGTH,"%ld",(long int)(atoi(stack->pop())*atoi(stack->pop())));
  stack->push(buffer);
  return true;
};
bool Magic::function_add()
{
  /*
   * Stack effect: 2 values popped, sum of them pushed
   */
  char buffer[MAX_TOKEN_LENGTH+1];
  snprintf(buffer,MAX_TOKEN_LENGTH,"%d",(atoi(stack->pop())+atoi(stack->pop())));
  stack->push(buffer);
  return true;
};
bool Magic::function_sub()
{
  /*
   * Stack effect: 2 values popped, difference of them pushed
   */
  char buffer[MAX_TOKEN_LENGTH+1];
  snprintf(buffer,MAX_TOKEN_LENGTH,"%d",(atoi(stack->pop())-atoi(stack->pop())));
  stack->push(buffer);
  return true;
};
bool Magic::function_load()
{
  /*
   * Stack effect: pushes success or (TODO) failure 
   */
  read_spell_list(); 
  stack->push("success");
  return true;
};

bool Magic::function_new_obj()
{
 /*
  * Stack effect: pop object number, quantity, quality, frame number
  * push pointer to new object
  */
  Obj *obj=new_obj(atoi(stack->pop()), 0, 0, 0, 0);
  obj->qty=atoi(stack->pop());
  obj->quality=atoi(stack->pop());
  obj->frame_n=atoi(stack->pop());
  stack->pushptr(obj);
  return true;
}
bool Magic::function_define_spell()
{
 /*
  * Stack effect: pop index,name,invocation,script,reagents;
  */
  int index=atoi(stack->pop());
  if (spell[index]) {delete spell[index];}
  spell[index]=new Spell(stack->pop(),stack->pop(),stack->pop(),atoi(stack->pop()));
  return true;
}

bool Magic::function_save_spell()
{
 /*
  * Stack effect: pop index
  */
  int index=atoi(stack->pop());
  if (spell[index]) {
    // TODO open a proper file, and seek it.
    cout<<"##"<<"Nuvie Spell"<<endl;
    cout<<"# "<<index<<endl;
    cout<<"name: "<<spell[index]->name<<endl;
    cout<<"invocaton: "<<spell[index]->invocation<<endl;
    cout<<"reagents: "<<spell[index]->reagents<<endl;
    cout<<"script: "<<spell[index]->script<<endl;
    cout<<"~"<<endl;
  }
  return true;
}

bool Magic::function_strcmp()
{
 /*
  * Stack effect: pop str1, str2
  * push: -1, 0 or 1 as per strcmp(str1,str2)
  */
  char buffer[MAX_TOKEN_LENGTH+1];
  snprintf(buffer,MAX_TOKEN_LENGTH,"%d",strcmp(stack->pop(),stack->pop()));
  stack->push(buffer);
  return true;
}
bool Magic::function_inv_to_spell()
{
 /*
  * Stack effect: pop invocation
  * push index of this spell and "success" or "failure"
  */
  uint16 index;
  char buf[MAX_TOKEN_LENGTH+1];
  strncpy(buf,stack->pop(),MAX_TOKEN_LENGTH);
  for (index=0;index<256;index++) 
  {
    if (!strcmp(spell[index]->invocation,buf)) {
      snprintf(buf,MAX_TOKEN_LENGTH,"%d",index);
      stack->push(buf);
      stack->push("success");
      return true;
    }
  }
  stack->push("failure");
  return true;
}
bool Magic::function_give_obj()
{
 /*
  * Stack effect: pop destination, pop object pointer
  * push: "success"  or  object "failure"
  */
  char token[MAX_TOKEN_LENGTH+1];
  strncpy(token,stack->pop(),MAX_TOKEN_LENGTH); 
 
  if (strcmp(token,"caster")) // only caster is valid dest for now
  {
    stack->push("failure");
    return true;
  }
  Actor *actor=event->player->get_actor();
  Obj *obj=(Obj *) stack->popptr();

  float weight=event->obj_manager->get_obj_weight(obj, OBJ_WEIGHT_EXCLUDE_CONTAINER_ITEMS);
  if(actor->can_carry_weight(weight)) {
    event->obj_manager->remove_obj(obj); //remove object from map.
    actor->inventory_add_object(obj);
    stack->push("success");
    return true;
  };
  stack->pushptr(obj);
  stack->push("failure");
  return true;
}

bool Magic::function_delete_obj() 
{
 /*
  * Stack effect: pop object pointer
  */
    Obj *obj=(Obj *) stack->popptr();
    delete_obj(obj);
  return true;
}

bool Magic::function_drop()
{
 /*
  * Stack effect: pops one item from the stack
  */
  stack->pop();
  return true;
}
bool Magic::function_dup()
{
 /*
  * Stack effect: duplicates the top item on the stack
  */
  stack->dup();
  return true;
}

bool Magic::function_swap()
{
 /*
  * Stack effect: swaps top two items on the stack
  */
  stack->swap();
  return true;
}
bool Magic::function_over()
{
 /*
  * Stack effect: copies second item on stack to top.
  */
  stack->over();
  return true;
}

bool Magic::function_input()
{
 /*
  * Stack effect: pushes string entered by user.
  */
//  event->scroll->set_input_mode(true);
//  stack->push(event->scroll->get_input().c_str()); 
  // FIXME don't read from stdin, but use MsgScroll;
  char str[MAX_SCRIPT_LENGTH+1];
  cin.get(str,MAX_SCRIPT_LENGTH);
  stack->push(str); 
  return true;
}

bool Magic::function_add_hp()
{
 /*
  * Stack effect: amount victim --
  */

//adds up to amount hp to the victim, respecting max hp
//  char victim_name[MAX_TOKEN_LENGTH+1];
  
  function_string2npc();
  if (strcmp("failure",stack->pop()))
  {
    stack->pop(); //failed to resolve victim, so drop amount.
    return true;
  }
  
  Actor *victim=(Actor*) stack->popptr();
  victim->set_hp(MAX((victim->get_hp()+atoi(stack->pop())),victim->get_maxhp()));

  
  return true;
}
  
bool Magic::function_add_mp()
{
 /*
  * Stack effect: 
  */
//adds up to amount mp to the victim, respecting max mp
//
//  char victim_name[MAX_TOKEN_LENGTH+1];
  
  function_string2npc();
  if (strcmp("failure",stack->pop()))
  {
    stack->pop(); //failed to resolve victim, so drop amount.
    return true;
  }
  
  Actor *victim=(Actor*) stack->popptr();
// FIXME Nuvie doesn't seem to keep track of max mp yet!
  //victim->set_magic(MAX((victim->get_magic()+stack->pop()),victim->get_maxmagic()));
  victim->set_magic(victim->get_magic()+atoi(stack->pop()));

  
  return true;
}

bool Magic::function_template()
{
 /*
  * Stack effect: 
  */
  return true;
}
