/*
 *  PortraitViewGump.cpp
 *  Nuvie
 *
 *  Created by Eric Fry on Mon Apr 09 2012.
 *  Copyright (c) 2012. All rights reserved.
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
#include "U6misc.h"
#include "Event.h"
#include "GUI.h"
#include "GUI_button.h"

#include "Party.h"
#include "Actor.h"
#include "Portrait.h"
#include "ViewManager.h"

#include "ContainerWidgetGump.h"
#include "PortraitViewGump.h"
#include "Keys.h"


PortraitViewGump::PortraitViewGump(Configuration *cfg) : DraggableView(cfg)
{
	portrait = NULL;
	font = NULL; gump_button = NULL;
	portrait_data = NULL; actor = NULL;
	cursor_tile = NULL;
	show_cursor = true;
	cursor_pos = CURSOR_CHECK;
	cursor_xoff = 1; cursor_yoff = 67;
}

PortraitViewGump::~PortraitViewGump()
{
	if(font)
		delete font;
	if(portrait_data)
		free(portrait_data);
}

bool PortraitViewGump::init(Screen *tmp_screen, void *view_manager, uint16 x, uint16 y, Font *f, Party *p, TileManager *tm, ObjManager *om, Portrait *por, Actor *a)
{
	View::init(x,y,f,p,tm,om);

	SetRect(area.x, area.y, 188, 91);

	portrait = por;
	set_actor(a);

	std::string datadir = GUI::get_gui()->get_data_dir();
	std::string imagefile;
	std::string path;

	build_path(datadir, "images", path);
	datadir = path;
	build_path(datadir, "gumps", path);
	datadir = path;

	gump_button = loadButton(datadir, "gump", 0, 67);

	build_path(datadir, "portrait_bg.bmp", imagefile);
	bg_image = SDL_LoadBMP(imagefile.c_str());

	set_bg_color_key(0, 0x70, 0xfc);

	font = new GUI_Font(GUI_FONT_GUMP);
	font->SetColoring( 0x08, 0x08, 0x08, 0x80, 0x58, 0x30, 0x00, 0x00, 0x00);

	SDL_Surface *image, *image1;

	build_path(datadir, "left_arrow.bmp", imagefile);
	image = SDL_LoadBMP(imagefile.c_str());
	image1 = SDL_LoadBMP(imagefile.c_str());

	left_button = new GUI_Button(this, 23, 6, image, image1, this);
	this->AddWidget(left_button);

	build_path(datadir, "right_arrow.bmp", imagefile);
	image = SDL_LoadBMP(imagefile.c_str());
	image1 = SDL_LoadBMP(imagefile.c_str());

	right_button = new GUI_Button(this, 166, 6, image, image1, this);
	this->AddWidget(right_button);

	if(party->get_member_num(actor) < 0)
	{
		left_button->Hide();
		right_button->Hide();
	}
	cursor_tile = tile_manager->get_gump_cursor_tile();

	return true;
}

void PortraitViewGump::set_actor(Actor *a)
{
	actor = a;
	if(portrait_data)
		free(portrait_data);
	portrait_data = portrait->get_portrait_data(actor);
}

void PortraitViewGump::left_arrow()
{
	if(party->get_member_num(actor) < 0)
		return;
	uint8 party_mem_num = party->get_member_num(actor);
	if(party_mem_num > 0)
		party_mem_num--;
	else
		party_mem_num = party->get_party_size() - 1;

	set_actor(party->get_actor(party_mem_num));
}

void PortraitViewGump::right_arrow()
{
	if(party->get_member_num(actor) < 0)
		return;
	set_actor(party->get_actor((party->get_member_num(actor) + 1) % party->get_party_size()));
}

void PortraitViewGump::Display(bool full_redraw)
{
 char buf[6]; //xxxxx\n
 //display_level_text();
 //display_spell_list_text();
 SDL_Rect dst;
 dst = area;
 SDL_BlitSurface(bg_image, NULL, surface, &dst);

 DisplayChildren(full_redraw);
 screen->blit(area.x+25,area.y+17,portrait_data,8,portrait->get_portrait_width(),portrait->get_portrait_height(),portrait->get_portrait_width(),false);

 int w,h;
 w = font->get_center(actor->get_name(), 138);

 font->SetColoring( 0x08, 0x08, 0x08, 0x80, 0x58, 0x30, 0x00, 0x00, 0x00);

 font->TextOut(screen->get_sdl_surface(), area.x + 29 + w, area.y + 6, actor->get_name());

 snprintf(buf, 5, "%d", actor->get_strength());
 font->TextExtent(buf, &w, &h);
 font->TextOut(screen->get_sdl_surface(), area.x + 170 - w, area.y + 18, buf);

 snprintf(buf, 5, "%d", actor->get_dexterity());
 font->TextExtent(buf, &w, &h);
 font->TextOut(screen->get_sdl_surface(), area.x + 170 - w, area.y + 27, buf);

 snprintf(buf, 5, "%d", actor->get_intelligence());
 font->TextExtent(buf, &w, &h);
 font->TextOut(screen->get_sdl_surface(), area.x + 170 - w, area.y + 36, buf);

 font->SetColoring( 0x6c, 0x00, 0x00, 0xbc, 0x34, 0x00, 0x00, 0x00, 0x00);

 snprintf(buf, 5, "%d", actor->get_magic());
 font->TextExtent(buf, &w, &h);
 font->TextOut(screen->get_sdl_surface(), area.x + 142 - w, area.y + 55, buf);

 snprintf(buf, 5, "%d", actor->get_maxmagic());
 font->TextExtent(buf, &w, &h);
 font->TextOut(screen->get_sdl_surface(), area.x + 170 - w, area.y + 55, buf);

 font->SetColoring( 0x00, 0x3c, 0x70, 0x74, 0x74, 0x74, 0x00, 0x00, 0x00);

 snprintf(buf, 5, "%d", actor->get_hp());
 font->TextExtent(buf, &w, &h);
 font->TextOut(screen->get_sdl_surface(), area.x + 142 - w, area.y + 64, buf);

 snprintf(buf, 5, "%d", actor->get_maxhp());
 font->TextExtent(buf, &w, &h);
 font->TextOut(screen->get_sdl_surface(), area.x + 170 - w, area.y + 64, buf);

 font->SetColoring( 0xa8, 0x28, 0x00, 0xa8, 0x54, 0x00, 0x00, 0x00, 0x00);

 snprintf(buf, 5, "%d", actor->get_level());
 font->TextExtent(buf, &w, &h);
 font->TextOut(screen->get_sdl_surface(), area.x + 142 - w, area.y + 73, buf);

 snprintf(buf, 5, "%d", actor->get_exp());
 font->TextExtent(buf, &w, &h);
 font->TextOut(screen->get_sdl_surface(), area.x + 170 - w, area.y + 73, buf);

 if(show_cursor)
	screen->blit(area.x+cursor_xoff,area.y+cursor_yoff,(unsigned char *)cursor_tile->data,8,16,16,16,true);
 update_display = false;
 screen->update(area.x, area.y, area.w, area.h);


 return;
}

GUI_status PortraitViewGump::set_cursor_pos(gumpCursorPos pos)
{
	if(party->get_member_num(actor) < 0)
		return GUI_YUM;
	cursor_pos = pos;
	if(cursor_pos == CURSOR_CHECK) {
		cursor_xoff = 1; cursor_yoff = 67;
	} else if(cursor_pos == CURSOR_LEFT) {
		cursor_xoff = 18; cursor_yoff = 1;
	} else {
		cursor_xoff = 162; cursor_yoff = 1;
	}
	return GUI_YUM;
}

GUI_status PortraitViewGump::callback(uint16 msg, GUI_CallBack *caller, void *data)
{
	//close gump and return control to Magic class for clean up.
	if(caller == gump_button)
	{
		Game::get_game()->get_view_manager()->close_gump(this);
		return GUI_YUM;
	}
	else if(caller == left_button)
	{
		left_arrow();
	}
	else if(caller == right_button)
	{
		right_arrow();
	}

    return GUI_PASS;
}

GUI_status PortraitViewGump::KeyDown(SDL_keysym key)
{
//	I was checking for numlock but probably don't need to
	KeyBinder *keybinder = Game::get_game()->get_keybinder();
	ActionType a = keybinder->get_ActionType(key);

	switch(keybinder->GetActionKeyType(a)) {
		case SOUTH_WEST_KEY:
		case SOUTH_KEY:
			return set_cursor_pos(CURSOR_CHECK);
		case NORTH_KEY:
			if(cursor_pos == CURSOR_CHECK)
				return set_cursor_pos(CURSOR_LEFT);
			return GUI_YUM;
		case NORTH_WEST_KEY:
		case WEST_KEY:
			if(cursor_pos == CURSOR_RIGHT)
				return set_cursor_pos(CURSOR_LEFT);
			return set_cursor_pos(CURSOR_CHECK);
		case NORTH_EAST_KEY:
		case SOUTH_EAST_KEY:
		case EAST_KEY:
			if(cursor_pos == CURSOR_CHECK)
				return set_cursor_pos(CURSOR_LEFT);
			return set_cursor_pos(CURSOR_RIGHT);
		case DO_ACTION_KEY:
			if(cursor_pos == CURSOR_CHECK)
				Game::get_game()->get_view_manager()->close_gump(this);
			else if(cursor_pos == CURSOR_LEFT)
				left_arrow();
			else
				right_arrow();
			return GUI_YUM;
		case NEXT_PARTY_MEMBER_KEY:
			right_arrow(); return GUI_YUM;
		case PREVIOUS_PARTY_MEMBER_KEY:
			left_arrow(); return GUI_YUM;
		case HOME_KEY:
			if(party->get_member_num(actor) >= 0)
				set_actor(party->get_actor(0));
			return GUI_YUM;
		case END_KEY:
			if(party->get_member_num(actor) >= 0)
				set_actor(party->get_actor(party->get_party_size() - 1));
			return GUI_YUM;
		default: break;
	}
	return GUI_PASS;
}

GUI_status PortraitViewGump::MouseDown(int x, int y, int button)
{
// if(party->get_member_num(actor) >= 0)
 {
	 if(button == SDL_BUTTON_WHEELDOWN)
	 {
		right_arrow();
		return GUI_YUM;
	 }
	 else if(button == SDL_BUTTON_WHEELUP)
	 {
		 left_arrow();
		 return GUI_YUM;
	 }
 }

	return DraggableView::MouseDown(x, y, button);
}
