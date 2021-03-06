/*
 *  PortraitView.cpp
 *  Nuvie
 *
 *  Created by Eric Fry on Tue May 20 2003.
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

#include <string>

#include "nuvieDefs.h"

#include "Configuration.h"
#include "U6misc.h"
#include "U6Lib_n.h"
#include "U6Shape.h"

#include "Game.h"
#include "Actor.h"
#include "Portrait.h"
#include "Font.h"
#include "ViewManager.h"
#include "MsgScroll.h"
#include "GUI.h"
#include "DollWidget.h"
#include "PortraitView.h"
#include "SunMoonStripWidget.h"


PortraitView::PortraitView(Configuration *cfg) : View(cfg)
{
 portrait_data = NULL; portrait = NULL;
 bg_data = NULL;
 name_string = new string; show_cursor = false;
 doll_widget = NULL; waiting = false; display_doll = false;
 cur_actor_num = 0;
 gametype = get_game_type(cfg);

 //FIXME: Portraits in SE/MD are different size than in U6! 79x85 76x83
 switch(gametype)
 {
   case NUVIE_GAME_U6: portrait_width=56; portrait_height=64; break;
   case NUVIE_GAME_SE: portrait_width=79; portrait_height=85; break;
   case NUVIE_GAME_MD: portrait_width=76; portrait_height=83; break;
 }
}

PortraitView::~PortraitView()
{
 if(portrait_data != NULL)
   free(portrait_data);

 if(bg_data != NULL)
   delete bg_data;

 delete name_string;
}

bool PortraitView::init(uint16 x, uint16 y, Font *f, Party *p, Player *player, TileManager *tm, ObjManager *om, Portrait *port)
{
 View::init(x,y,f,p,tm,om);

 portrait = port;

 doll_widget = new DollWidget(config, this);
 doll_widget->init(NULL, 0, 16, tile_manager, obj_manager, true);

 AddWidget(doll_widget);
 doll_widget->Hide();
 
 if(gametype == NUVIE_GAME_U6)
 {
   SunMoonStripWidget *sun_moon_widget = new SunMoonStripWidget(player, tile_manager);
   sun_moon_widget->init(-8, -2);
   AddWidget(sun_moon_widget);
 }
 else if(gametype == NUVIE_GAME_MD)
 {
   load_background("mdscreen.lzc", 1);
 }
 else if(gametype == NUVIE_GAME_SE)
 {
   load_background("bkgrnd.lzc", 0);
 }

 return true;
}


void PortraitView::load_background(const char *f, uint8 lib_offset)
{
  U6Lib_n file;
  bg_data = new U6Shape();
  std::string path;
  config_get_path(config,f,path);
  file.open(path,4,gametype);
  unsigned char *temp_buf = file.get_item(lib_offset);
  bg_data->load(temp_buf + 8);
  free(temp_buf);
}

void PortraitView::Display(bool full_redraw)
{

 if(Game::get_game()->is_new_style() || Game::get_game()->is_original_plus_full_map())
   screen->fill(bg_color, area.x, area.y, area.w, area.h);
 if(portrait_data != NULL/* && (full_redraw || update_display)*/)
  {
   update_display = false;
   if(gametype == NUVIE_GAME_U6)
   {
     if(display_doll)
       screen->blit(area.x+72,area.y+16,portrait_data,8,portrait_width,portrait_height,portrait_width,false);
     else
       screen->blit(area.x+(area.w-portrait_width)/2,area.y+(area.h-portrait_height)/2,portrait_data,8,portrait_width,portrait_height,portrait_width,true);
     display_name(80);
   }
   else if(gametype == NUVIE_GAME_MD)
   {
     uint16 w,h;
     bg_data->get_size(&w, &h);
     screen->blit(area.x,area.y-2,bg_data->get_data(),8,w,h,w,true);

     screen->blit(area.x+(area.w-portrait_width)/2,area.y+6,portrait_data,8,portrait_width,portrait_height,portrait_width,true);
     display_name(100);
   }
   else if(gametype == NUVIE_GAME_SE)
   {
     uint16 w,h;
     bg_data->get_size(&w, &h);
     screen->blit(area.x,area.y,bg_data->get_data(),8,w,h,w,true);

     screen->blit(area.x+(area.w-portrait_width)/2+1,area.y+1,portrait_data,8,portrait_width,portrait_height,portrait_width,true);
     display_name(98);
   }
  }
  if(show_cursor && gametype == NUVIE_GAME_U6) // FIXME: should we be using scroll's drawCursor?
   {
    screen->fill(bg_color, area.x, area.y + area.h - 8, 8, 8);
    Game::get_game()->get_scroll()->drawCursor(area.x, area.y + area.h - 8);
   } 
   DisplayChildren(full_redraw);
   screen->update(area.x, area.y, area.w, area.h);
}

bool PortraitView::set_portrait(Actor *actor, const char *name)
{
 if(Game::get_game()->is_new_style())
   this->Show();
 cur_actor_num = actor->get_actor_num();
 int doll_x_offset = 0;

 if(portrait_data != NULL)
   free(portrait_data);

 portrait_data = portrait->get_portrait_data(actor);

 if(gametype == NUVIE_GAME_U6 && actor->has_readied_objects())
   {
    if(portrait_data == NULL)
      doll_x_offset = 34;
      
    doll_widget->MoveRelativeToParent(doll_x_offset, 16);

    display_doll = true;
    doll_widget->Show();
    doll_widget->set_actor(actor);
   }
 else
   {
    display_doll = false;
    doll_widget->Hide();
    doll_widget->set_actor(NULL);
    
    if(portrait_data == NULL)
      return false;
   }

 if(name == NULL)
   name = actor->get_name();
 if(name == NULL)
   name_string->assign("");  // just in case
 else
   name_string->assign(name);
 
 if(screen)
   screen->fill(bg_color, area.x, area.y, area.w, area.h);
   
 Redraw();
 return true;
}

void PortraitView::display_name(uint16 y_offset)
{
 const char *name;

 name = name_string->c_str();

 font->drawString(screen, name, area.x + (area.w - strlen(name) * 8) / 2, area.y+y_offset);

 return;
}


/* On any input return to previous status view if waiting.
 * Returns true if event was used.
 */
GUI_status PortraitView::HandleEvent(const SDL_Event *event)
{
    if(waiting
       && (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_KEYDOWN))
    {
        if(Game::get_game()->is_new_style())
            this->Hide();
        else // FIXME revert to previous status view
            Game::get_game()->get_view_manager()->set_inventory_mode();
        // Game::get_game()->get_scroll()->set_input_mode(false);
        Game::get_game()->get_scroll()->message("\n");
        set_waiting(false);
        return(GUI_YUM);
    }
    return(GUI_PASS);
}


/* Start/stop waiting for input to continue, and (for now) steal cursor from
 * MsgScroll.
 */
void PortraitView::set_waiting(bool state)
{
    if(state == true && display_doll == false && portrait_data == NULL) // don't wait for nothing
    {
        if(Game::get_game()->is_new_style())
            this->Hide();
        return;
    }
    waiting = state;
    set_show_cursor(waiting);
    Game::get_game()->get_scroll()->set_show_cursor(!waiting);
    Game::get_game()->get_gui()->lock_input(waiting ? this : NULL);
}
