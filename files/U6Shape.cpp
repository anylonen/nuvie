/*
 *  U6Shape.cpp
 *  Nuvie
 *
 *  Created by Markus Niemist� on Sun May 25 2003.
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

/*
 * ==========
 *  Includes
 * ==========
 */
#include <string>
#include <stdlib.h>
#include "SDL.h"
#include "SDL_endian.h"
#include "nuvieDefs.h"
#include "U6misc.h"
#include "U6Lzw.h"
#include "U6Lib_n.h"
#include "NuvieIO.h"
#include "U6Shape.h"


/*
 * Structure of shape file:
 * ========================
 *
 *      -> means input from file
 *      <- means storing pixel data
 *
 * .shp files are lzw compressed. After decompressing the file represents
 * following structure: -> file size (dword)
 *                      -> set of offsets (each word)
 *			-> set of shapes
 *
 * File size should be quite clear.
 *
 * Offsets are stored as unsigned words. The first offset in file is the
 * offset of the first object (simple, huh?). The number of offsets (objects)
 * in the file can be calculated as follows:
 *                      num_objects = (1st offset - 4) / 4.
 *
 * Frame structure:     -> num of pixels right from hot spot, X1 (word)
 *                      -> num of pixels left from hot spot, X2 (word)
 *                      -> num of pixels above hot spot, Y1 (word)
 *                      -> num of pixels below hot spot, Y2 (word)
 *                      -> set of pixel blocks
 *
 * The width of the shape can be calculated by adding X1 and X2 together and
 * height by adding Y1 and Y2 together. Coordinates for hot spot are X2 and Y1.
 *
 * Now the data it self is stored in pixel blocks which are quite complex:
 *                      -> number of pixels or repeats, num1 (word)
 *                      if (num1 and 1)
 *                         repeat num1 >> 1 times
 *                            -> temp value (unsigned byte)
 *                            if (temp value and 1)
 *                               -> pixel
 *                               <- store pixel temp value >> 1 times
 *                            else
 *                               <- read temp value >> 1 bytes
 *                         end
 *                     else
 *                         <- read num >> 1 bytes
 *
 * Color number 255 seems to be transperent.
 *
 * I hope this clears things up a bit.
 */


/*
 * =====================
 *  U6Shape::U6Shape();
 * =====================
 *
 * Just intializes all structures to 0.
 */
U6Shape::U6Shape()
{
 raw = NULL;
 hotx = hoty = 0;
 width = height = 0;
}

/*
 * ======================
 *  U6Shape::~U6Shape();
 * ======================
 *
 * Frees all memory allocated by this instance of U6Shape class.
 */
U6Shape::~U6Shape(void)
{
 if(raw)
  free(raw);
}

bool U6Shape::init(uint16 w, uint16 h, uint16 hx, uint16 hy)
{
 width = w;
 height = h;
 hotx = hx;
 hoty = hy;
 raw = (uint8 *)malloc(width * height);

 if(raw == NULL)
 {
	 DEBUG(0,LEVEL_ERROR,"malloc failed to allocate space for shape\n");
	 return false;
 }

 memset(raw, 0xff, width * height);

 return true;
}

bool U6Shape::load(std::string filename)
{
 return false;
}

bool U6Shape::load(U6Lib_n *file, uint32 index)
{
 unsigned char *buf;

 buf = file->get_item(index);
 if(buf != NULL)
   {
    if(load(buf))
      {
       free(buf);
       return true;
      }
    else
       free(buf);
   }

 return false;
}

bool U6Shape::load_from_lzc(std::string filename, uint32 idx, uint32 sub_idx)
{
  U6Lib_n lib_n;
  unsigned char *buf = NULL;

  if(!lib_n.open(filename, 4, NUVIE_GAME_MD))
  {
    return NULL;
  }

  if(idx >= lib_n.get_num_items())
  {
    return NULL;
  }

  buf = lib_n.get_item(idx,NULL);
  NuvieIOBuffer io;
  io.open(buf, lib_n.get_item_size(idx), false);
  U6Lib_n lib1;
  lib1.open(&io, 4, NUVIE_GAME_MD);

  if(sub_idx >= lib1.get_num_items())
  {
    return NULL;
  }

  if(load(&lib1, (uint32)sub_idx))
  {
    free(buf);
    return true;
  }

  free(buf);
  return false;
}

/*
 * =========================================
 *  bool U6Shape::load(unsigned char *buf);
 * =========================================
 *
 * Loads shape from buf
 * Returns true if successful, else returns false.
 */
bool U6Shape::load(unsigned char *buf)
{
	int encoded;
	unsigned char *data;
	uint16 num_pixels;
	sint16 xpos, ypos;

	/* A file already loaded. */
	if (raw != NULL)
		return false;
		/* NOT REACHED */

  data = buf;

  /* Size and hot point. */
  width = SDL_SwapLE16(*(uint16*)data); data += 2;
  width += hotx = SDL_SwapLE16(*(uint16*)data); data += 2;

  height = hoty = SDL_SwapLE16(*(uint16*)data); data += 2;
  height += SDL_SwapLE16(*(uint16*)data); data += 2;

  width++; height++;

  /* Allocate memory for shape and make it all transperent. */
  raw = (unsigned char*)malloc(width * height);
  if (raw == NULL)
  {
    DEBUG(0,LEVEL_ERROR,"malloc failed to allocate space for shape\n");
    return false;
  }
  memset(raw, 255, width*height);

  /* Get the pixel data. */
  while ((num_pixels = SDL_SwapLE16(*(uint16*)data)) != 0)
    {

     data += 2;

     /* Coordinates relative to hot spot. */
     xpos = SDL_SwapLE16(*(uint16*)data); data += 2;
     ypos = SDL_SwapLE16(*(uint16*)data); data += 2;

     if (((hotx+xpos)>=width) || ((hoty+ypos)>=height)) 
     {
       break;
     }
     /*
      * Test if this block of pixels is encoded
      * (bit0 is set).
      */
     encoded = num_pixels & 1;

     /* Divide it by 2. */
     num_pixels >>= 1;
     
     /* Normal pixel:
      * =============
      *
      * Just fetch as many pixels as num_pixels suggests.
      */
     if (!encoded)
      {
       memcpy(raw + (hotx + xpos) +
				   (hoty + ypos) * width, data, num_pixels);
       data += num_pixels;

       continue;
				/* NOT REACHED */
			}

			/* Encoded pixel:
			 * ==============
			 *
			 * Do as many repeats as num_pixels suggests.
			 */
			for (int j = 0; j < num_pixels; )
			{
				unsigned char num_pixels2 = *data++;
				int repeat = num_pixels2 & 1;

				num_pixels2 >>= 1;

				/*
				 * Repeat pixel value (data + 1) num_pixels2
				 * times.
				 */
				if (repeat)
				{
					memset(raw + (hotx + xpos) +
					   (hoty + ypos) * width + j,
					   *data++, num_pixels2);
				}


			 	/*
				 * Just fetch as many pixels as num_pixels2
				 * suggests.
				 */
				else
				{
					memcpy(raw + (hotx + xpos) +
					   (hoty + ypos) * width + j, data,
					   num_pixels2);
					data += num_pixels2;
				}
				j += num_pixels2;
			}

		}

	return true;
}

// TODO - allow for failure
bool U6Shape::load_WoU_background(Configuration *config, nuvie_game_t game_type)
{
 U6Lib_n file;
 unsigned char *temp_buf;
 std::string filename;

 if(game_type == NUVIE_GAME_MD)
	config_get_path(config,"mdscreen.lzc",filename);
 else // SE
	config_get_path(config,"screen.lzc", filename);

 file.open(filename,4,game_type);
 temp_buf = file.get_item(0);
 load(temp_buf + 8);
 free(temp_buf);
 return true;
}

/*
 * =====================================
 *  unsigned char *U6Shape::get_data();
 * =====================================
 *
 * Returns raw data representing the shape or NULL on failure.
 */
unsigned char *U6Shape::get_data()
{
	return raw;
}

/*
 * ============================================
 *  SDL_Surface *U6Shape::get_shape_surface();
 * ============================================
 *
 * Returns a SDL_Surface representing the shape
 * or NULL on failure. NOTE! user must free this
 * data.
 */
SDL_Surface *U6Shape::get_shape_surface()
{
	if (raw == NULL)
		return NULL;
		/* NOT REACHED */

	return SDL_CreateRGBSurfaceFrom(raw, width, height,8, width, 0, 0, 0, 0);
}


/*
 * ====================================================
 *  bool U6Shape::get_hot_point(uint16 *x, uint16 *y);
 * ====================================================
 *
 * Puts the coordinates of the shape in x and y and
 * returns true or on failure just returns false.
 */
bool U6Shape::get_hot_point(uint16 *x, uint16 *y)
{
	if (raw == NULL)
		return false;
		/* NOT REACHED */

	*x = hotx;
	*y = hoty;

	return true;
}

/*
 * ===============================================
 *  bool U6Shape::get_size(uint16 *w, uint16 *h);
 * ===============================================
 *
 * Puts the size of the shape in w and h and
 * returns true or on failure just returns false.
 */
bool U6Shape::get_size(uint16 *w, uint16 *h)
{
	if (raw == NULL)
		return false;
		/* NOT REACHED */

	*w = width;
  *h = height;

	return true;
}

void U6Shape::draw_line(uint16 sx, uint16 sy, uint16 ex, uint16 ey, uint8 color)
{
	if (raw == NULL)
		return;

	draw_line_8bit(sx, sy, ex, ey, color, raw, width, height);
}

bool U6Shape::blit(U6Shape *shp, uint16 x, uint16 y)
{
	if(shp == NULL)
		return false;

	unsigned char *src_data = shp->get_data();
	uint16 src_w, src_h;

	shp->get_size(&src_w, &src_h);

	if(x+src_w > width || y+src_h > height)
		return false;

	for(int i=0;i<src_h;i++)
	{
		memcpy(&raw[x+y*width+i*width], &src_data[i*src_w], src_w);
	}

	return true;
}

void U6Shape::fill(uint8 color)
{
	memset(raw, color, width*height);
}
