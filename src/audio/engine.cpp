#include <numbers>
#include <cmath>
#include "SDL3/SDL_stdinc.h"
#include "audio/engine.h"
#include "audio/dsl.h"

const int BUFFER_SIZE = 4096;
const int SAMPLE_SIZE = 44100;

static WaveData* wd = nullptr;
static SDL_AudioStream *stream = nullptr;
void AUDIO_Init(WaveData* data){
  wd = data;
  SDL_AudioSpec spec;
  SDL_zero(spec);
  spec.freq = wd->rate;
  spec.format = SDL_AUDIO_F32;
  spec.channels = 1;
  stream = SDL_OpenAudioDeviceStream // declare audio stream
  (
    SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, 
    &spec, 
    oscillator_callback, // sdl launches background audio thread that continuously calls this
    wd
  );

  SDL_ResumeAudioStreamDevice(stream); // unpause stream
}

void AUDIO_SetPlaying(const bool *keyPressed){
  if (keyPressed){
    wd->isPlaying = true;
  } else {
    wd->isPlaying = false;
  }
}

void oscillator_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount){
    additional_amount /= sizeof (float);  // convert from bytes to samples 
    WaveData *data = (WaveData *)userdata;
    while (additional_amount > 0) {
        float samples[128];  // feed 128 frames at a time
        const int total = SDL_min(additional_amount, SDL_arraysize(samples));
        // generate a 440Hz sine wave
        for (int i = 0; i < total; i++) {
            float wave = computeSineWave(data->phase);
            data->isPlaying ? samples[i] = wave * data->volume : samples[i] = wave * 0.0f;
            data->phase += data->freq / data->rate;   
            if (data->phase >= 1.0f) { data->phase -= 1.0f; }
      }

        // feed the new data to the stream.
        SDL_PutAudioStreamData(stream, samples, total * sizeof (float));
        additional_amount -= total;
    }
}