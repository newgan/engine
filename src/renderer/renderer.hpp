#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "SDL3/SDL_video.h"
#include "glm/glm.hpp"

struct UniformBufferObject {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};

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

struct Buffer {
  vk::raii::Buffer buffer{nullptr};
  vk::raii::DeviceMemory memory{nullptr};
  void* mappedMemory{nullptr};
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

  vk::raii::CommandPool createCommandPool(vk::CommandPoolCreateFlags flags);

  Buffer createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                      vk::MemoryPropertyFlags properties);

  void createDescriptorPool();

  void createDescriptorSets();

  void createVertexBuffer();

  void createIndexBuffer();

  void createUniformBuffer();

  void updateUniformBuffer(uint32_t frameIndex);

  void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer,
                  vk::DeviceSize size);

  vk::raii::CommandBuffers createCommandBuffers(vk::raii::CommandPool& pool,
                                                int numBuffers);
  void createSyncObjects();

  void recreateSwapchain();

  uint32_t findMemoryType(uint32_t typeFilter,
                          vk::MemoryPropertyFlags properties);

  void transitionImageLayout(uint32_t imageIndex, vk::ImageLayout oldLayout,
                             vk::ImageLayout newLayout,
                             vk::AccessFlags2 srcAccessMask,
                             vk::AccessFlags2 dstAccessMask,
                             vk::PipelineStageFlags2 srcStageMask,
                             vk::PipelineStageFlags2 dstStageMask);

  vk::raii::ShaderModule createShaderModule(const std::vector<char>& code);

  void createDescriptorSetLayout();

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

  uint32_t graphicsQueueIndex{};

  // swapchain resources
  vk::raii::SwapchainKHR swapchain{nullptr};
  std::vector<vk::Image> swapchainImages;
  vk::Format swapchainImageFormat{};
  vk::Extent2D swapchainExtent{};
  std::vector<vk::raii::ImageView> swapchainImageViews;

  // command resources
  vk::raii::CommandPool commandPool{nullptr};
  vk::raii::CommandPool transientCommandPool{nullptr};
  std::vector<vk::raii::CommandBuffer> commandBuffers;

  // synchronization
  std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
  std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
  std::vector<vk::raii::Fence> inFlightFences;
  uint32_t frameIndex = 0;

  // pipeline
  vk::raii::Pipeline graphicsPipeline{nullptr};
  vk::raii::PipelineLayout pipelineLayout{nullptr};
  vk::raii::DescriptorSetLayout descriptorSetLayout{nullptr};

  // buffers
  Buffer vertexBuffer{};
  Buffer indexBuffer{};
  std::vector<Buffer> uniformBuffers;

  // resource descriptors
  vk::raii::DescriptorPool descriptorPool{nullptr};
  std::vector<vk::raii::DescriptorSet> descriptorSets;
};
