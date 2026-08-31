#define VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#include "renderer.hpp"

#include <chrono>
#include <fstream>
#include <vector>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_vulkan.h"
#include "glm/gtc/matrix_transform.hpp"

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<Vertex> vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                      {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};

const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

void Renderer::run() {
  bool done = false;

  while (!done) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        done = true;
      }
    }

    drawFrame();
  }

  device.waitIdle();
}

void Renderer::init(SDL_Window* window, int width, int height) {
  initVulkan(window, width, height);
}

void Renderer::initVulkan(SDL_Window* window, int width, int height) {
  createInstance();
  createSurface(window);
  getPhysicalDevice();
  createLogicalDevice();
  createSwapchain(width, height);

  commandPool =
      createCommandPool(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
  transientCommandPool =
      createCommandPool(vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
                        vk::CommandPoolCreateFlagBits::eTransient);

  createVertexBuffer();
  createDescriptorSetLayout();
  createUniformBuffer();
  createDescriptorPool();
  createDescriptorSets();
  createIndexBuffer();
  commandBuffers = createCommandBuffers(commandPool, MAX_FRAMES_IN_FLIGHT);
  createPipeline();
  createSyncObjects();
}

void Renderer::drawFrame() {
  auto fenceRes =
      device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
  device.resetFences(*inFlightFences[frameIndex]);

  // get next swapchain image
  auto [result, imageIndex] = swapchain.acquireNextImage(
      UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

  // record cmds
  commandBuffers[frameIndex].reset();
  updateUniformBuffer(frameIndex);
  recordCommandBuffer(imageIndex);

  vk::PipelineStageFlags waitDestinationStageMask(
      vk::PipelineStageFlagBits::eColorAttachmentOutput);
  vk::SubmitInfo submitInfo{};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &*presentCompleteSemaphores[frameIndex];
  submitInfo.pWaitDstStageMask = &waitDestinationStageMask;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &*commandBuffers[frameIndex];
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &*renderFinishedSemaphores[frameIndex];

  graphicsQueue.submit(submitInfo, *inFlightFences[frameIndex]);

  // present img
  vk::PresentInfoKHR presentInfoKHR{};
  presentInfoKHR.waitSemaphoreCount = 1,
  presentInfoKHR.pWaitSemaphores = &*renderFinishedSemaphores[frameIndex],
  presentInfoKHR.swapchainCount = 1, presentInfoKHR.pSwapchains = &*swapchain,
  presentInfoKHR.pImageIndices = &imageIndex;

  try {
    result = graphicsQueue.presentKHR(presentInfoKHR);
  } catch (const vk::SurfaceLostKHRError& e) {
    recreateSwapchain();
  }

  frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::createInstance() {
  vk::ApplicationInfo appInfo{};
  appInfo.pApplicationName = "engine";
  appInfo.apiVersion = vk::ApiVersion13;

  uint32_t instanceExtensionsCount = 0;

  char const* const* instanceExtensions =
      SDL_Vulkan_GetInstanceExtensions(&instanceExtensionsCount);

  std::vector<const char*> extensions(
      instanceExtensions, instanceExtensions + instanceExtensionsCount);

  const char* validationLayers[] = {
      "VK_LAYER_KHRONOS_validation",
  };

  vk::InstanceCreateInfo instanceCI{};
  instanceCI.pApplicationInfo = &appInfo;
  instanceCI.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  instanceCI.ppEnabledExtensionNames = extensions.data();

#ifdef DEBUG
  instanceCI.setPEnabledLayerNames(validationLayers);
#endif

  instance = vk::raii::Instance(context, instanceCI);
}

void Renderer::getPhysicalDevice() {
  auto physicalDevices = instance.enumeratePhysicalDevices();
  if (physicalDevices.empty()) {
    throw std::runtime_error("failed to find GPUS with Vulkan support!");
  }

  physicalDevice = physicalDevices[0];

  // for (auto& device : physicalDevices) {
  //   auto properties = device.getProperties();
  //
  //   // todo: better filtering + scoring to determine candidate device
  //   // option to change device manually
  //   if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
  //     physicalDevice = device;
  //   }
  // }
}

void Renderer::createLogicalDevice() {
  auto queueFamilies = physicalDevice.getQueueFamilyProperties();

  // we may need to ensure that the family has present caps
  for (int i = 0; i < queueFamilies.size(); i++) {
    auto flags = queueFamilies[i].queueFlags;

    if (flags & vk::QueueFlagBits::eGraphics) {
      graphicsQueueIndex = i;
      break;
    }
  }

  float queuePriority = 1.0f;

  vk::DeviceQueueCreateInfo queueCI{};
  queueCI.queueFamilyIndex = graphicsQueueIndex;
  queueCI.queueCount = 1;
  queueCI.pQueuePriorities = &queuePriority;

  vk::StructureChain<vk::PhysicalDeviceFeatures2,
                     vk::PhysicalDeviceVulkan11Features,
                     vk::PhysicalDeviceVulkan13Features,
                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
      featureChain{};

  featureChain.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters =
      true;

  featureChain.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering =
      true;

  featureChain.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 =
      true;

  featureChain.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
      .extendedDynamicState = true;

  std::vector<const char*> requiredDeviceExtension = {
      vk::KHRSwapchainExtensionName};

  vk::DeviceCreateInfo deviceCI{};
  deviceCI.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>();
  deviceCI.queueCreateInfoCount = 1;
  deviceCI.pQueueCreateInfos = &queueCI;
  deviceCI.enabledExtensionCount =
      static_cast<uint32_t>(requiredDeviceExtension.size());
  deviceCI.ppEnabledExtensionNames = requiredDeviceExtension.data();

  device = vk::raii::Device(physicalDevice, deviceCI);
  graphicsQueue = vk::raii::Queue(device, graphicsQueueIndex, 0);
}

void Renderer::createSurface(SDL_Window* window) {
  VkSurfaceKHR rawSurface;
  SDL_Vulkan_CreateSurface(window, *instance, nullptr, &rawSurface);
  surface = vk::raii::SurfaceKHR(instance, rawSurface);
}

void Renderer::createSwapchain(int windowWidth, int windowHeight) {
  auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);

  swapchainExtent = surfaceCapabilities.currentExtent;
  if (surfaceCapabilities.currentExtent.width == 0xFFFFFFFF) {
    swapchainExtent = {.width = static_cast<uint32_t>(windowWidth),
                       .height = static_cast<uint32_t>(windowHeight)};
  }

  uint32_t imageCount = surfaceCapabilities.minImageCount + 1;

  if (surfaceCapabilities.maxImageCount > 0 &&
      imageCount > surfaceCapabilities.maxImageCount) {
    imageCount = surfaceCapabilities.maxImageCount;
  }

  swapchainImageFormat = vk::Format::eB8G8R8A8Srgb;

  vk::SwapchainCreateInfoKHR swapchainCI{};
  swapchainCI.surface = *surface;
  swapchainCI.minImageCount = imageCount;
  swapchainCI.imageFormat = swapchainImageFormat;
  swapchainCI.imageColorSpace = vk::ColorSpaceKHR::eExtendedSrgbNonlinearEXT;
  swapchainCI.imageExtent = swapchainExtent;
  swapchainCI.imageArrayLayers = 1;
  swapchainCI.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
  swapchainCI.imageSharingMode = vk::SharingMode::eExclusive;
  swapchainCI.preTransform = surfaceCapabilities.currentTransform;
  swapchainCI.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  swapchainCI.presentMode = vk::PresentModeKHR::eMailbox;
  swapchainCI.clipped = true;

  swapchain = vk::raii::SwapchainKHR(device, swapchainCI);
  swapchainImages = swapchain.getImages();

  for (auto& image : swapchainImages) {
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.image = image;
    viewInfo.format = swapchainImageFormat;
    viewInfo.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    swapchainImageViews.emplace_back(device, viewInfo);
  }
}

vk::raii::CommandPool Renderer::createCommandPool(
    vk::CommandPoolCreateFlags flags) {
  vk::CommandPoolCreateInfo poolInfo{};
  poolInfo.flags = flags;
  poolInfo.queueFamilyIndex = graphicsQueueIndex;

  return vk::raii::CommandPool(device, poolInfo);
}

uint32_t Renderer::findMemoryType(uint32_t typeFilter,
                                  vk::MemoryPropertyFlags properties) {
  vk::PhysicalDeviceMemoryProperties memProperties =
      physicalDevice.getMemoryProperties();

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

void Renderer::createDescriptorPool() {
  vk::DescriptorPoolSize poolSize{};
  poolSize.type = vk::DescriptorType::eUniformBuffer;
  poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

  vk::DescriptorPoolCreateInfo poolInfo{};
  poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
  poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;

  descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
}

void Renderer::createDescriptorSets() {
  std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                               *descriptorSetLayout);
  vk::DescriptorSetAllocateInfo allocInfo{};
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
  allocInfo.pSetLayouts = layouts.data();

  descriptorSets = device.allocateDescriptorSets(allocInfo);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffers[i].buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = vk::WholeSize;

    vk::WriteDescriptorSet descriptorWrite{};
    descriptorWrite.dstSet = descriptorSets[i];
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
    descriptorWrite.pBufferInfo = &bufferInfo;

    device.updateDescriptorSets(descriptorWrite, {});
  }
}

Buffer Renderer::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                              vk::MemoryPropertyFlags properties) {
  vk::BufferCreateInfo bufferInfo{};
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = vk::SharingMode::eExclusive;

  Buffer buffer{};
  buffer.buffer = vk::raii::Buffer(device, bufferInfo);

  vk::MemoryRequirements memRequirements =
      buffer.buffer.getMemoryRequirements();

  vk::MemoryAllocateInfo memoryAllocateInfo{};
  memoryAllocateInfo.allocationSize = memRequirements.size;
  memoryAllocateInfo.memoryTypeIndex =
      findMemoryType(memRequirements.memoryTypeBits, properties);

  buffer.memory = vk::raii::DeviceMemory(device, memoryAllocateInfo);
  buffer.buffer.bindMemory(*buffer.memory, 0);

  return buffer;
}

void Renderer::createVertexBuffer() {
  vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  auto stagingBuffer =
      createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  void* dataStaging = stagingBuffer.memory.mapMemory(0, bufferSize);
  memcpy(dataStaging, vertices.data(), bufferSize);
  stagingBuffer.memory.unmapMemory();

  vertexBuffer = createBuffer(bufferSize,
                              vk::BufferUsageFlagBits::eVertexBuffer |
                                  vk::BufferUsageFlagBits::eTransferDst,
                              vk::MemoryPropertyFlagBits::eDeviceLocal);

  copyBuffer(stagingBuffer.buffer, vertexBuffer.buffer, bufferSize);
}

void Renderer::createIndexBuffer() {
  vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  auto stagingBuffer =
      createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  void* dataStaging = stagingBuffer.memory.mapMemory(0, bufferSize);
  memcpy(dataStaging, indices.data(), bufferSize);
  stagingBuffer.memory.unmapMemory();

  indexBuffer = createBuffer(bufferSize,
                             vk::BufferUsageFlagBits::eIndexBuffer |
                                 vk::BufferUsageFlagBits::eTransferDst,
                             vk::MemoryPropertyFlagBits::eDeviceLocal);

  copyBuffer(stagingBuffer.buffer, indexBuffer.buffer, bufferSize);
}

void Renderer::createUniformBuffer() {
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
    Buffer buffer =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent);

    buffer.mappedMemory = buffer.memory.mapMemory(0, bufferSize);
    uniformBuffers.push_back(std::move(buffer));
  }
}

void Renderer::updateUniformBuffer(uint32_t frameIndex) {
  static auto startTime = std::chrono::high_resolution_clock::now();

  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(
                   currentTime - startTime)
                   .count();

  UniformBufferObject ubo{};
  ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                          glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.view =
      glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f));

  ubo.proj = glm::perspective(
      glm::radians(45.0f),
      static_cast<float>(swapchainExtent.width /
                         static_cast<float>(swapchainExtent.height)),
      0.1f, 10.0f);

  // glm inverted y
  ubo.proj[1][1] *= -1;

  memcpy(uniformBuffers[frameIndex].mappedMemory, &ubo, sizeof(ubo));
}

void Renderer::copyBuffer(vk::raii::Buffer& srcBuffer,
                          vk::raii::Buffer& dstBuffer, vk::DeviceSize size) {
  vk::raii::CommandBuffer copyBuffer =
      std::move(createCommandBuffers(transientCommandPool, 1).front());

  copyBuffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  copyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy(0, 0, size));
  copyBuffer.end();

  vk::SubmitInfo submitInfo{};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &*copyBuffer;

  graphicsQueue.submit(submitInfo, nullptr);
  graphicsQueue.waitIdle();
}

vk::raii::CommandBuffers Renderer::createCommandBuffers(
    vk::raii::CommandPool& pool, int numBuffers) {
  vk::CommandBufferAllocateInfo allocInfo{};
  allocInfo.commandPool = pool;
  allocInfo.level = vk::CommandBufferLevel::ePrimary;
  allocInfo.commandBufferCount = numBuffers;

  return vk::raii::CommandBuffers(device, allocInfo);
}

// todo im lazy but we need this for window resizing and other shit
void Renderer::recreateSwapchain() { device.waitIdle(); }

void Renderer::createSyncObjects() {
  for (size_t i = 0; i < swapchainImages.size(); i++) {
    renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
    inFlightFences.emplace_back(
        device, vk::FenceCreateInfo{vk::FenceCreateFlagBits::eSignaled});
  }
}

std::vector<char> readFile(const std::string& filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file: " + filename);
  }

  size_t fileSize = static_cast<size_t>(file.tellg());

  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return buffer;
}

vk::raii::ShaderModule Renderer::createShaderModule(
    const std::vector<char>& code) {
  vk::ShaderModuleCreateInfo createInfo{};
  createInfo.codeSize = code.size() * sizeof(char);
  createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

  return vk::raii::ShaderModule(device, createInfo);
}

void Renderer::createDescriptorSetLayout() {
  vk::DescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

  vk::DescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &uboLayoutBinding;

  descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
}

void Renderer::createPipeline() {
  vk::raii::ShaderModule shaderModule =
      createShaderModule(readFile("slang.spv"));

  vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
  vertShaderStageInfo.module = shaderModule;
  vertShaderStageInfo.pName = "vertMain";

  vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
  fragShaderStageInfo.module = shaderModule;
  fragShaderStageInfo.pName = "fragMain";

  vk::PipelineShaderStageCreateInfo stages[] = {vertShaderStageInfo,
                                                fragShaderStageInfo};

  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();
  vk::PipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.vertexBindingDescriptionCount = 1;
  vertexInput.pVertexBindingDescriptions = &bindingDescription;
  vertexInput.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attributeDescriptions.size());
  vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();

  vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

  std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport,
                                                 vk::DynamicState::eScissor};

  vk::PipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  vk::Viewport viewport{0.0f,
                        0.0f,
                        static_cast<float>(swapchainExtent.width),
                        static_cast<float>(swapchainExtent.height),
                        0.0f,
                        1.0f};

  vk::Rect2D scissor{vk::Offset2D{0, 0}, swapchainExtent};

  vk::PipelineViewportStateCreateInfo viewportState{};
  viewportState.pViewports = &viewport;
  viewportState.pScissors = &scissor;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  vk::PipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.depthClampEnable = vk::False;
  rasterizer.rasterizerDiscardEnable = vk::False;
  rasterizer.polygonMode = vk::PolygonMode::eFill;
  rasterizer.cullMode = vk::CullModeFlagBits::eBack;
  rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
  rasterizer.depthBiasEnable = vk::False;
  rasterizer.lineWidth = 1.0f;

  vk::PipelineMultisampleStateCreateInfo multisampling{};
  multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
  multisampling.sampleShadingEnable = vk::False;

  // simple alpha blend
  vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.blendEnable = vk::True;
  colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
  colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
  colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
  colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
  colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
  colorBlendAttachment.colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  vk::PipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.logicOpEnable = vk::False;
  colorBlending.logicOp = vk::LogicOp::eCopy;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &*descriptorSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 0;

  pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

  vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
  pipelineRenderingCreateInfo.colorAttachmentCount = 1;
  pipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainImageFormat;

  vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                     vk::PipelineRenderingCreateInfoKHR>
      pipelineCreateInfoChain{};

  auto& pipelineCreateInfo =
      pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>();

  pipelineCreateInfo.stageCount = 2;
  pipelineCreateInfo.pStages = stages;
  pipelineCreateInfo.pVertexInputState = &vertexInput;
  pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
  pipelineCreateInfo.pViewportState = &viewportState;
  pipelineCreateInfo.pRasterizationState = &rasterizer;
  pipelineCreateInfo.pMultisampleState = &multisampling;
  pipelineCreateInfo.pColorBlendState = &colorBlending;
  pipelineCreateInfo.pDynamicState = &dynamicState;
  pipelineCreateInfo.layout = *pipelineLayout;
  pipelineCreateInfo.renderPass = nullptr;

  auto& renderingCreateInfo =
      pipelineCreateInfoChain.get<vk::PipelineRenderingCreateInfoKHR>();

  renderingCreateInfo.colorAttachmentCount = 1;
  renderingCreateInfo.pColorAttachmentFormats = &swapchainImageFormat;

  graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfo);
}

void Renderer::transitionImageLayout(uint32_t imageIndex,
                                     vk::ImageLayout oldLayout,
                                     vk::ImageLayout newLayout,
                                     vk::AccessFlags2 srcAccessMask,
                                     vk::AccessFlags2 dstAccessMask,
                                     vk::PipelineStageFlags2 srcStageMask,
                                     vk::PipelineStageFlags2 dstStageMask) {
  vk::ImageMemoryBarrier2 barrier{};

  barrier.srcStageMask = srcStageMask;
  barrier.srcAccessMask = srcAccessMask;
  barrier.dstStageMask = dstStageMask;
  barrier.dstAccessMask = dstAccessMask;

  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;

  barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
  barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;

  barrier.image = swapchainImages[imageIndex];

  barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  vk::DependencyInfo depInfo{};

  depInfo.dependencyFlags = {};
  depInfo.imageMemoryBarrierCount = 1;
  depInfo.pImageMemoryBarriers = &barrier;

  commandBuffers[frameIndex].pipelineBarrier2(depInfo);
}

void Renderer::recordCommandBuffer(uint32_t imageIndex) {
  commandBuffers[frameIndex].begin({});

  transitionImageLayout(imageIndex, vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eColorAttachmentOptimal, {},
                        vk::AccessFlagBits2::eColorAttachmentWrite,
                        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits2::eColorAttachmentOutput);

  vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
  vk::RenderingAttachmentInfo attachmentInfo = {};
  attachmentInfo.imageView = swapchainImageViews[imageIndex];
  attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
  attachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
  attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
  attachmentInfo.clearValue = clearColor;

  vk::RenderingInfo renderingInfo = {};
  renderingInfo.renderArea = {.offset = {0, 0}, .extent = swapchainExtent};
  renderingInfo.layerCount = 1;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.pColorAttachments = &attachmentInfo;

  commandBuffers[frameIndex].beginRendering(renderingInfo);

  commandBuffers[frameIndex].bindPipeline(vk::PipelineBindPoint::eGraphics,
                                          *graphicsPipeline);

  commandBuffers[frameIndex].setViewport(
      0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapchainExtent.width),
                      static_cast<float>(swapchainExtent.height), 0.0f, 1.0f));
  commandBuffers[frameIndex].setScissor(
      0, vk::Rect2D(vk::Offset2D(0, 0), swapchainExtent));

  commandBuffers[frameIndex].bindVertexBuffers(0, *vertexBuffer.buffer, {0});
  commandBuffers[frameIndex].bindIndexBuffer(*indexBuffer.buffer, 0,
                                             vk::IndexType::eUint16);

  commandBuffers[frameIndex].bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, pipelineLayout, 0,
      *descriptorSets[frameIndex], nullptr);

  commandBuffers[frameIndex].drawIndexed(static_cast<uint32_t>(indices.size()),
                                         1, 0, 0, 0);

  commandBuffers[frameIndex].endRendering();

  transitionImageLayout(imageIndex, vk::ImageLayout::eColorAttachmentOptimal,
                        vk::ImageLayout::ePresentSrcKHR,
                        vk::AccessFlagBits2::eColorAttachmentWrite, {},
                        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits2::eBottomOfPipe);

  commandBuffers[frameIndex].end();
}
