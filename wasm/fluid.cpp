// Copyright 2011 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <stdio.h>
#include <SDL/SDL.h>
#include <stdlib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define IX(i,j) ((i)+(N+2)*(j))
#define SWAP(x0,x) {float *tmp=x0;x0=x;x=tmp;}

const int N=4;
const int size=(N+2)*(N+2);
bool init=false;
int t=0;
SDL_Surface *screen;

// Set bnd_offset to 0 if you don't want boundary squares to display, 1 if you do
// int bnd_offset = 0;
int bnd_offset = 1;

int canvas_width = 456;
int canvas_height = 456;
int pixel_size = fmin(canvas_width, canvas_height)/(N+(2*bnd_offset));

float u[size], v[size], u_prev[size], v_prev[size];
float dens[size], dens_prev[size];
float visc = 20;
float diff = 20;
float dt = 0.002;

void draw_dens(int N, float dens[size]) {
  t++;

#ifdef TEST_SDL_LOCK_OPTS
  EM_ASM("SDL.defaults.copyOnLock = false; SDL.defaults.discardOnLock = true; SDL.defaults.opaqueFrontBuffer = false;");
#endif

  if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
  for (int i = 0; i < canvas_width; i++) {
    for (int j = 0; j < canvas_height; j++) {
#ifdef TEST_SDL_LOCK_OPTS
      // Alpha behaves like in the browser, so write proper opaque pixels.
      int alpha = 255;
#else
      // To emulate native behavior with blitting to screen, alpha component is ignored. Test that it is so by outputting
      // data (and testing that it does get discarded)
      int alpha = (i+j) % 255;
#endif

      int pixel_x = (i / pixel_size)+(1-bnd_offset);
      int pixel_y = (j / pixel_size)+(1-bnd_offset);
      int shade;
      if (pixel_x > N+2 || pixel_y > N+2) {
        shade = 255;
      } else {
        shade = 255*dens[IX(pixel_x,pixel_y)];
      }
      *((Uint32*)screen->pixels + i * canvas_width + j) = SDL_MapRGBA(screen->format, shade, shade, shade, alpha);
    }
  }
  if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_Flip(screen);
}

void step() {
  draw_dens ( N, dens );
}

extern "C" int main(int argc, char** argv) {

  if (!init) {
    SDL_Init(SDL_INIT_VIDEO);
    screen = SDL_SetVideoMode(canvas_width, canvas_height, 32, SDL_SWSURFACE);
    init = true;
  }

  for (int i = 0; i < size; i++) {
    dens[i] = emscripten_random();
  }

  emscripten_set_main_loop(step, 0, 1);

  return 0;
}
