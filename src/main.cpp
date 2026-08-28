#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer.hpp"
#include "window.hpp"

constexpr int windowWidth = 1280;
constexpr int windowHeight = 720;

int main(int argc, char* argv[]) {
  Window window(windowWidth, windowHeight);

  Renderer renderer;
  renderer.init(window.getHandle(), windowWidth, windowHeight);
  renderer.run();

  return 0;
}
