#pragma once
#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_stdinc.h"

typedef struct WaveData {
  float freq;
  float phase = 0;
  float volume;
} WaveData;

void oscillator_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount);