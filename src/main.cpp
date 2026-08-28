#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <iostream>
#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_oldnames.h"
#include "SDL3/SDL_stdinc.h"
#include "audio/engine.h"
#include "input/action.h"
#include "input/frame_input.h"

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

  SDL_StartTextInput(window);

  bool done = false;
  while (!done) {
    FrameInput input;
    input.key_states = SDL_GetKeyboardState(NULL);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch(event.type) {
        case SDL_EVENT_QUIT:
          done = true;
          break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
          input.clicked = true;
          input.click_x = event.button.x;
          input.click_y = event.button.y;
          break;
        case SDL_EVENT_TEXT_INPUT:
          input.text_input += event.text.text;
          break;
      }
    }
    ACTION_HandleInput(input);
  }

  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
