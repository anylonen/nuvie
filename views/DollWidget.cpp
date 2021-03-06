/*
 *  DollWidget.cpp
 *  Nuvie
 *
 *  Created by Eric Fry on Mon Sep 01 2003.
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
#include <cassert>
// FIX need to subclass this class for U6, MD & SE
#include "Configuration.h"
#include "nuvieDefs.h"
#include "U6misc.h"
#include "U6Lib_n.h"
#include "U6Shape.h"

#include "MsgScroll.h"
#include "Event.h"

#include "Actor.h"
#include "ActorManager.h"
#include "GamePalette.h"
#include "DollWidget.h"
#include "CommandBar.h"
#include "MapWindow.h"
#include "Player.h"
#include "ViewManager.h"
#include "NuvieBmpFile.h"

#define USE_BUTTON 1 /* FIXME: put this in a common location */
#define ACTION_BUTTON 3
#define DRAG_BUTTON 1

static SDL_Rect item_hit_rects[8] = { {24, 0,16,16},   // ACTOR_HEAD
                                     { 0, 8,16,16},   // ACTOR_NECK
                                     {48, 8,16,16},   // ACTOR_BODY
                                     { 0,24,16,16},   // ACTOR_ARM
                                     {48,24,16,16},   // ACTOR_ARM_2
                                     { 0,40,16,16},   // ACTOR_HAND
                                     {48,40,16,16},   // ACTOR_HAND_2
                                     {24,48,16,16} }; // ACTOR_FOOT

static const unsigned char gump_blocked_tile_data[] = {
	170,170,170,170,170,170,170,170,170,170,170,170,170,170,170,170,
	170,170,170,170,170,170,170,170,170,170,170,170,170,170,170,170,
	170,170,170,170,64,178,55,54,54,55,178,64,170,170,170,170,
	170,170,170,64,55,168,64,170,170,64,168,55,64,170,170,170,
	170,170,64,55,64,170,170,170,170,170,12,12,55,64,170,170,
	170,170,178,130,170,170,170,170,170,12,12,12,168,178,170,170,
	170,170,55,64,170,170,170,170,12,12,12,170,64,55,170,170,
	170,170,54,170,170,170,170,12,12,12,170,170,170,54,170,170,
	170,170,54,170,170,170,12,12,12,170,170,170,170,54,170,170,
	170,170,55,64,170,12,12,12,170,170,170,170,64,55,170,170,
	170,170,178,168,12,12,12,170,170,170,170,170,168,178,170,170,
	170,170,64,55,12,12,170,170,170,170,170,64,55,64,170,170,
	170,170,170,64,55,168,64,170,170,64,168,55,64,170,170,170,
	170,170,170,170,64,178,55,54,54,55,178,64,170,170,170,170,
	170,170,170,170,170,170,170,170,170,170,170,170,170,170,170,170,
	170,170,170,170,170,170,170,170,170,170,170,170,170,170,170,170
	};

static const unsigned char gump_empty_tile_data[] = {
	170,170,170,170,170,170,170,170,170,170,170,170,170,170,170,170,
	170,170,170,170,170,170,170,170,170,170,170,170,170,170,170,170,
	170,170,170,170,64,178,55,54,54,55,178,64,170,170,170,170,
	170,170,170,64,55,168,64,170,170,64,168,55,64,170,170,170,
	170,170,64,55,64,170,170,170,170,170,170,64,55,64,170,170,
	170,170,178,130,170,170,170,170,170,170,170,170,168,178,170,170,
	170,170,55,64,170,170,170,170,170,170,170,170,64,55,170,170,
	170,170,54,170,170,170,170,170,170,170,170,170,170,54,170,170,
	170,170,54,170,170,170,170,170,170,170,170,170,170,54,170,170,
	170,170,55,64,170,170,170,170,170,170,170,170,64,55,170,170,
	170,170,178,168,170,170,170,170,170,170,170,170,168,178,170,170,
	170,170,64,55,64,170,170,170,170,170,170,64,55,64,170,170,
	170,170,170,64,55,168,64,170,170,64,168,55,64,170,170,170,
	170,170,170,170,64,178,55,54,54,55,178,64,170,170,170,170,
	170,170,170,170,170,170,170,170,170,170,170,170,170,170,170,170,
	170,170,170,170,170,170,170,170,170,170,170,170,170,170,170,170
	};

DollWidget::DollWidget(Configuration *cfg, GUI_CallBack *callback): GUI_Widget(NULL, 0, 0, 0, 0)
{
 config = cfg;
 callback_object = callback;

 actor = NULL; tile_manager = NULL;
 selected_obj = NULL; obj_manager = NULL;
 unready_obj = NULL; empty_tile = NULL; blocked_tile = NULL;
 
 bg_color = Game::get_game()->get_palette()->get_bg_color();
 need_to_free_tiles = false;

 use_new_dolls = true; old_use_new_dolls = true;
 actor_doll = NULL; doll_bg = NULL;
 md_doll_shp = NULL;
}

DollWidget::~DollWidget()
{
	if(need_to_free_tiles)
	{
		if(blocked_tile)
			delete blocked_tile;
		if(empty_tile)
			delete empty_tile;
	}
	free_doll_shapes();
}

bool DollWidget::init(Actor *a, uint16 x, uint16 y, TileManager *tm, ObjManager *om, bool in_portrait_view)
{
 tile_manager = tm;
 obj_manager = om;
 is_in_portrait_view = in_portrait_view;
 if(!Game::get_game()->is_new_style() || is_in_portrait_view)
 {
 switch(Game::get_game()->get_game_type())
 {
 case NUVIE_GAME_U6 : blocked_tile = tile_manager->get_tile(TILE_U6_BLOCKED_EQUIP);
                      empty_tile = tile_manager->get_tile(TILE_U6_EQUIP);
                      break;

 case NUVIE_GAME_SE : blocked_tile = tile_manager->get_tile(TILE_SE_BLOCKED_EQUIP);
                      empty_tile = tile_manager->get_tile(TILE_SE_EQUIP);
                      break;

 case NUVIE_GAME_MD : blocked_tile = tile_manager->get_tile(TILE_MD_BLOCKED_EQUIP); // FIXME: different depending on npc
                      empty_tile = tile_manager->get_tile(TILE_MD_EQUIP); // FIXME: different depending on npc
                      break;
 }
 }
 else
 {
	 blocked_tile = new Tile();
	 memcpy(&blocked_tile->data, &gump_blocked_tile_data, 256);
	 empty_tile = new Tile();
	 memcpy(&empty_tile->data, &gump_empty_tile_data, 256);
	 need_to_free_tiles = true;
 }

 GUI_Widget::Init(NULL, x, y, 64, 64);

 set_actor(a);
 set_accept_mouseclick(true, USE_BUTTON); // accept [double]clicks from button1 (even if double-click disabled we need clicks)

 return true;
}

void DollWidget::free_doll_shapes()
{
	if(actor_doll) {
		SDL_FreeSurface(actor_doll);
		actor_doll = NULL;
	}
	if(doll_bg) {
		SDL_FreeSurface(doll_bg);
		doll_bg = NULL;
	}
	if(md_doll_shp) {
		delete md_doll_shp;
		md_doll_shp = NULL;
	}
}

void DollWidget::setColorKey(SDL_Surface *image)
{
	if(image) {
		Uint32 bg_color_key = SDL_MapRGB(image->format, 0xf1, 0x0f, 0xc4);
		SDL_SetColorKey(image, SDL_SRCCOLORKEY, bg_color_key);
	}
}

void DollWidget::set_actor(Actor *a)
{
 actor = a;
 if(!Game::get_game()->is_new_style()) { // needed so it can be changed in the menus
	 config->value(config_get_game_key(config) + "/use_new_dolls", use_new_dolls, false);
	if(old_use_new_dolls != use_new_dolls) {
		if(!use_new_dolls)
			free_doll_shapes();
		old_use_new_dolls = use_new_dolls;
	}
 }
 if(use_new_dolls) {
	free_doll_shapes();
	if(actor) {
		ViewManager *vm = Game::get_game()->get_view_manager();
		if(actor->is_avatar())
			actor_doll = vm->loadAvatarDollImage(actor_doll, true);
		else
			actor_doll = vm->loadCustomActorDollImage(actor_doll, actor->get_actor_num(), true);
		setColorKey(actor_doll);
		if(actor_doll) {
			std::string imagefile;
			build_path(vm->getDollDataDirString(), "orig_style", imagefile);
			build_path(imagefile, "doll_bg.bmp", imagefile);
			NuvieBmpFile bmp;
			doll_bg = bmp.getSdlSurface32(imagefile);
			if(doll_bg) {
				SDL_Rect dst;
				dst.w = 27;
				dst.h = 30;
				dst.x = 3;
				dst.y = 1;
				SDL_BlitSurface(actor_doll, NULL, doll_bg, &dst);
				setColorKey(doll_bg);
			}
		}
	}
 }
 else if(Game::get_game()->get_game_type() == NUVIE_GAME_MD)
 {
   load_md_doll_shp();
 }
 Redraw();
}

void DollWidget::load_md_doll_shp()
{
  if(actor==NULL)
  {
    return;
  }

  if(md_doll_shp)
    delete md_doll_shp;

  md_doll_shp = new U6Shape();
  U6Lib_n file;
  std::string filename;
  config_get_path(config,"mdinv.lzc",filename);
  file.open(filename,4,NUVIE_GAME_MD);
  uint8 num = actor->get_actor_num()+1;
  if(actor->is_avatar() && Game::get_game()->get_player()->get_gender() == 0)
  {
    num--;
  }
  unsigned char *temp_buf = file.get_item(num);
  if(temp_buf)
  {
    md_doll_shp->load(temp_buf + 8);
    free(temp_buf);
  }
  else
  {
    delete md_doll_shp;
    md_doll_shp = NULL;
  }
}

SDL_Rect *DollWidget::get_item_hit_rect(uint8 location)
{
    if(location < 8)
        return(&item_hit_rects[location]);
    return(NULL);
}


void DollWidget::Display(bool full_redraw)
{
 //if(full_redraw || update_display)
 // {
   update_display = false;

   if(actor != NULL)
     display_doll();
   screen->update(area.x, area.y, area.w, area.h);
//  }

}

inline void DollWidget::display_new_doll()
{
	if(doll_bg) {
		SDL_Rect dst;
		dst = area;
		dst.w = 33;
		dst.h = 33;
		dst.x += 15;
		dst.y += 15;
		SDL_BlitSurface(doll_bg, NULL, surface, &dst);
	}
}

inline void DollWidget::display_old_doll()
{
 Tile *tile;
 uint16 i,j;
	int tilenum = 368;
	if(Game::get_game()->get_game_type() == NUVIE_GAME_MD) // FIXME: different depending on npc - Also needs npc doll info code
		tilenum = 275;
	else if(Game::get_game()->get_game_type() == NUVIE_GAME_SE)
	{
		if(actor->get_obj_n() == 310 || actor->get_obj_n() == 311
		   || actor->get_obj_n() == 312)
			tilenum = 404;
		else if(actor->get_obj_n() == 318)
			tilenum = 408;
		else
			tilenum = 400;
	}
//	 screen->fill(bg_color, area.x, area.y, area.w, area.h); // should be taken care of by the main view
	 for(i=0;i<2;i++)
	 {
		 for(j=0;j<2;j++) // draw doll
		 {
			 tile = tile_manager->get_tile(tilenum+i*2+j);
			 screen->blit(area.x+16+j*16,area.y+16+i*16,tile->data,8,16,16,16,true);
		 }
	 }
	 if(md_doll_shp)
	 {
	   uint16 w,h;
	   md_doll_shp->get_size(&w, &h);
	   screen->blit(area.x+20,area.y+18,md_doll_shp->get_data(),8,w,h,w,true);
	 }
}

inline void DollWidget::display_doll()
{
 if(!Game::get_game()->is_new_style() || is_in_portrait_view) {
	if(use_new_dolls)
		display_new_doll();
	else
		display_old_doll();
 }
 display_readied_object(ACTOR_NECK, area.x, (area.y+8) + 0 * 16, actor, empty_tile);
 display_readied_object(ACTOR_BODY, area.x+3*16, (area.y+8) + 0 * 16, actor, empty_tile);

 display_readied_object(ACTOR_ARM, area.x, (area.y+8) + 1 * 16, actor, empty_tile);
 display_readied_object(ACTOR_ARM_2, area.x+3*16, (area.y+8) + 1 * 16, actor, actor->is_double_handed_obj_readied() ? blocked_tile : empty_tile);

 display_readied_object(ACTOR_HAND, area.x, (area.y+8) + 2 * 16, actor, empty_tile);
 display_readied_object(ACTOR_HAND_2, area.x+3*16, (area.y+8) + 2 * 16, actor, empty_tile);

 display_readied_object(ACTOR_HEAD, area.x+16+8, area.y, actor, empty_tile);
 display_readied_object(ACTOR_FOOT, area.x+16+8, area.y+3*16, actor, empty_tile);

 return;
}

inline void DollWidget::display_readied_object(uint8 location, uint16 x, uint16 y, Actor *actor, Tile *empty_tile)
{
 Obj *obj;
 Tile *tile;

 obj = actor->inventory_get_readied_object(location);

 if(obj)
   tile = tile_manager->get_tile(obj_manager->get_obj_tile_num(obj->obj_n)+obj->frame_n);
 else
   tile = empty_tile;

 screen->blit(x,y,tile->data,8,16,16,16,true);

 return;
}

// when no action is pending the Use button may be used to start dragging,
// otherwise it has the same effect as ENTER (using InventoryView's callback)
GUI_status DollWidget::MouseDown(int x, int y, int button)
{
 Event *event = Game::get_game()->get_event();
 uint8 location;
 Obj *obj;
 x -= area.x;
 y -= area.y;

 CommandBar *command_bar = Game::get_game()->get_command_bar();
 if(button == ACTION_BUTTON && event->get_mode() == MOVE_MODE
    && command_bar->get_selected_action() > 0) // Exclude attack mode too
 {
	if(command_bar->try_selected_action() == false) // start new action
		return GUI_YUM; // false if new event doesn't need target
 }

 if(actor && selected_obj == NULL && (button == USE_BUTTON || button == ACTION_BUTTON || button == DRAG_BUTTON))
   {
    for(location=0;location<8;location++)
      {
       if(HitRect(x,y,item_hit_rects[location]))  // FIXME: duplicating code in InventoryWidget
         {
          DEBUG(0,LEVEL_DEBUGGING,"Hit %d\n",location);
          obj = actor->inventory_get_readied_object(location);
          if(button == ACTION_BUTTON && command_bar->get_selected_action() > 0
             && event->get_mode()== INPUT_MODE)
          {
        	  if(obj)
        	  {
        		  event->select_obj(obj, actor);
                  return GUI_YUM;
        	  }
        	  else
        	  {
        		  // has not found a target yet
        	       Game::get_game()->get_scroll()->display_string("nothing!\n");
        	       event->endAction(true);
        	       event->set_mode(MOVE_MODE);
        	  }
              return GUI_PASS;
          }
          if(obj)
           {

             if((event->get_mode()==MOVE_MODE || event->get_mode()==EQUIP_MODE)
                && button==DRAG_BUTTON)
                 selected_obj = obj; // start dragging
             else // send to View
                 callback_object->callback(INVSELECT_CB, this, obj);
           }
          return GUI_YUM;
         }
      }
   }

 return GUI_PASS;
}

// un-ready selected item
GUI_status DollWidget::MouseUp(int x,int y,int button)
{
 Event *event = Game::get_game()->get_event();

 // only act now if double-click is disabled
 if(selected_obj && !Game::get_game()->get_map_window()->is_doubleclick_enabled())
   {
    event->unready(selected_obj);
    Redraw();
    unready_obj = NULL;
   }
 else if(selected_obj)
   {
    wait_for_mouseclick(USE_BUTTON);
    unready_obj = selected_obj;
   }

 selected_obj = NULL;
 return GUI_PASS;
}

GUI_status DollWidget::MouseClick(int x, int y, int button)
{
    return(MouseUp(x, y, button));
}

GUI_status DollWidget::MouseMotion(int x,int y,Uint8 state)
{
 Tile *tile;

 if(selected_obj && !dragging && Game::get_game()->is_dragging_enabled())
   {
    dragging = true;
    tile = tile_manager->get_tile(obj_manager->get_obj_tile_num(selected_obj->obj_n)+selected_obj->frame_n);
    return gui_drag_manager->start_drag(this, GUI_DRAG_OBJ, selected_obj, tile->data, 16, 16, 8);
   }

	return GUI_PASS;
}

void DollWidget::drag_drop_success(int x, int y, int message, void *data)
{
 DEBUG(0,LEVEL_DEBUGGING,"DollWidget::drag_drop_success()\n");
 dragging = false;
// handled by drop target
// actor->remove_readied_object(selected_obj);
// actor->inventory_remove_obj(selected_obj);
 selected_obj = NULL;
 Redraw();
}

void DollWidget::drag_drop_failed(int x, int y, int message, void *data)
{
 DEBUG(0,LEVEL_DEBUGGING,"DollWidget::drag_drop_failed()\n");
 dragging = false;
 selected_obj = NULL;
}

bool DollWidget::drag_accept_drop(int x, int y, int message, void *data)
{
    DEBUG(0,LEVEL_DEBUGGING,"DollWidget::drag_accept_drop()\n");
    if(message == GUI_DRAG_OBJ)
    {
        Obj *obj = (Obj*)data;
        if(obj->is_readied() && obj->get_actor_holding_obj() == actor)
        {
            // FIXME: need to detect ready location so player can switch hands
            DEBUG(0,LEVEL_WARNING,"DollWidget: Object already equipped!\n");
            return false;
        }
        if(obj->get_actor_holding_obj() != actor)
        {
            if(obj->is_in_inventory())
            {
                Event *event = Game::get_game()->get_event();
                event->display_move_text(actor, obj);
                if(event->can_move_obj_between_actors(obj, obj->get_actor_holding_obj(), actor, false))
                {
                    Game::get_game()->get_player()->subtract_movement_points(3);
                    DEBUG(0,LEVEL_DEBUGGING,"Drop Accepted\n");
                    return true;
                }
            }
        }
        if(obj->get_actor_holding_obj() == actor
           || Game::get_game()->get_map_window()->can_get_obj(actor, obj))
        {
            DEBUG(0,LEVEL_DEBUGGING,"Drop Accepted\n");
            return true;
        }
        else
        {
            DEBUG(0,LEVEL_WARNING,"DollWidget: Must be holding object!\n");
            return false;
        }
    }

    DEBUG(0,LEVEL_DEBUGGING,"Drop Refused\n");
    return false;
}

void DollWidget::drag_perform_drop(int x, int y, int message, void *data)
{
 DEBUG(0,LEVEL_DEBUGGING,"DollWidget::drag_perform_drop()\n");
 Obj *obj;

 x -= area.x;
 y -= area.y;

 if(message == GUI_DRAG_OBJ)
   {
    DEBUG(0,LEVEL_DEBUGGING,"Ready item.\n");
    obj = (Obj *)data;

    bool can_equip = true;
    if(!obj->is_in_inventory()) // get
      {
       // event->newAction(GET_MODE);
       Game::get_game()->get_scroll()->display_string("Get-");
       can_equip = Game::get_game()->get_event()->perform_get(obj, NULL, actor);
//       if(!can_equip)
//       {
//        assert(!(obj->status & OBJ_STATUS_IN_CONTAINER));
//        obj_manager->add_obj(obj); // add back to map
//       }
      }
    else
        obj_manager->moveto_inventory(obj, actor);
    if(can_equip) // ready
      {
       assert(!obj->is_readied());
       Game::get_game()->get_event()->ready(obj, actor);
      }
    Redraw();
   }

 return;
}

void DollWidget::drag_draw(int x, int y, int message, void* data)
{
	Tile* tile;

	if (!selected_obj)
		return;

	tile = tile_manager->get_tile(obj_manager->get_obj_tile_num (selected_obj) + selected_obj->frame_n);

	int	nx = x - 8;
	int	ny = y - 8;

	if (nx + 16 >= screen->get_width())
		nx = screen->get_width()-17;
	else if (nx < 0)
		nx = 0;

	if (ny + 16 >= screen->get_height())
		ny = screen->get_height()-17;
	else if (ny < 0)
		ny = 0;

	screen->blit(nx, ny, tile->data, 8, 16, 16, 16, true);
	screen->update(nx, ny, 16, 16);
}


/* Use object.
 */
GUI_status DollWidget::MouseDouble(int x, int y, int button)
{
    // we have to check if double-clicks are allowed here, since we use single-clicks
    if(!Game::get_game()->get_map_window()->is_doubleclick_enabled())
        return(GUI_PASS);
    Event *event = Game::get_game()->get_event();
    Obj *obj = selected_obj;

    unready_obj = NULL;
    selected_obj = NULL;

    if(!(actor && obj))
        return(GUI_YUM);

    if(event->newAction(USE_MODE))
        event->select_obj(obj);
    return(GUI_YUM);
}

// change container, ready/unready object, activate arrows
GUI_status DollWidget::MouseDelayed(int x, int y, int button)
{
    Event *event = Game::get_game()->get_event();
    if(unready_obj)
    {
        event->unready(unready_obj);
        Redraw();
        unready_obj = NULL;
    }
    return GUI_PASS;
}
