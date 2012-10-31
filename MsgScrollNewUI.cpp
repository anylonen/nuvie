/*
 *  MsgScrollNewUI.cpp
 *  Nuvie
 *
 *  Created by Eric Fry on Sun Sep 23 2012.
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
#include <string>
#include <cctype>
#include <iostream>

#include "nuvieDefs.h"
#include "Configuration.h"
#include "U6misc.h"
#include "FontManager.h"
#include "Font.h"
#include "GamePalette.h"
#include "GUI.h"
#include "MsgScroll.h"
#include "Portrait.h"
#include "Player.h"
#include "ConvFont.h"
#include "MsgScrollNewUI.h"
#include "ActorManager.h"
#include "TimedEvent.h"

// MsgScrollNewUI Class

MsgScrollNewUI::MsgScrollNewUI(Configuration *cfg, Screen *s)
{
 drop_target = false; //we don't participate in drag and drop.

 font_normal = Game::get_game()->get_font_manager()->get_conv_font();

 font_garg = new ConvFont();
 font_garg->init(NULL, 256, 128);

 init(cfg, font_normal);

 scrollback_height = MSGSCROLL_SCROLLBACK_HEIGHT;

 std::string new_scroll_cfg = config_get_game_key(config) + "/newscroll";

 cfg->value(new_scroll_cfg + "/solid_bg", solid_bg, false);

 int c;
 cfg->value(new_scroll_cfg + "/bg_color", c, 218);
 bg_color = clamp_max(c, 255);
 cfg->value(new_scroll_cfg + "/border_color", c, 220);
 border_color = clamp_max(c, 255);

 cfg->value(new_scroll_cfg + "/width", c, 30);
 scroll_width = c;
 cfg->value(new_scroll_cfg + "/height", c, 19);
 scroll_height = clamp_max(c, scrollback_height);



 uint16 x_off = config_get_video_x_offset(config);
 uint16 y_off = config_get_video_y_offset(config);

 GUI_Widget::Init(NULL, x_off, y_off, s->get_width(), s->get_height());

 cursor_wait = 0;

 timer = NULL;

 position = 0;
}

MsgScrollNewUI::~MsgScrollNewUI()
{
	delete font_garg;
}

void MsgScrollNewUI::display_string(std::string s, Font *f, bool include_on_map_window)
{
	if(s.empty())
		return;
	bool has_trailing_whitespace = (!trailing_whitespace.empty());
	s = trailing_whitespace + s;
	trailing_whitespace.clear();

	std::string::reverse_iterator iter;
	uint16 i;
	for(i=0,iter=s.rbegin();iter != s.rend();iter++,i++)
	{
		char c = *iter;
		if(c != '\t' && c != '\n')
			break;
	}

	if(i>0)
	{
		trailing_whitespace = s.substr(s.length()-i,i);
		s = s.substr(0, s.length()-i);
	}

	if(!s.empty())
	{
		if(position>0 && position == msg_buf.size())
		{
			if(!has_trailing_whitespace)
				position--;
			else
			{
				position += count_empty_lines(s) - 1;
			}
		}
		timer = new TimedCallback(this, NULL, 2000);

		MsgScroll::display_string(s, f, include_on_map_window);
	}
}

uint16 MsgScrollNewUI::count_empty_lines(std::string s)
{
	std::string::iterator iter;
	uint16 count = 0;
	for(iter=s.begin();iter != s.end();iter++)
	{
		char c = *iter;
		if(c != ' ' && c != '\t' && c != '\n')
			break;

		if(c == '\n')
			count++;
	}

	return count;
}

void MsgScrollNewUI::set_font(uint8 font_type)
{
	if(font_type == NUVIE_FONT_NORMAL)
	{
		font = font_normal;
	}
	else
	{
		font = font_garg;
	}
}

bool MsgScrollNewUI::is_garg_font()
{
	return (font==font_garg);
}

uint16 MsgScrollNewUI::callback(uint16 msg, CallBack *caller, void *data)
{
	if(msg == CB_TIMED && (timer == NULL || timer == caller))
	{
		timer = NULL;
		if(input_mode)
		{
			new TimedCallback(this, NULL, 100);
		}
		else
		{
			//roll up the message scroll so it's out of the way.
			if(position < msg_buf.size())
			{
				if((uint16)(position+1) < msg_buf.size()
						|| msg_buf.back()->total_length > 0) //don't advance if on second last line and the last line is empty.
				{
					position++;
					new TimedCallback(this, NULL, 50);
				}
			}

		}
	}

	return 1;
}

void MsgScrollNewUI::Display(bool full_redraw)
{
	MsgText *token;

	uint16 y = area.y + 4;
	std::list<MsgLine *>::iterator iter;

	iter=msg_buf.begin();
	for(uint16 i=0;i < position && iter != msg_buf.end(); i++)
		iter++;

	for(uint16 i=0;i< scroll_height && iter != msg_buf.end();i++,iter++)
	{
		MsgLine *msg_line = *iter;
		std::list<MsgText *>::iterator iter1;

		iter1=msg_line->text.begin();

		//if not last record or if last record is not an empty line.
		if(i + position < (msg_buf.size()-1) || (iter1 != msg_line->text.end() && ((*iter)->total_length != 0)))
		{
			if(bg_color != 255)
			{
				if(solid_bg)
					screen->fill(bg_color, area.x, y + (i==0?-4:4), scroll_width * 7 + 8, (i==0?18:10));
				else
					screen->stipple_8bit(bg_color, area.x, y + (i==0?-4:4), scroll_width * 7 + 8, (i==0?18:10));
			}

			if(border_color != 255)
			{
				screen->fill(border_color, area.x, y + (i==0?-4:4), 1, (i==0?18:10));
				screen->fill(border_color, area.x + scroll_width * 7 + 7, y + (i==0?-4:4), 1, (i==0?18:10));
			}

			for(uint16 total_length = 0;iter1 != msg_line->text.end() ; iter1++)
			{
				token = *iter1;

				total_length += token->font->drawString(screen, token->s.c_str(), area.x + 4 + 4 + total_length, y + 4, 0); //FIX for hardcoded font height
			}
			y+=10;
		}

	}
	if(border_color != 255 && y != area.y + 4)
	{
		screen->fill(border_color, area.x, y + 4, scroll_width * 7 + 8, 1); //draw bottom border
	}
/* Debug
	char buf[10];
	snprintf(buf, 10, "%d", position);
	font_normal->drawString(screen, buf, 160, 10);
	snprintf(buf, 10, "%d", (int)msg_buf.size());
	font_normal->drawString(screen, buf, 160, 20);
*/
	screen->update(area.x,area.y, area.w, area.h);
}

GUI_status MsgScrollNewUI::KeyDown(SDL_keysym key)
{
	ScrollEventType event = SCROLL_ESCAPE;

	switch(key.sym)
	{
	case SDLK_PAGEDOWN: event = SCROLL_DOWN; break;
	case SDLK_PAGEUP: event = SCROLL_UP; break;
	default : break;
	}

	if(scroll_movement_event(event) == GUI_YUM)
		return GUI_YUM;

    return MsgScroll::KeyDown(key);
}

GUI_status MsgScrollNewUI::MouseDown(int x, int y, int button)
{
	ScrollEventType event = SCROLL_ESCAPE;

	switch(button)
	{
	case SDL_BUTTON_WHEELDOWN : event = SCROLL_DOWN; break;
	case SDL_BUTTON_WHEELUP : event = SCROLL_UP; break;
	default : break;
	}

	return scroll_movement_event(event);
}

GUI_status MsgScrollNewUI::scroll_movement_event(ScrollEventType event)
{
	switch(event)
	{
	case SCROLL_UP :
		if(position > 0)
		{
			position--;
			grab_focus();
		}
		return GUI_YUM;

	case SCROLL_DOWN :
		if(position < msg_buf.size())
			position++;
		return (GUI_YUM);

	default :
		release_focus();
		new TimedCallback(this, NULL, 50);
		break;
	}

	return GUI_PASS;
}

MsgLine *MsgScrollNewUI::add_new_line()
{
	MsgLine *line = MsgScroll::add_new_line();

	if(position + scroll_height < msg_buf.size())
	{
		position++;
	}
	else if(position + scroll_height > scrollback_height)
	{
		position--;
	}
	return line;
}