/*
 *  Copyright (c) 2010 Dan Gudmundsson
 *  See the file "license.terms" for information on usage and redistribution
 *  of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 *     $Id$
 */

#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <dlfcn.h> 
#endif
#include "esdl.h"
#include "esdl_events.h"


#ifdef  _OSX_COCOA
int erl_drv_stolen_main_thread_join(ErlDrvTid tid, void **respp);
int erl_drv_steal_main_thread(char *name,
			      ErlDrvTid *dtid,
			      void* (*func)(void*),
			      void* arg,
			      ErlDrvThreadOpts *opts);
#endif

int esdl_gl_initiated = 0;
ErlDrvTid esdl_thread;
ErlDrvMutex * esdl_batch_locker_m;
ErlDrvCond  * esdl_batch_locker_c;
void * esdl_result;

void * esdl_gl_main_loop(void * );
int es_init_opengl2(ErlDrvPort, ErlDrvTermData, char *bp);

typedef struct {
   ErlDrvTermData caller;
   int op;
   char *buff;
   int no_bins;
   char *base[3];
   ErlDrvBinary *bin[3];
   int size[3];
} esdl_q_t;

#define MAX_Q 1024
int esdl_q_first, esdl_q_n;
esdl_q_t esdl_q[MAX_Q];

/* See wx/c_src/egl_impl.h */
typedef void (*ESDL_GL_DISPATCH) (int, char *, ErlDrvPort, ErlDrvTermData, char **, int *);
ESDL_GL_DISPATCH esdl_gl_dispatch;

typedef int (*ESDL_GL_INIT) (void *);

/*** WIN32 ***/ 
#ifdef _WIN32
#define RTLD_LAZY 0
typedef HMODULE DL_LIB_P;

void * dlsym(HMODULE Lib, const char *func) 
{
    void * funcp;
    funcp = (void *) GetProcAddress(Lib, func);
    return funcp;
}

HMODULE dlopen(const char *DLL, int unused) 
{
    return LoadLibrary(DLL);
}

void DisplayErrorMsg() 
{ 
    // Retrieve the system error message for the last-error code

    LPVOID lpMsgBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    MessageBox(NULL, (LPCTSTR)lpMsgBuf, TEXT("Error"), MB_OK);
    LocalFree(lpMsgBuf);
}

#else 
typedef void * DL_LIB_P;
void DisplayErrorMsg() {
}
#endif 

/** Initialization code **/ 
void es_init_opengl(sdl_data *sd, int len, char *bp) {
    if(!sd->use_smp) {
	es_init_opengl2(sd->driver_data, 
			driver_caller(sd->driver_data), bp);
    } else {
	gl_dispatch(sd, ESDL_OpenglInitFunc, len, bp);
    }
}

int es_init_opengl2(ErlDrvPort port, ErlDrvTermData caller, char *bp) {
    DL_LIB_P LIBhandle;
    ESDL_GL_INIT init_opengl;
    int res = 0;
    ErlDrvTermData rt[8];

#ifdef _WIN32
  void * erlCallbacks = &WinDynDriverCallbacks;
#else 
  void * erlCallbacks = NULL;
#endif
   
   if(esdl_gl_initiated == 0) {
       if((LIBhandle = dlopen(bp, RTLD_LAZY))) {
	 init_opengl = (ESDL_GL_INIT) dlsym(LIBhandle, "egl_init_opengl");
	 esdl_gl_dispatch = (ESDL_GL_DISPATCH) dlsym(LIBhandle, "egl_dispatch");
	 if(init_opengl && esdl_gl_dispatch) {
	    init_opengl(erlCallbacks);
	    esdl_gl_initiated = 1;
	    res = 1;
	 } else {
	    fprintf(stderr, "In lib %s:\r\n", bp);
	    if(!init_opengl) 
	       fprintf(stderr, " function not found egl_init_opengl\r\n");
	    if(!esdl_gl_dispatch) 
	       fprintf(stderr, " function not found egl_dispatch\r\n");
	    fflush(stderr);
	    res = 0;
	 }
      } else {
	 fprintf(stderr, "Failed locating lib %s:\r\n", bp);
	 DisplayErrorMsg();
	 fflush(stderr);
	 res = 0;
      }
   } else {
       res = 2;
   }

   rt[0] = ERL_DRV_ATOM; rt[1]=driver_mk_atom((char *) "_esdl_result_");  
   rt[2] = ERL_DRV_INT; rt[3] = res;
   rt[4] = ERL_DRV_TUPLE; rt[5] = 2;
   driver_send_term(port,caller,rt,6);   
   return res;
}

void gl_dispatch(sdl_data *sd, int op, int len, char *bp) 
{
   int i;
   if(!sd->use_smp) {  /* Not SMP invoke directly */
      char * bs[3];
      int bs_sz[3];
      for(i=0; i<3; i++) {
	  bs[i] = sd->bin[i].base;
	  bs_sz[i] = sd->bin[i].size;
      }
      esdl_gl_dispatch(op, bp, sd->driver_data, driver_caller(sd->driver_data), bs, bs_sz);
   } else { 
      //printf("Dispatch cmd %d \r\n", op);
      /* SMP * copy everything and queue request to thread */
      int pos;
      erl_drv_mutex_lock(esdl_batch_locker_m);
      
      while(esdl_q_n == MAX_Q) { /* queue is full wait */
	  //fprintf(stderr, "%d: Wait \r\n", __LINE__); fflush(stderr);
	  erl_drv_cond_wait(esdl_batch_locker_c, esdl_batch_locker_m);
	  //fprintf(stderr, "%d: Wait done\r\n", __LINE__); fflush(stderr);
      }
      
      pos = (esdl_q_first + esdl_q_n) % MAX_Q;
      esdl_q[pos].op = op;
      esdl_q[pos].buff = driver_alloc(len);
      //fprintf(stderr, "%d: Q %d %d %d\r\n", __LINE__, op, pos, esdl_q_n); fflush(stderr);
      memcpy(esdl_q[pos].buff, bp, len);
      esdl_q[pos].caller = driver_caller(sd->driver_data);
      for(i=0; i< sd->next_bin; i++) {
	 esdl_q[pos].base[i] = sd->bin[i].base;
	 esdl_q[pos].bin[i]  = sd->bin[i].bin;
	 esdl_q[pos].size[i] = sd->bin[i].size;
	 driver_binary_inc_refc(sd->bin[i].bin);
      }
      esdl_q[pos].no_bins = sd->next_bin;
      esdl_q_n++;
      erl_drv_cond_signal(esdl_batch_locker_c);
      erl_drv_mutex_unlock(esdl_batch_locker_m);
   }
}


void start_opengl_thread(sdl_data *sd) 
{
   int res;
   esdl_batch_locker_m = erl_drv_mutex_create((char *)"esdl_batch_locker_m");
   esdl_batch_locker_c = erl_drv_cond_create((char *)"esdl_batch_locker_c");
   esdl_q_first = 0;
   esdl_q_n = 0;
   esdl_result = (void *) -1;
   erl_drv_mutex_lock(esdl_batch_locker_m);
#ifdef  _OSX_COCOA
   res = erl_drv_steal_main_thread("ESDL OpenGL dispatcher", &esdl_thread,
				   esdl_gl_main_loop, (void *) sd, NULL);
#else
   res = erl_drv_thread_create("ESDL OpenGL dispatcher", &esdl_thread,
			       esdl_gl_main_loop, (void *) sd, NULL);
#endif
   if(res == 0) {
      erl_drv_cond_wait(esdl_batch_locker_c, esdl_batch_locker_m);
      erl_drv_mutex_unlock(esdl_batch_locker_m);
   }
}

void stop_opengl_thread()
{
   erl_drv_mutex_lock(esdl_batch_locker_m);
   esdl_q_n = -(esdl_q_n+1);
   erl_drv_cond_signal(esdl_batch_locker_c);
   erl_drv_mutex_unlock(esdl_batch_locker_m);   
#ifdef  _OSX_COCOA
   erl_drv_stolen_main_thread_join(esdl_thread, NULL);
#else
   erl_drv_thread_join(esdl_thread, NULL);
#endif
   erl_drv_mutex_destroy(esdl_batch_locker_m);
   erl_drv_cond_destroy(esdl_batch_locker_c);
}

void * esdl_gl_main_loop(void *sd) {
   char * bs[3];
   int bs_sz[3];
   int i,j,pos;
   ErlDrvPort port = ((sdl_data *)sd)->driver_data;
   esdl_init_native_gui();
   erl_drv_mutex_lock(esdl_batch_locker_m);
   SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK );
   while(1) {
       if(esdl_q_n > 0) {
	   for(i=0; i<3; i++) {
	       bs[i] = esdl_q[esdl_q_first].base[i];
	       bs_sz[i] = esdl_q[esdl_q_first].size[i];
	   }
	   //fprintf(stderr, "%d: X %d %d %d\r\n", __LINE__, esdl_q[esdl_q_first].op,
	   //esdl_q_first, esdl_q_n); fflush(stderr);

	   if(esdl_q[esdl_q_first].op >= OPENGL_START) {
	       esdl_gl_dispatch(esdl_q[esdl_q_first].op, esdl_q[esdl_q_first].buff,
			     port, esdl_q[esdl_q_first].caller, bs, bs_sz);
	   } else {
	     /* OpenGL must be initilized in the thread */
	     switch(esdl_q[esdl_q_first].op) {
	     case SDL_GL_SwapBuffersFunc:
		   SDL_GL_SwapBuffers();
		   break;
		 /* Events must be handled in this thread on windows */
	     case SDL_PumpEventsFunc:
		   SDL_PumpEvents();
		   break;
	     case SDL_PeepEventsFunc:
		   es_peepEvents2(port, esdl_q[esdl_q_first].caller, 
				esdl_q[esdl_q_first].buff);
		 break;
	     case SDL_PollEventFunc:
		   es_pollEvent2(port, esdl_q[esdl_q_first].caller);
		   break;
	     case SDL_WaitEventFunc:
		   es_waitEvent2(port, esdl_q[esdl_q_first].caller);
		   break;	     
		 /* Other gl related functions */

	     case SDL_SetVideoModeFunc:
		   es_setVideoMode2(port, esdl_q[esdl_q_first].caller, 
				  esdl_q[esdl_q_first].buff);
		   break;
	     case SDL_VideoModeOKFunc:
		   es_videoModeOK2(port, esdl_q[esdl_q_first].caller, 
				 esdl_q[esdl_q_first].buff);
		   break;
	     case SDL_WM_ToggleFullScreenFunc:
		   es_wm_toggleFullScreen2(port, esdl_q[esdl_q_first].caller, 
					 esdl_q[esdl_q_first].buff);
		   break;
	     case ESDL_OpenglInitFunc:
		   es_init_opengl2(port, esdl_q[esdl_q_first].caller, 
				 esdl_q[esdl_q_first].buff);
		   break;
	     case SDL_GL_GetAttributeFunc:
		   es_gl_getAttribute2(port, esdl_q[esdl_q_first].caller, 
				     esdl_q[esdl_q_first].buff);
		   break;
	     case SDL_GL_SetAttributeFunc:
		   es_gl_setAttribute2(port, esdl_q[esdl_q_first].caller, 
				     esdl_q[esdl_q_first].buff);
		   break;
	     case SDL_ShowCursorFunc:
		   es_showCursor2(port, esdl_q[esdl_q_first].caller, 
				esdl_q[esdl_q_first].buff);
		   break;
	     case SDL_WM_SetCaptionFunc:
		   es_wm_setCaption2(esdl_q[esdl_q_first].buff);
		   break;
	     case SDL_WM_GetInfoFunc:
		   es_wm_getInfo2(port, esdl_q[esdl_q_first].caller, 
				esdl_q[esdl_q_first].buff);
		   break;
	     case SDL_WM_MaximizeFunc:
		   es_wm_maximize2(port, esdl_q[esdl_q_first].caller, 
				 esdl_q[esdl_q_first].buff);
		   break;
	     }
	 }
	 for(i=0; i < esdl_q[esdl_q_first].no_bins; i++)
	     driver_free_binary(esdl_q[esdl_q_first].bin[i]);
	 driver_free(esdl_q[esdl_q_first].buff);
	 // fprintf(stderr, "%d: Xed %d \r\n", __LINE__, esdl_q[esdl_q_first].op); fflush(stderr);

	 esdl_q_first++;
	 esdl_q_first %= MAX_Q;
	 esdl_q_n--;
       } else {
	   erl_drv_cond_signal(esdl_batch_locker_c);
	   // fprintf(stderr, "%d: TW\r\n", __LINE__); fflush(stderr);
	   while(esdl_q_n == 0) {
	       erl_drv_cond_wait(esdl_batch_locker_c, esdl_batch_locker_m);
	   }
	   // fprintf(stderr, "%d: TWed\r\n", __LINE__); fflush(stderr);
	   if(esdl_q_n < 0) { /* Time to quit */
	       // fprintf(stderr, "%d: T Quit\r\n", __LINE__); fflush(stderr);
	       esdl_q_n = -esdl_q_n-1;
	       break;
	   }
       }
   }
   /* Free all unused memory */
   for(i=0; i<esdl_q_n; i++) {
       pos = (esdl_q_first + i) % MAX_Q;
       driver_free(esdl_q[pos].buff);
       for(j=0; j < esdl_q[pos].no_bins; j++)
	   driver_free_binary(esdl_q[pos].bin[j]);
   }
   erl_drv_mutex_unlock(esdl_batch_locker_m);
   SDL_Quit();
#ifndef _OSX_COCOA
   erl_drv_thread_exit(NULL);
#endif
   return NULL;
}

void * esdl_gl_sync() {
   void * result;
   fprintf(stderr, "%d: GL sync start \r\n", __LINE__); fflush(stderr);
   erl_drv_mutex_lock(esdl_batch_locker_m);
   while(esdl_result == (void *) -1)
       erl_drv_cond_wait(esdl_batch_locker_c, esdl_batch_locker_m);
   result = esdl_result;
   erl_drv_mutex_unlock(esdl_batch_locker_m);
   fprintf(stderr, "%d: GL sync done \r\n", __LINE__); fflush(stderr);
   esdl_result = (void *) -1;
   return result;
}
