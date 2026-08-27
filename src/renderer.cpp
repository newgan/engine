#include "renderer.hpp"

#include <fstream>
#include <vector>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_vulkan.h"

void Renderer::run() {
  bool done = false;

  while (!done) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        done = true;
      }
    }

    draw();
  }

  vkDeviceWaitIdle(device);
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
  createCommandBuffers();
  createPipeline();
  createSyncObjects();
}

void Renderer::draw() {
  vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
  vkResetFences(device, 1, &inFlightFence);

  // get next swapchain image
  uint32_t imageIndex;

  vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore,
                        VK_NULL_HANDLE, &imageIndex);

  // record cmds
  recordCommandBuffer(commandBuffer, imageIndex);

  VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  // submit to gpu
  VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                          .waitSemaphoreCount = 1,
                          .pWaitSemaphores = &imageAvailableSemaphore,
                          .pWaitDstStageMask = &waitStage,
                          .commandBufferCount = 1,
                          .pCommandBuffers = &commandBuffer,
                          .signalSemaphoreCount = 1,
                          .pSignalSemaphores = &renderFinishedSemaphore};

  vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);

  // present img
  VkPresentInfoKHR presentInfo{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                               .waitSemaphoreCount = 1,
                               .pWaitSemaphores = &renderFinishedSemaphore,
                               .swapchainCount = 1,
                               .pSwapchains = &swapchain,
                               .pImageIndices = &imageIndex};

  vkQueuePresentKHR(graphicsQueue, &presentInfo);
}

void Renderer::cleanup() {
  vkDeviceWaitIdle(device);

  vkDestroyFence(device, inFlightFence, nullptr);
  vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
  vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);

  vkDestroyPipeline(device, graphicsPipeline, nullptr);
  vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

  for (VkImageView imageView : swapchainImageViews) {
    vkDestroyImageView(device, imageView, nullptr);
  }

  vkDestroyCommandPool(device, commandPool, nullptr);

  vkDestroySwapchainKHR(device, swapchain, nullptr);
  vkDestroyDevice(device, nullptr);

  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkDestroyInstance(instance, nullptr);
}

void Renderer::createInstance() {
  VkApplicationInfo appInfo{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                            .pApplicationName = "engine",
                            .apiVersion = VK_API_VERSION_1_3};

  uint32_t instanceExtensionsCount = 0;
  char const* const* instanceExtensions{
      SDL_Vulkan_GetInstanceExtensions(&instanceExtensionsCount)};

  const char* validationLayers[] = {
      "VK_LAYER_KHRONOS_validation",
  };

  VkInstanceCreateInfo instanceCI{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &appInfo,
      .enabledExtensionCount = instanceExtensionsCount,
      .ppEnabledExtensionNames = instanceExtensions,
  };

#ifdef DEBUG
  // TODO: we should probably enable/disable specific layers eventually, but im
  // lazy
  createInfo.enabledLayerCount =
      static_cast<uint32_t>(std::size(validationLayers));
  createInfo.ppEnabledLayerNames = validationLayers;
#endif

  vkCreateInstance(&instanceCI, nullptr, &instance);
}

void Renderer::getPhysicalDevice() {
  uint32_t deviceCount = 0;

  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

  std::vector<VkPhysicalDevice> devices(deviceCount);

  vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

  physicalDevice = devices[0];
}

void Renderer::createLogicalDevice() {
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);

  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           queueFamilies.data());

  for (uint32_t i = 0; i < queueFamilyCount; ++i) {
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      graphicsQueueFamily = i;
      break;
    }
  }

  float queuePriority = 1.0f;

  VkDeviceQueueCreateInfo queueCI{
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = graphicsQueueFamily,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority};

  const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  VkPhysicalDeviceVulkan13Features features13{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .synchronization2 = VK_TRUE,
      .dynamicRendering = VK_TRUE};

  VkDeviceCreateInfo deviceCI{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                              .pNext = &features13,
                              .queueCreateInfoCount = 1,
                              .pQueueCreateInfos = &queueCI,
                              .enabledExtensionCount = 1,
                              .ppEnabledExtensionNames = deviceExtensions};

  vkCreateDevice(physicalDevice, &deviceCI, nullptr, &device);

  vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
}

void Renderer::createSurface(SDL_Window* window) {
  SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);
}

void Renderer::createSwapchain(int windowWidth, int windowHeight) {
  VkSurfaceCapabilitiesKHR surfaceCaps{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                            &surfaceCaps);

  swapchainExtent = surfaceCaps.currentExtent;
  if (surfaceCaps.currentExtent.width == 0xFFFFFFFF) {
    swapchainExtent = {.width = static_cast<uint32_t>(windowWidth),
                       .height = static_cast<uint32_t>(windowHeight)};
  }

  swapchainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;

  VkSwapchainCreateInfoKHR swapchainCI{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface,
      .minImageCount = surfaceCaps.minImageCount,
      .imageFormat = swapchainImageFormat,
      .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
      .imageExtent = swapchainExtent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .preTransform = surfaceCaps.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR};

  vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapchain);

  uint32_t imageCount = 0;
  vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
  swapchainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(device, swapchain, &imageCount,
                          swapchainImages.data());

  swapchainImageViews.resize(swapchainImages.size());

  for (size_t i = 0; i < swapchainImages.size(); ++i) {
    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = swapchainImages[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchainImageFormat,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]);
  }
}

void Renderer::createCommandPool() {
  VkCommandPoolCreateInfo commandPoolCI{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = graphicsQueueFamily};
  vkCreateCommandPool(device, &commandPoolCI, nullptr, &commandPool);
}

void Renderer::createCommandBuffers() {
  VkCommandBufferAllocateInfo cbAllocCI{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool,
      .commandBufferCount = 1};
  vkAllocateCommandBuffers(device, &cbAllocCI, &commandBuffer);
}

VkShaderModule Renderer::createShaderModule(const std::vector<char>& code) {
  VkShaderModuleCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = code.size(),
      .pCode = reinterpret_cast<const uint32_t*>(code.data())};

  VkShaderModule shaderModule{};

  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create shader module");
  }

  return shaderModule;
}

void Renderer::createSyncObjects() {
  VkSemaphoreCreateInfo semaphoreCI{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

  VkFenceCreateInfo fenceCI{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                            .flags = VK_FENCE_CREATE_SIGNALED_BIT};

  vkCreateSemaphore(device, &semaphoreCI, nullptr, &imageAvailableSemaphore);

  vkCreateSemaphore(device, &semaphoreCI, nullptr, &renderFinishedSemaphore);

  vkCreateFence(device, &fenceCI, nullptr, &inFlightFence);
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

void Renderer::createPipeline() {
  auto vertexCode = readFile("triangle.vert.spv");
  auto fragmentCode = readFile("triangle.frag.spv");

  VkShaderModule vertexShader = createShaderModule(vertexCode);

  VkShaderModule fragmentShader = createShaderModule(fragmentCode);
  VkPipelineShaderStageCreateInfo vertexStage{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vertexShader,
      .pName = "main"};

  VkPipelineShaderStageCreateInfo fragmentStage{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fragmentShader,
      .pName = "main"};

  VkPipelineShaderStageCreateInfo stages[] = {vertexStage, fragmentStage};

  VkPipelineVertexInputStateCreateInfo vertexInput{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE};

  VkPipelineViewportStateCreateInfo viewportState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1};

  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                    VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamicState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2,
      .pDynamicStates = dynamicStates};

  VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .lineWidth = 1.0f};

  VkPipelineMultisampleStateCreateInfo multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

  VkPipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

  VkPipelineColorBlendStateCreateInfo colorBlending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment};

  VkPipelineLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

  VkPipelineRenderingCreateInfo renderingInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &swapchainImageFormat};

  vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);

  VkGraphicsPipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &renderingInfo,

      .stageCount = 2,
      .pStages = stages,

      .pVertexInputState = &vertexInput,
      .pInputAssemblyState = &inputAssembly,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pColorBlendState = &colorBlending,
      .pDynamicState = &dynamicState,

      .layout = pipelineLayout};

  vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                            &graphicsPipeline);
}

void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer,
                                   uint32_t imageIndex) {
  vkResetCommandBuffer(commandBuffer, 0);

  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  VkImageMemoryBarrier toColor{};
  toColor.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  toColor.srcAccessMask = 0;
  toColor.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  toColor.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  toColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  toColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toColor.image = swapchainImages[imageIndex];
  toColor.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  toColor.subresourceRange.levelCount = 1;
  toColor.subresourceRange.layerCount = 1;

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &toColor);

  VkClearValue clear{};
  clear.color = {{0.02f, 0.02f, 0.03f, 1.0f}};

  VkRenderingAttachmentInfo colorAttachment{};
  colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  colorAttachment.imageView = swapchainImageViews[imageIndex];
  colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.clearValue = clear;

  VkRenderingInfo renderingInfo{};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderingInfo.renderArea = {{0, 0}, swapchainExtent};
  renderingInfo.layerCount = 1;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.pColorAttachments = &colorAttachment;

  vkCmdBeginRendering(commandBuffer, &renderingInfo);

  VkViewport viewport{
      0.0f, 0.0f, float(swapchainExtent.width), float(swapchainExtent.height),
      0.0f, 1.0f};
  VkRect2D scissor{{0, 0}, swapchainExtent};

  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    graphicsPipeline);
  vkCmdDraw(commandBuffer, 3, 1, 0, 0);

  vkCmdEndRendering(commandBuffer);

  VkImageMemoryBarrier toPresent = toColor;
  toPresent.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  toPresent.dstAccessMask = 0;
  toPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  vkCmdPipelineBarrier(commandBuffer,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &toPresent);

  vkEndCommandBuffer(commandBuffer);
}
