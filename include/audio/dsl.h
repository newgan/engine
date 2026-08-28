#pragma once
#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_stdinc.h"
#include "audio/engine.h"

inline float computeSineWave(WaveData* data){
  float output = 0.0f;
  float wave = SDL_sinf(data->phase * 2 * SDL_PI_F);
  data->phase += data->freq / data->rate;   
  if (data->phase >= 1.0f) { data->phase -= 1.0f; }
  data->isPlaying ? output = wave * data->volume : output = wave * 0.0f;
  return output;
}