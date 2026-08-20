#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <iostream>
#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_oldnames.h"
#include "SDL3/SDL_stdinc.h"
#include "audio.h"

int main(int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    std::cout << "Failed to init SDL. \n";
  }

  WaveData wd = { .phase = 0.0f, .freq = 440.0f, .volume = 0.8f}; // define audio data
  SDL_AudioSpec spec;
  SDL_zero(spec);
  spec.freq = 48000.0f;
  spec.format = SDL_AUDIO_F32;
  spec.channels = 1;
  SDL_AudioStream *stream = SDL_OpenAudioDeviceStream // declare audio stream
  (
    SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, 
    &spec, 
    oscillator_callback, // sdl launches background audio thread that continuously calls this
    &wd
  );

  SDL_ResumeAudioStreamDevice(stream); // unpause stream

  SDL_Window *window = SDL_CreateWindow("engine", 640, 480, 0);

  if (!window) {
    std::cout << "Failed to init window.";
  }

  bool done = false;

  while (!done) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        done = true;
      }
    }
  }

  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
