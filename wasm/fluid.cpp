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


void set_bnd ( int N, int b, float * x )
{
  int i;
  for ( i=1 ; i<=N ; i++ ) {
    x[IX(0 ,i)] = b==1 ? -x[IX(1,i)] : x[IX(1,i)];
    x[IX(N+1,i)] = b==1 ? -x[IX(N,i)] : x[IX(N,i)];
    x[IX(i,0 )] = b==2 ? -x[IX(i,1)] : x[IX(i,1)];
    x[IX(i,N+1)] = b==2 ? -x[IX(i,N)] : x[IX(i,N)];
  }
  x[IX(0 ,0 )] = 0.5*(x[IX(1,0 )]+x[IX(0 ,1)]);
  x[IX(0 ,N+1)] = 0.5*(x[IX(1,N+1)]+x[IX(0 ,N )]);
  x[IX(N+1,0 )] = 0.5*(x[IX(N,0 )]+x[IX(N+1,1)]);
  x[IX(N+1,N+1)] = 0.5*(x[IX(N,N+1)]+x[IX(N+1,N )]);
}

void diffuse ( int N, int b, float * x, float * x0, float diff, float dt )
{
  int i, j, k;
  float a=dt*diff*N*N;
  for ( k=0 ; k<20 ; k++ ) {
    for ( i=1 ; i<=N ; i++ ) {
      for ( j=1 ; j<=N ; j++ ) {
        x[IX(i,j)] = (x0[IX(i,j)] + a*(x[IX(i-1,j)]+x[IX(i+1,j)]+
          x[IX(i,j-1)]+x[IX(i,j+1)]))/(1+4*a);
        }
      }
    set_bnd ( N, b, x );
  }
}

void advect ( int N, int b, float * d, float * d0, float * u, float * v, float dt )
{
  int i, j, i0, j0, i1, j1;
  float x, y, s0, t0, s1, t1, dt0;
  dt0 = dt*N;
  for ( i=1 ; i<=N ; i++ ) {
    for ( j=1 ; j<=N ; j++ ) {
      x = i-dt0*u[IX(i,j)]; y = j-dt0*v[IX(i,j)];
      if (x<0.5) x=0.5; if (x>N+0.5) x=N+ 0.5; i0=(int)x; i1=i0+1;
      if (y<0.5) y=0.5; if (y>N+0.5) y=N+ 0.5; j0=(int)y; j1=j0+1;
      s1 = x-i0; s0 = 1-s1; t1 = y-j0; t0 = 1-t1;
      d[IX(i,j)] = s0*(t0*d0[IX(i0,j0)]+t1*d0[IX(i0,j1)])+
        s1*(t0*d0[IX(i1,j0)]+t1*d0[IX(i1,j1)]);
    }
  }
  set_bnd ( N, b, d );
}

void project ( int N, float * u, float * v, float * p, float * div )
{
  int i, j, k;
  float h;
  h = 1.0/N;
  for ( i=1 ; i<=N ; i++ ) {
    for ( j=1 ; j<=N ; j++ ) {
      div[IX(i,j)] = -0.5*h*(u[IX(i+1,j)]-u[IX(i-1,j)]+
        v[IX(i,j+1)]-v[IX(i,j-1)]);
        p[IX(i,j)] = 0;
    }
  }
  set_bnd ( N, 0, div ); set_bnd ( N, 0, p );
  for ( k=0 ; k<20 ; k++ ) {
    for ( i=1 ; i<=N ; i++ ) {
      for ( j=1 ; j<=N ; j++ ) {
        p[IX(i,j)] = (div[IX(i,j)]+p[IX(i-1,j)]+p[IX(i+1,j)]+
          p[IX(i,j-1)]+p[IX(i,j+1)])/4;
      }
    }
    set_bnd ( N, 0, p );
  }
  for ( i=1 ; i<=N ; i++ ) {
    for ( j=1 ; j<=N ; j++ ) {
      u[IX(i,j)] -= 0.5*(p[IX(i+1,j)]-p[IX(i-1,j)])/h;
      v[IX(i,j)] -= 0.5*(p[IX(i,j+1)]-p[IX(i,j-1)])/h;
    }
  }
  set_bnd ( N, 1, u ); set_bnd ( N, 2, v );
}

void add_source ( int N, float * x, float * s, float dt )
{
  int i, size=(N+2)*(N+2);
  for ( i=0 ; i<size ; i++ ) x[i] += dt*s[i];
}

void dens_step ( int N, float * x, float * x0, float * u, float * v, float diff,
float dt )
{
  add_source ( N, x, x0, dt );
  SWAP ( x0, x ); diffuse ( N, 0, x, x0, diff, dt );
  SWAP ( x0, x ); advect ( N, 0, x, x0, u, v, dt );
}

void vel_step ( int N, float * u, float * v, float * u0, float * v0,
float visc, float dt )
{
  add_source ( N, u, u0, dt ); add_source ( N, v, v0, dt );
  SWAP ( u0, u ); diffuse ( N, 1, u, u0, visc, dt );
  SWAP ( v0, v ); diffuse ( N, 2, v, v0, visc, dt );
  project ( N, u, v, u0, v0 );
  SWAP ( u0, u ); SWAP ( v0, v );
  advect ( N, 1, u, u0, u0, v0, dt ); advect ( N, 2, v, v0, u0, v0, dt );
  project ( N, u, v, u0, v0 );
}

const int N=100;
const int size=(N+2)*(N+2);
bool init=false;
int t=0;
SDL_Surface *screen;

// Set bnd_offset to 0 if you don't want boundary squares to display, 1 if you do
// int bnd_offset = 0;
int bnd_offset = 1;

int canvas_width = 756;
int canvas_height = 756;
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
        float unweightedSmoke = 1-(dens[IX(pixel_x,pixel_y)]);
        float weightedSmoke = unweightedSmoke*unweightedSmoke*unweightedSmoke;
        shade = 255*weightedSmoke;
      }
      *((Uint32*)screen->pixels + i * canvas_width + j) = SDL_MapRGBA(screen->format, shade, shade, shade, alpha);
    }
  }
  if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
  SDL_Flip(screen);
}

void step() {
  // get_from_UI ( dens_prev, u_prev, v_prev );
  vel_step ( N, u, v, u_prev, v_prev, visc, dt );
  dens_step ( N, dens, dens_prev, u, v, diff, dt );
  draw_dens ( N, dens );
}

// density: function(x,y) {
//       if (
//         x >= Math.floor(arrWidth/2.0)-(arrWidth/30) &&
//           x <= Math.floor(arrWidth/2.0)+(arrWidth/30) &&
//           y >= arrHeight-(arrHeight/30)
//         ) { return 1; }
//       return 0;
//     },
//     velocityU: (x,y) => {
//       if (
//         x >= Math.floor(arrWidth/2.0)-(arrWidth/30) &&
//           x <= Math.floor(arrWidth/2.0)+(arrWidth/30) &&
//           y >= arrHeight-(arrHeight/30)
//         ) { return 10; }
//       return 0;
//     },
//     velocityV: (x,y) => {
//       if (
//         x >= Math.floor(arrWidth/2.0)-(arrWidth/30) &&
//           x <= Math.floor(arrWidth/2.0)+(arrWidth/30) &&
//           y >= arrHeight-(arrHeight/30)
//         ) { return -100; }
//       return 0;
//     },

void apply_velocity_template() {
  int middle = (N+2)/2;
  float lengthUnit = (N+2)/30;
  for (int i=0; i<N+2; i++) {
    for (int j=0; j<N+2; j++) {
      if (
        j >= middle-lengthUnit &&
          j <= middle+lengthUnit &&
          i >= (N+2)-lengthUnit
      ) {
        u[IX(i,j)] = 10.0;
        u_prev[IX(i,j)] = 10.0;
        v[IX(i,j)] = -100.0;
        v_prev[IX(i,j)] = -100.0;
      } else {
        u[IX(i,j)] = 0.0;
        u_prev[IX(i,j)] = 0.0;
        v[IX(i,j)] = 0.0;
        v_prev[IX(i,j)] = 0.0;
      }
    }
  }
}

void apply_density_template() {
  int middle = (N+2)/2;
  float lengthUnit = (N+2)/30;
  for (int i=0; i<N+2; i++) {
    for (int j=0; j<N+2; j++) {
      if (
        j >= middle-lengthUnit &&
          j <= middle+lengthUnit &&
          i >= (N+2)-lengthUnit
      ) {
        dens[IX(i,j)] = 1;
        dens_prev[IX(i,j)] = 1;
      } else {
        dens[IX(i,j)] = 0;
        dens_prev[IX(i,j)] = 0;
      }
    }
  }
}

// void apply_density_template() {
//   int middle = (N+2)/2;
//   for (int i=0; i<N+2; i++) {
//     for (int j=0; j<N+2; j++) {
//       if (sqrt(pow(i-middle, 2) + pow(j-middle,2)) < 15) {
//         dens[IX(i,j)] = 1;
//         dens_prev[IX(i,j)] = 1;
//       } else {
//         dens[IX(i,j)] = 0;
//         dens_prev[IX(i,j)] = 0;
//       }
//     }
//   }
// }

extern "C" int main(int argc, char** argv) {

  if (!init) {
    SDL_Init(SDL_INIT_VIDEO);
    screen = SDL_SetVideoMode(canvas_width, canvas_height, 32, SDL_SWSURFACE);
    init = true;
  }

  apply_density_template();
  apply_velocity_template();

  emscripten_set_main_loop(step, 0, 0);

  return 0;
}
