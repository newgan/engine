#define VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#include "renderer.hpp"

#include <fstream>
#include <vector>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_vulkan.h"

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<Vertex> vertices = {{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

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
  createCommandPool();
  createVertexBuffer();
  createCommandBuffers();
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

  result = graphicsQueue.presentKHR(presentInfoKHR);

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
    if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) {
      graphicsQueueFamily = i;
    }
  }

  float queuePriority = 1.0f;

  vk::DeviceQueueCreateInfo queueCI{};
  queueCI.queueFamilyIndex = graphicsQueueFamily;
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
  graphicsQueue = vk::raii::Queue(device, graphicsQueueFamily, 0);
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

void Renderer::createCommandPool() {
  vk::CommandPoolCreateInfo poolInfo{};
  poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
  poolInfo.queueFamilyIndex = graphicsQueueFamily;

  commandPool = vk::raii::CommandPool(device, poolInfo);
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

void Renderer::createVertexBuffer() {
  vk::BufferCreateInfo bufferInfo{};
  bufferInfo.size = sizeof(vertices[0]) * vertices.size();
  bufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer;
  bufferInfo.sharingMode = vk::SharingMode::eExclusive;

  vertexBuffer = vk::raii::Buffer(device, bufferInfo);

  vk::MemoryRequirements memRequirements = vertexBuffer.getMemoryRequirements();

  vk::MemoryAllocateInfo memoryAllocateInfo{};
  memoryAllocateInfo.allocationSize = memRequirements.size;
  memoryAllocateInfo.memoryTypeIndex =
      findMemoryType(memRequirements.memoryTypeBits,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent);

  vertexBufferMemory = vk::raii::DeviceMemory(device, memoryAllocateInfo);
  vertexBuffer.bindMemory(*vertexBufferMemory, 0);

  void* data = vertexBufferMemory.mapMemory(0, bufferInfo.size);
  memcpy(data, vertices.data(), bufferInfo.size);
  vertexBufferMemory.unmapMemory();
}

void Renderer::createCommandBuffers() {
  vk::CommandBufferAllocateInfo allocInfo{};
  allocInfo.commandPool = commandPool;
  allocInfo.level = vk::CommandBufferLevel::ePrimary;
  allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

  commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
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
  rasterizer.frontFace = vk::FrontFace::eClockwise;
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
  pipelineLayoutInfo.setLayoutCount = 0;
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

  commandBuffers[frameIndex].bindVertexBuffers(0, *vertexBuffer, {0});

  commandBuffers[frameIndex].draw(static_cast<uint32_t>(vertices.size()), 1, 0,
                                  0);

  commandBuffers[frameIndex].endRendering();

  transitionImageLayout(imageIndex, vk::ImageLayout::eColorAttachmentOptimal,
                        vk::ImageLayout::ePresentSrcKHR,
                        vk::AccessFlagBits2::eColorAttachmentWrite, {},
                        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits2::eBottomOfPipe);

  commandBuffers[frameIndex].end();
}
