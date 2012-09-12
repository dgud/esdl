/*
 *  Copyright (c) 2001 Dan Gudmundsson
 *  See the file "license.terms" for information on usage and redistribution
 *  of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 * 
 *     $Id$
 */
/* 
 * sdl_video functions
 */

#include <string.h>
#include <stdlib.h>
#include "esdl.h"

#include <SDL_syswm.h>

void es_setVideoMode(sdl_data *sd, int len, char* bp) 
{
   if(!sd->use_smp) {
       es_setVideoMode2(sd->driver_data, 
			driver_caller(sd->driver_data), bp);
   } else { /* opengl initialization must be called from thread */
       gl_dispatch(sd, SDL_SetVideoModeFunc, len, bp);
   }
}

void es_setVideoMode2(ErlDrvPort port, ErlDrvTermData caller, char* bp) 
{
   int w, h, bpp, type;
   SDL_Surface *screen;
   ErlDrvTermData rt[8];

   w    = get16be(bp);
   h    = get16be(bp);
   bpp  = get16be(bp);
   type = get32be(bp);
   screen = SDL_SetVideoMode(w, h, bpp, type),
   rt[0] = ERL_DRV_ATOM; rt[1]=driver_mk_atom((char *) "_esdl_result_");  
   rt[2] = ERL_DRV_UINT;  rt[3] = (ErlDrvUInt) screen;
   rt[4] = ERL_DRV_TUPLE; rt[5] = 2;
   driver_send_term(port,caller,rt,6);
}

void es_videoDriverName(sdl_data *sd, int len, char *buff)
{
  char *bp;
  int sendlen = 0;

  bp = sdl_get_temp_buff(sd, 512);
  if (SDL_VideoDriverName(bp, 512) != NULL) {
    sendlen = strlen(bp);
  }
  sdl_send(sd, sendlen);
}

void es_getVideoSurface(sdl_data *sd, int len, char *buff)
{
   char *bp, *start;
   int sendlen;
   SDL_Surface *ref;
   bp = start = sdl_get_temp_buff(sd, 8);
   
   ref = SDL_GetVideoSurface();
   PUSHGLPTR(ref, bp);
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_getVideoInfo(sdl_data *sd, int len, char *buff)
{
   char *bp, *start;
   int sendlen;
   const SDL_VideoInfo *vi;
   bp = start = sdl_get_temp_buff(sd, 9+2*8);
   vi = SDL_GetVideoInfo();
   if(vi->hw_available)   put8(bp, 1);
   else                   put8(bp, 0);
   if(vi->wm_available)   put8(bp, 1);
   else                   put8(bp, 0);
   if(vi->blit_hw)        put8(bp, 1);
   else                   put8(bp, 0);
   if(vi->blit_hw_CC)     put8(bp, 1);
   else                   put8(bp, 0);
   if(vi->blit_hw_A)      put8(bp, 1);
   else                   put8(bp, 0);
   if(vi->blit_sw)        put8(bp, 1);
   else                   put8(bp, 0);
   if(vi->blit_sw_CC)     put8(bp, 1);
   else                   put8(bp, 0);
   if(vi->blit_sw_A)      put8(bp, 1);
   else                   put8(bp, 0);
   if(vi->blit_fill)      put8(bp, 1);
   else                   put8(bp, 0);
   put32be(bp, vi->video_mem);   
   PUSHGLPTR(vi->vfmt, bp);
   
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_videoModeOK(sdl_data *sd, int len, char *bp)
{   
   if(!sd->use_smp) {
       es_videoModeOK2(sd->driver_data, driver_caller(sd->driver_data), bp);
   } else { 
       gl_dispatch(sd, SDL_VideoModeOKFunc, len, bp);
   }
}

void es_videoModeOK2(ErlDrvPort port, ErlDrvTermData caller, char *buff)
{   
   char *bp;
   int w, h, bpp, type;
   int res;
   ErlDrvTermData rt[8];

   bp = buff;
  
   w    = get16be(bp);
   h    = get16be(bp);
   bpp  = get16be(bp);
   type = get32be(bp);
  
   res = SDL_VideoModeOK(w,h,bpp,type);
   rt[0] = ERL_DRV_ATOM; rt[1]=driver_mk_atom((char *) "_esdl_result_");  
   rt[2] = ERL_DRV_INT; rt[3] = res;
   rt[4] = ERL_DRV_TUPLE; rt[5] = 2;
   driver_send_term(port,caller,rt,6);
}


void es_listModes(sdl_data *sd, int len, char *buff)
{
   char *bp, *start;
   int sendlen, i;
   Uint32 flags;
   SDL_PixelFormat *pfp, pf;
   SDL_Rect ** res; 

   bp = buff;   
   flags = get32be(bp);
   switch(get8(bp)) {
   case 0:
      POPGLPTR(pfp, bp);
      break;
   case 1:
      POPGLPTR(pf.palette, bp);
      pf.BitsPerPixel = get8(bp);
      pf.BytesPerPixel = get8(bp);
      pf.Rloss = get8(bp);
      pf.Gloss = get8(bp);
      pf.Bloss = get8(bp);
      pf.Aloss = get8(bp);   
      pf.Rshift = get8(bp);  
      pf.Gshift = get8(bp);
      pf.Bshift = get8(bp);  
      pf.Ashift = get8(bp);
      pf.Rmask  = get32be(bp);
      pf.Gmask  = get32be(bp);
      pf.Bmask  = get32be(bp);
      pf.Amask  = get32be(bp);  
      pf.colorkey= get32be(bp);
      pf.alpha= get8(bp);
      pfp = &pf;
   default:
      error();
   }

   bp = start = sdl_get_temp_buff(sd, 128*2*4+1);
   res = SDL_ListModes(pfp, flags);
   switch((long)res) {
   case 0: /* NULL */
      put8(bp, 0);      
      break;
   case -1:
      put8(bp,-1);
      break;
   default: 
      put8(bp,0); /* Wasted byte :- )*/
      for(i=0; res[i]; i++) {
	 put16be(bp, res[i]->x); 
	 put16be(bp, res[i]->y); 
	 put16be(bp, res[i]->w); 
	 put16be(bp, res[i]->h); 	 
      }
   }
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_flip(sdl_data *sd, int len, char *buff)
{
   char *bp, *start;
   int sendlen, res;
   SDL_Surface *ref;
   bp = buff;

   POPGLPTR(ref, bp);
   res = SDL_Flip(ref);
   start = bp = sdl_get_temp_buff(sd, 1);
   put8(bp, res);
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_setColors(sdl_data *sd, int len, char *buff)
{
   char *bp, *start;
   int sendlen, res = 1, first, length, i, tot =0;
   SDL_Surface *ref;
   SDL_Color colors[256];
   bp = buff;
   
   POPGLPTR(ref, bp);
   first  = get32be(bp);  
   length = get32be(bp);  
   do {
      for(i = 0; i < length && i < 256; i++) {
	 colors[i].r = get8(bp);
	 colors[i].b = get8(bp);
	 colors[i].g = get8(bp);
      };   
      res &= SDL_SetColors(ref, colors, first, i);
      tot += i;
      first += i;      
   } while(tot < length);

   start = bp = sdl_get_temp_buff(sd, 1);
   put8(bp, res);
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_mapRGB(sdl_data *sd, int len, char *buff)
{
   char *bp, *start;  
   int sendlen;
   SDL_Surface * sptr;
   Uint32 res;
   Uint8 r,g,b;
   
   bp = buff;
   POPGLPTR(sptr, bp);
   r = get8(bp);
   g = get8(bp);
   b = get8(bp);
   if(sptr == NULL  || sptr->format == NULL) 
      error();
   res = SDL_MapRGB(sptr->format, r,g,b);
   
   bp = start = sdl_get_temp_buff(sd, 4);
   put32be(bp, res);
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_getRGB(sdl_data *sd, int len, char *buff)
{
   error();      
}

void es_blitSurface(sdl_data *sd, int len, char * buff)
{
   char *bp, *start;  
   int sendlen;
   char type, res;
   SDL_Surface *sptr, *dptr;   
   SDL_Rect s, d, *src, *dest;
   
   bp = buff;
   POPGLPTR(sptr, bp);
   POPGLPTR(dptr, bp);
   src = &s;
   dest = &d;

   switch get8(bp) {
   case 0:
      src = NULL;
      dest= NULL;
      type = 0;
      break;
   case 1:
      src->x = get16be(bp);
      src->y = get16be(bp);
      src->w = get16be(bp);
      src->h = get16be(bp);     
      dest = NULL;
      type = 1;
      break;
   case 2:
      dest->x = get16be(bp);
      dest->y = get16be(bp);
      dest->w = get16be(bp);
      dest->h = get16be(bp);
      src = NULL;
      type = 2;
      break;
   case 3:
      src->x  = get16be(bp);
      src->y  = get16be(bp);
      src->w  = get16be(bp);
      src->h  = get16be(bp);
      dest->x = get16be(bp);
      dest->y = get16be(bp);
      dest->w = get16be(bp);
      dest->h = get16be(bp);
      type = 3;
      break;
   default:
      error();
   };
   
   res = SDL_BlitSurface(sptr, src, dptr, dest);
   bp = start = sdl_get_temp_buff(sd, 9*2);
   put8(bp, res);
   put8(bp, type);
   switch(type) {
   case 0:
      break;
   case 1:
      put16be(bp, src->x);
      put16be(bp, src->y);
      put16be(bp, src->w);
      put16be(bp, src->h);     
      break;
   case 2:
      put16be(bp, dest->x);
      put16be(bp, dest->y);
      put16be(bp, dest->w);
      put16be(bp, dest->h);
      break;
   case 3:
      put16be(bp, src->x);
      put16be(bp, src->y);
      put16be(bp, src->w);
      put16be(bp, src->h);
      put16be(bp, dest->x);
      put16be(bp, dest->y);
      put16be(bp, dest->w);
      put16be(bp, dest->h);
      break;
   default:
      error();
   };
   
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void print_rect(SDL_Rect * r)
{
   if(r)
      fprintf(stderr, "x %d y %d w %d h %d \n\r", 
              r->x, r->y, r->w, r->h);
   else
      fprintf(stderr, "Rect is null\n\r");     
}

void es_fillRect(sdl_data *sd, int len, char * buff)
{
   char *bp, *start;  
   int sendlen;
   char res;
   SDL_Surface *sptr;
   SDL_Rect s, *src;
   Uint32 color;

   src = &s;
   bp = buff;
   POPGLPTR(sptr, bp);
   color = get32be(bp);
   
   if(get8(bp) == 0) {
      src = NULL;
   } else {
      src->x = get16be(bp);
      src->y = get16be(bp);
      src->w = get16be(bp);
      src->h = get16be(bp);
   }
   res = SDL_FillRect(sptr, src, color);
   
   bp = start = sdl_get_temp_buff(sd, 1);
   put8(bp, res);
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}


void es_updateRects(sdl_data *sd, int len, char * buff)
{
   char *bp;  
   int length, i, all;
   SDL_Surface *sptr;
   SDL_Rect s[64], *sr;
   
   bp = buff;
   POPGLPTR(sptr, bp);
   length  = get16be(bp);

   all = 0;
   while(all < length) {
      sr = s;
      for(i = 0; i < 64 && all < length; i++, all++) {
         sr->x = get16be(bp);
         sr->y = get16be(bp);
         sr->w = get16be(bp);
         sr->h = get16be(bp);
         sr++;
      }
      SDL_UpdateRects(sptr, i, s);
   }
}

void es_setColorKey(sdl_data *sd, int len, char * buff)
{
   char *bp, *start;  
   int sendlen;
   SDL_Surface * sptr;
   Uint32 flag, key;
   int res;
   
   bp = buff;
   POPGLPTR(sptr, bp);
   if(sptr == NULL) 
      error();
   flag = get32be(bp);
   key  = get32be(bp);
    
   res  = SDL_SetColorKey(sptr, flag, key);
    
   bp = start = sdl_get_temp_buff(sd, 1);
   put8(bp, res);    
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_displayFormat(sdl_data *sd, int len, char * buff)
{
   char *bp, *start;  
   int sendlen;
   SDL_Surface * sptr, *new;
    
   bp = buff;
   POPGLPTR(sptr, bp);
   if(sptr == NULL) 
      error();            
   new = SDL_DisplayFormat(sptr);
   bp = start = sdl_get_temp_buff(sd, 8);
   PUSHGLPTR(new, bp);
    
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_createRGBSurface(sdl_data *sd, int len, char *buff){
   char *bp, *start;  
   int sendlen;
   SDL_Surface * sptr;
   int width, height, depth, def;
   Uint32 flags, rmask, gmask, bmask, amask;
    
   bp = buff;
   flags  = get32be(bp);
   width  = get16be(bp);
   height = get16be(bp);
   depth  = get8(bp); 
   def = get8(bp);
   if (def==1) 
     {
   rmask  = get32be(bp);
   gmask  = get32be(bp);
   bmask  = get32be(bp);
   amask  = get32be(bp);
     }
   else
     {
#if SDL_BYTEORDER == SDL_LIL_ENDIAN /* OpenGL RGBA masks */
       rmask = 0x000000FF;
       gmask = 0x0000FF00;
       bmask = 0x00FF0000; 
       amask = 0xFF000000;
#else
       rmask = 0xFF000000;
       gmask = 0x00FF0000; 
       bmask = 0x0000FF00; 
       amask = 0x000000FF;
#endif
     };
   
   sptr = SDL_CreateRGBSurface(flags, width, height, depth, 
			       rmask, gmask, bmask, amask);
   bp = start = sdl_get_temp_buff(sd, 8);
   PUSHGLPTR(sptr, bp);
    
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_createRGBSurfaceFrom(sdl_data *sd, int len, char *buff){
   char *bp, *start;  
   int sendlen;
   SDL_Surface * sptr;
   void * pixels;
   int width, height, depth, pitch, size;
   Uint32 rmask, gmask, bmask, amask;
    
   bp = buff;
   width  = get16be(bp);
   height = get16be(bp);
   depth  = get8(bp); 
   pitch  = get16be(bp);
   rmask  = get32be(bp);
   gmask  = get32be(bp);
   bmask  = get32be(bp);
   amask  = get16be(bp);
   size   = get32be(bp);
   
   if(size == -1) 
   {
      POPGLPTR(pixels, bp);
   }
   else {
      /* Get Pixels */
      if((pixels = malloc(size)) == NULL){
	 bp = start = sdl_get_temp_buff(sd, 8);
	 sptr = NULL;
	 PUSHGLPTR(sptr, bp);	   
	 sendlen = bp - start;
	 sdl_send(sd, sendlen);
	 return;
      }
      memcpy(pixels, (void *) bp, size);
   }; 
   
   sptr = SDL_CreateRGBSurfaceFrom(pixels, width, height, depth, pitch,
				   rmask, gmask, bmask, amask);
   bp = start = sdl_get_temp_buff(sd, 8);
   PUSHGLPTR(sptr, bp);
   
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_freeSurface(sdl_data *sd, int len, char * buff)
{
    char *bp;  
    SDL_Surface * sptr;
    
    bp = buff;
    POPGLPTR(sptr, bp);
    if(sptr == NULL) 
	error();            
    SDL_FreeSurface(sptr);
}

void es_lockSurface(sdl_data *sd, int len, char * buff)
{
   char *bp, *start;  
   SDL_Surface * sptr;
   int sendlen, res;
   
   bp = buff;
   POPGLPTR(sptr, bp);
   if(sptr == NULL) 
      error();             
   res = SDL_LockSurface(sptr);
   bp = start = sdl_get_temp_buff(sd, 1);
   put8(bp, res);
   
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_unlockSurface(sdl_data *sd, int len, char * buff)
{
   char *bp;  
   SDL_Surface * sptr;
   
   bp = buff;
   POPGLPTR(sptr, bp);
   SDL_UnlockSurface(sptr);
}

void es_loadBMP(sdl_data *sd, int len, char* bp)
{
   char *start;
   SDL_Surface *bmpSurface;
   int sendlen;

   bmpSurface = SDL_LoadBMP(bp);    
   bp = start = sdl_get_temp_buff(sd, 8);
   PUSHGLPTR(bmpSurface, bp);
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_saveBMP(sdl_data *sd, int len, char* bp)
{
   char *start;
   SDL_Surface *sptr;
   int sendlen, res;

   POPGLPTR(sptr, bp);
   res = SDL_SaveBMP(sptr, bp);    
   bp = start = sdl_get_temp_buff(sd, 1);
   put8(bp, res); 
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_setAlpha(sdl_data *sd, int len, char *buff)
{
   char *bp, *start;
   SDL_Surface *sptr;
   int sendlen, res;
   Uint32 flag, alpha;
   bp = buff;   
   POPGLPTR(sptr, bp);
   flag =  get32be(bp);
   alpha = get32be(bp);
   res = SDL_SetAlpha(sptr, flag, alpha);
   bp = start = sdl_get_temp_buff(sd, 4);
   put32be(bp, res); 
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_setClipping(sdl_data *sd, int len, char *buff)
{
   error();
}

void es_setGamma(sdl_data *sd, int len, char *bp)
{
   char *start;
   int sendlen, res;
   float red, green, blue;

   red   = * (float *) bp; bp += sizeof(float);
   green = * (float *) bp; bp += sizeof(float);
   blue  = * (float *) bp; bp += sizeof(float);
   res = SDL_SetGamma(red, green, blue);
   bp = start = sdl_get_temp_buff(sd, 4);
   * (float *) bp = res;
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_setGammaRamp(sdl_data *sd, int len, char * buff) 
{
   char *bp, *start;
   int sendlen, res;
   Uint16 *red,*green,*blue;
   bp = buff;   
   red   = (Uint16 *) bp; bp += 256*sizeof(Uint16);
   green = (Uint16 *) bp; bp += 256*sizeof(Uint16);
   blue  = (Uint16 *) bp; bp += 256*sizeof(Uint16);
   res = SDL_SetGammaRamp(red,green,blue);
   bp = start = sdl_get_temp_buff(sd, 4);
   put32be(bp, res); 
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_getGammaRamp(sdl_data *sd, int len, char * buff)
{
   char *bp, *start;
   int sendlen, res, i;
   Uint16 red[256], green[256],blue[256];
   res = SDL_GetGammaRamp(red,green,blue);
   bp = start = sdl_getbuff(sd, 4 + 256*2*3);
   put32be(bp, res); 
   for(i=0; i<256; i++) {
      put16be(bp, red[i]);
   }
   for(i=0; i<256; i++) {
      put16be(bp, green[i]);
   }
   for(i=0; i<256; i++) {
      put16be(bp, blue[i]);
   }
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_wm_setCaption(sdl_data *sd, int len, char *bp) 
{ 
    if(!sd->use_smp) {
	es_wm_setCaption2(bp);
    } else { 
	gl_dispatch(sd, SDL_WM_SetCaptionFunc, len, bp);
    }
}

void es_wm_setCaption2(char *bp) 
{
    char* title;
    char* icon;

    title = bp;
    icon = title + strlen(title) + 1;
    
    SDL_WM_SetCaption(title, icon);
}

void es_wm_getCaption(sdl_data *sd, int len, char *buff) 
{
   char * title, *icon, *bp, *start;
   int i, titlelen, iconlen, sendlen;
   
   SDL_WM_GetCaption(&title, &icon);
   for(titlelen = 0; title[titlelen] != '\0'; titlelen++);
   for(iconlen = 0; icon[iconlen] != '\0'; iconlen++);
   
   bp = start = sdl_get_temp_buff(sd, 4+titlelen+iconlen);
   put16be(bp, titlelen);
   put16be(bp, iconlen);
   for(i = 0; i < titlelen; i++)
      put8(bp, title[i]);
   for(i = 0; i < iconlen; i++)
      put8(bp, icon[i]);   
   
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_wm_setIcon(sdl_data *sd, int len, char *buff) 
{
   char *bp;
   SDL_Surface *sptr;
   Uint16 size;

   bp = buff;   
   POPGLPTR(sptr, bp);
   size = get16be(bp);
   if (size > 0) {
     SDL_WM_SetIcon(sptr, (Uint8 *) bp);
   } else {
     SDL_WM_SetIcon(sptr, NULL);
   }
}

void es_wm_iconifyWindow(sdl_data *sd, int len, char *buff) 
{
   SDL_WM_IconifyWindow();     
}

void es_wm_toggleFullScreen(sdl_data *sd, int len, char *bp) 
{
    if(!sd->use_smp) {
	es_wm_toggleFullScreen2(sd->driver_data, driver_caller(sd->driver_data), bp);
    } else {
	gl_dispatch(sd, SDL_WM_ToggleFullScreenFunc, len, bp);
    }
}

void es_wm_toggleFullScreen2(ErlDrvPort port, ErlDrvTermData caller, char *buff) 
{
    char *bp;
    SDL_Surface *sptr;
    int res;
    ErlDrvTermData rt[8];

    bp = buff;   
    POPGLPTR(sptr, bp);
    res = SDL_WM_ToggleFullScreen(sptr);
    rt[0] = ERL_DRV_ATOM; rt[1]=driver_mk_atom((char *) "_esdl_result_");  
    rt[2] = ERL_DRV_INT; rt[3] = res;
    rt[4] = ERL_DRV_TUPLE; rt[5] = 2;
    driver_send_term(port,caller,rt,6);
}


void es_wm_grabInput(sdl_data *sd, int len, char *bp) 
{
  char *start;
  SDL_GrabMode mode;
  int sendlen;

  mode = (SDL_GrabMode) *bp;
  mode = SDL_WM_GrabInput(mode);
  bp = start = sdl_get_temp_buff(sd, 1);
  *bp++ = mode;
  sendlen = bp - start;
  sdl_send(sd, sendlen);
}

void es_wm_getInfo(sdl_data *sd, int len, char *bp) 
{
    if(!sd->use_smp) {
	es_wm_getInfo2(sd->driver_data, driver_caller(sd->driver_data), bp);
    } else {
	gl_dispatch(sd, SDL_WM_GetInfoFunc, len, bp);
    }
}

void es_wm_getInfo2(ErlDrvPort port, ErlDrvTermData caller, char *buff) 
{
   int res;
   SDL_SysWMinfo info;
   ErlDrvTermData rt[12];

   SDL_VERSION(&info.version);
   SDL_GetWMInfo(&info);

   rt[0] = ERL_DRV_ATOM; rt[1]=driver_mk_atom((char *) "_esdl_result_");  
   rt[2] = ERL_DRV_INT; rt[3] = info.version.major;
   rt[4] = ERL_DRV_INT; rt[5] = info.version.minor;
   rt[6] = ERL_DRV_INT; rt[7] = info.version.patch;
#ifdef WIN32
   if(sizeof(info.window) == 4) {
       rt[8] = ERL_DRV_UINT; rt[9] = (unsigned int) info.window;
   } else {
       rt[8] = ERL_DRV_UINT; rt[9] = 0;
   }
#else
   rt[8] = ERL_DRV_UINT; rt[9] = 0;
#endif
   rt[10] = ERL_DRV_TUPLE; rt[11] = 5;
   res = driver_send_term(port,caller,rt,12);
}

void es_wm_isMaximized(sdl_data *sd, int len, char *buff)
{
     int sendlen;
     char *bp, *start;
     int s;
     
#ifdef _WIN32
     {
	SDL_SysWMinfo info;
	
	SDL_VERSION(&info.version);
	SDL_GetWMInfo(&info);
	s = IsZoomed(info.window);
     }
#else
     {  /* Workaround for missing functions, window manager dep functions */
/* 	SDL_Surface *screen; */
/* 	SDL_Rect **modes; */
	
 	s = 0; 
/* 	screen = SDL_GetVideoSurface();		    */
	/* Get available fullscreen modes, I think it gives the max resolution */
/* 	modes = SDL_ListModes(screen->format, screen->flags | SDL_FULLSCREEN);  */
	/* Check is there are any modes available */
/* 	if(modes == (SDL_Rect **)0  || modes == (SDL_Rect **)-1) { */
/* 	   s = 0;   */  /* We don't know ?? */
/* 	}  */
/* 	else { */
/* 	   if ( screen->w >= modes[0]->w && screen->h >= modes[0]->h)  */
/* 	      s = 1; */
/* 	   else */
/* 	      s = 0; */
/* 	} */
     }
#endif

     start = bp = sdl_get_temp_buff(sd, 1);
     put8(bp, s);
     sendlen = bp - start;
     sdl_send(sd, sendlen);
}

void es_wm_maximize(sdl_data *sd, int len, char *bp)
{
    if(!sd->use_smp) {
	es_wm_maximize2(sd->driver_data, 
			driver_caller(sd->driver_data), bp);
    } else { 
	gl_dispatch(sd, SDL_WM_MaximizeFunc, len, bp);
    }
}

void es_wm_maximize2(ErlDrvPort port, ErlDrvTermData caller, char *buff)
{
#ifdef _WIN32
    SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
    SDL_GetWMInfo(&info);
    ShowWindow(info.window, SW_SHOWMAXIMIZED);
#endif
}



void es_gl_setAttribute(sdl_data *sd, int len, char *bp)
{
    if(!sd->use_smp) {
	es_gl_setAttribute2(sd->driver_data, 
			    driver_caller(sd->driver_data), bp);
    } else { 
	gl_dispatch(sd, SDL_GL_SetAttributeFunc, len, bp);
    }
}

void es_gl_setAttribute2(ErlDrvPort port, ErlDrvTermData caller, char *buff)
{
    char *bp;
    int attr, val;
    bp = buff;
    attr = get16be(bp);
    val = get32be(bp);
    SDL_GL_SetAttribute(attr, val);
}


void es_gl_getAttribute(sdl_data *sd, int len, char *bp)
{
    if(!sd->use_smp) {
	es_gl_getAttribute2(sd->driver_data, 
			    driver_caller(sd->driver_data), bp);
    } else { 
	gl_dispatch(sd, SDL_GL_GetAttributeFunc, len, bp);
    }
}

void es_gl_getAttribute2(ErlDrvPort port, ErlDrvTermData caller, char *buff)
{
    char *bp;
    int attr, val;
    ErlDrvTermData rt[8];

    bp = buff;
    attr = get16be(bp);
    SDL_GL_GetAttribute(attr, &val);

    rt[0] = ERL_DRV_ATOM; rt[1]=driver_mk_atom((char *) "_esdl_result_");  
    rt[2] = ERL_DRV_INT; rt[3] = val;
    rt[4] = ERL_DRV_TUPLE; rt[5] = 2;
    driver_send_term(port,caller,rt,6);
}


void es_gl_swapBuffers(sdl_data *sd, int len, char *buff)
{  
   char *bp, *start;
   int sendlen;
   Uint32 ts;
   
   if(!sd->use_smp) 
      SDL_GL_SwapBuffers();
   else
      gl_dispatch(sd, SDL_GL_SwapBuffersFunc, len, buff);
   
   /* Timer is removed it a bad idea anyway 
      (works if vsync is on only I believe) */
   
   bp = start = sdl_get_temp_buff(sd, 4);
   ts = 0; 
   put32be(bp, ts);
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_mapRGBA(sdl_data *sd, int len, char *buff)
{
   char *bp, *start;  
   int sendlen;
   SDL_Surface * sptr;
   Uint32 res;
   Uint8 r,g,b,a;
   
   bp = buff;
   POPGLPTR(sptr, bp);
   r = get8(bp);
   g = get8(bp);
   b = get8(bp);
   a = get8(bp);
   if(sptr == NULL  || sptr->format == NULL) 
      error();
   res = SDL_MapRGBA(sptr->format, r,g,b,a);
   
   bp = start = sdl_get_temp_buff(sd, 4);
   put32be(bp, res);
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_getRGBA(sdl_data *sd, int len, char *buff)
{
  error();
}

void es_getClipRect(sdl_data *sd, int len, char *buff)
{
   char *bp, *start;  
   int sendlen;
   SDL_Surface * sptr;
   SDL_Rect rect;
   
   bp = buff;
   POPGLPTR(sptr, bp);
   if(sptr == NULL) 
      error();
   SDL_GetClipRect(sptr, &rect);
   
   bp = start = sdl_get_temp_buff(sd, 8);
   put16be(bp, rect.x);
   put16be(bp, rect.y);
   put16be(bp, rect.w);
   put16be(bp, rect.h);
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}

void es_setClipRect(sdl_data *sd, int len, char *buff)
{
   char *bp;  
   SDL_Surface * sptr;
   SDL_Rect rect;
   
   bp = buff;
   POPGLPTR(sptr, bp);
   rect.x = get16be(bp);
   rect.y = get16be(bp);
   rect.w = get16be(bp);
   rect.h = get16be(bp);
   if(sptr == NULL) 
      error();
   SDL_SetClipRect(sptr, &rect);
   
}

void es_displayFormatAlpha(sdl_data *sd, int len, char * buff)
{
   char *bp, *start;  
   int sendlen;
   SDL_Surface * sptr, *new;
    
   bp = buff;
   POPGLPTR(sptr, bp);
   if(sptr == NULL) 
      error();            
   new = SDL_DisplayFormatAlpha(sptr);
   bp = start = sdl_get_temp_buff(sd, 8);
   PUSHGLPTR(new, bp);
    
   sendlen = bp - start;
   sdl_send(sd, sendlen);
}


