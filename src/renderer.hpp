#include <vector>

#include "SDL3/SDL_video.h"
#include "vulkan/vulkan_core.h"

class Renderer {
 public:
  void run();

  void init(SDL_Window* window, int width, int height);
  void cleanup();

 private:
  void initVulkan(SDL_Window* window, int width, int height);

  void draw();

  void createInstance();

  void getPhysicalDevice();

  void createLogicalDevice();

  void createSurface(SDL_Window* window);

  void createSwapchain(int windowWidth, int windowHeight);

  void createCommandPool();

  void createCommandBuffers();

  void createSyncObjects();

  VkShaderModule createShaderModule(const std::vector<char>& code);

  void createPipeline();

  void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

 private:
  VkInstance instance{};
  VkPhysicalDevice physicalDevice{};
  VkDevice device{};

  VkQueue graphicsQueue{};
  uint32_t graphicsQueueFamily{};

  VkSurfaceKHR surface{};
  VkSwapchainKHR swapchain{};
  std::vector<VkImage> swapchainImages;
  VkFormat swapchainImageFormat{};
  VkExtent2D swapchainExtent{};
  std::vector<VkImageView> swapchainImageViews;

  VkCommandPool commandPool{};
  VkCommandBuffer commandBuffer{};

  VkSemaphore imageAvailableSemaphore{};
  VkSemaphore renderFinishedSemaphore{};
  VkFence inFlightFence{};

  VkShaderModule vertexShader{};
  VkShaderModule fragmentShader{};

  VkPipeline graphicsPipeline{};
  VkPipelineLayout pipelineLayout{};
};
