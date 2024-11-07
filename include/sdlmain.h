#ifndef SCHISM_SDLMAIN_H_
#define SCHISM_SDLMAIN_H_

#ifdef SCHISM_SDL2
#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_syswm.h>
#else
#include <SDL/SDL.h>
// hax
#define SDL_CreateThread(a, b, c) SDL_CreateThread(a, c)
#define SDL_SetThreadPriority(a)
#endif

#endif /* SCHISM_SDLMAIN_H_ */
