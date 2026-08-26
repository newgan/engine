#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <iostream>
#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_oldnames.h"
#include "SDL3/SDL_stdinc.h"
#include "audio/engine.h"
#include "input/action.h"

int main(int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    std::cout << "Failed to init SDL. \n";
  }

  WaveData wd = { .freq = 440.25f, .phase = 0.0f, .volume = 0.8f, .rate = 48000.0f}; // define audio data
  AUDIO_Init(&wd);
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
    ACTION_HandleInput();
  }

  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
