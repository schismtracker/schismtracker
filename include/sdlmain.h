#ifndef SCHISM_SDLMAIN_H_
#define SCHISM_SDLMAIN_H_

#include <SDL.h>
#include <SDL_thread.h>
#ifdef vector
# undef vector
#endif
#include <SDL_syswm.h>

/* use 64-bit functions only if they're available! */
#if SDL_VERSION_ATLEAST(2, 0, 18)
#define SCHISM_TICKS_PASSED(a, b) ((a) >= (b))
#define SCHISM_GET_TICKS() SDL_GetTicks64()
typedef uint64_t schism_ticks_t;
#else
#define SCHISM_TICKS_PASSED(a, b) (SDL_TICKS_PASSED(a, b))
#define SCHISM_GET_TICKS() SDL_GetTicks()
typedef uint32_t schism_ticks_t;
#endif

#endif /* SCHISM_SDLMAIN_H_ */
