#pragma once
#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_stdinc.h"

float computeSineWave(float phase){
  return SDL_sinf(phase * 2 * SDL_PI_F);
}