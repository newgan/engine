#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"

class Window {
 public:
  Window(int width, int height) {
    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow("engine", width, height, SDL_WINDOW_VULKAN);
  }

  ~Window() {
    SDL_DestroyWindow(window);
    SDL_Quit();
  }

  SDL_Window* getHandle() const { return window; }

 private:
  SDL_Window* window{};
};
