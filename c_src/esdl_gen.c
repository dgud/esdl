/*
 *  Copyright (c) 2001 Dan Gudmundsson
 *  See the file "license.terms" for information on usage and redistribution
 *  of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 *     $Id$
 */

/* 
 * General SDL functions.
 */

#include "esdl.h"
#include <string.h>

#ifdef _OSX_COCOA
	#include <AppKit/NSScreen.h>
#endif

void es_init(sdl_data *sd, int len, char *bp) 
{
   Uint32 mode;
   
   mode = * (Uint32 *) bp;
   if (SDL_Init(mode) < 0)  {
     char* e = SDL_GetError();
     fprintf(stderr, "Couldn't initialize SDL: %s\n\r", e);
   }
#ifdef _OSX_COCOA
   char sz[64];
   NSRect fullframe = [[NSScreen mainScreen] frame]; snprintf(sz, 64, "%d,%d", (int)fullframe.size.width, (int)fullframe.size.height); setenv("SDL_VIDEO_WINDOW_FULLSCREEN_SIZE", sz, YES);
   NSRect prefframe = [[NSScreen mainScreen] visibleFrame]; snprintf(sz, 64, "%d,%d", (int)fullframe.size.width, (int)prefframe.size.height); setenv("SDL_VIDEO_WINDOW_MAXIMIZE_SIZE", sz, YES);
#endif
}

void es_quit(sdl_data *sd, int len, char * buff) 
{
    SDL_Quit();
}

void es_getError(sdl_data *sd, int len, char *buff)
{
   char * err, *bp, *start;  
   int length;
   err = SDL_GetError();
   length = strlen(err);
   bp = start = sdl_getbuff(sd, length);
   while(*err != '\0') {
      put8(bp, *err++);
   }
   sdl_send(sd, bp - start);
}
