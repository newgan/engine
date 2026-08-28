#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "SDL3/SDL_video.h"
#include "glm/glm.hpp"

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;

  static vk::VertexInputBindingDescription getBindingDescription() {
    vk::VertexInputBindingDescription desc{};
    desc.binding = 0;
    desc.stride = sizeof(Vertex);
    desc.inputRate = vk::VertexInputRate::eVertex;

    return desc;
  }

  static std::array<vk::VertexInputAttributeDescription, 2>
  getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 2> desc{};

    desc[0].location = 0;
    desc[0].binding = 0;
    desc[0].format = vk::Format::eR32G32Sfloat;
    desc[0].offset = offsetof(Vertex, pos);

    desc[1].location = 1;
    desc[1].binding = 0;
    desc[1].format = vk::Format::eR32G32B32Sfloat;
    desc[1].offset = offsetof(Vertex, color);

    return desc;
  }
};

class Renderer {
 public:
  void run();

  void init(SDL_Window* window, int width, int height);

 private:
  void initVulkan(SDL_Window* window, int width, int height);

  void drawFrame();

  void createInstance();

  void getPhysicalDevice();

  void createLogicalDevice();

  void createSurface(SDL_Window* window);

  void createSwapchain(int windowWidth, int windowHeight);

  void createCommandPool();

  uint32_t findMemoryType(uint32_t typeFilter,
                          vk::MemoryPropertyFlags properties);

  void createVertexBuffer();

  void createCommandBuffers();

  void createSyncObjects();

  void recreateSwapchain();

  void transitionImageLayout(uint32_t imageIndex, vk::ImageLayout oldLayout,
                             vk::ImageLayout newLayout,
                             vk::AccessFlags2 srcAccessMask,
                             vk::AccessFlags2 dstAccessMask,
                             vk::PipelineStageFlags2 srcStageMask,
                             vk::PipelineStageFlags2 dstStageMask);

  vk::raii::ShaderModule createShaderModule(const std::vector<char>& code);

  void createPipeline();

  void recordCommandBuffer(uint32_t imageIndex);

 private:
  vk::raii::Context context;
  vk::raii::Instance instance{nullptr};

  // owned by instance
  vk::raii::SurfaceKHR surface{nullptr};
  vk::raii::PhysicalDevice physicalDevice{nullptr};

  // device
  vk::raii::Device device{nullptr};
  vk::raii::Queue graphicsQueue{nullptr};

  uint32_t graphicsQueueFamily{};

  // swapchain resources
  vk::raii::SwapchainKHR swapchain{nullptr};
  std::vector<vk::Image> swapchainImages;
  vk::Format swapchainImageFormat{};
  vk::Extent2D swapchainExtent{};
  std::vector<vk::raii::ImageView> swapchainImageViews;

  // command resources
  vk::raii::CommandPool commandPool{nullptr};
  std::vector<vk::raii::CommandBuffer> commandBuffers;

  // synchronization
  std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
  std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
  std::vector<vk::raii::Fence> inFlightFences;
  uint32_t frameIndex = 0;

  // pipeline
  vk::raii::Pipeline graphicsPipeline{nullptr};
  vk::raii::PipelineLayout pipelineLayout{nullptr};

  // buffers
  vk::raii::Buffer vertexBuffer{nullptr};
  vk::raii::DeviceMemory vertexBufferMemory{nullptr};
};
