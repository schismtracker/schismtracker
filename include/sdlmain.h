#ifndef __sdlmain_header
#  define __sdlmain_header

/* just a fancy way to get SDL headers */

#  include <SDL.h>
#  include <SDL_thread.h>
#  include <SDL_syswm.h>
#  ifdef USE_OPENGL
#    include <SDL_opengl.h>
#  endif

#endif /* ! __sdlmain_header */
