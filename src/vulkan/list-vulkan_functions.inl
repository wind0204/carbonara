#ifndef EXPORTED_VULKAN_FUNCTION
#define EXPORTED_VULKAN_FUNCTION( function )
#endif

EXPORTED_VULKAN_FUNCTION( vkGetInstanceProcAddr )

#undef EXPORTED_VULKAN_FUNCTION


//
// Global-level functions
//

#ifndef GLOBAL_LEVEL_VULKAN_FUNCTION
#define GLOBAL_LEVEL_VULKAN_FUNCTION( function )
#endif

GLOBAL_LEVEL_VULKAN_FUNCTION( vkEnumerateInstanceExtensionProperties )
//GLOBAL_LEVEL_VULKAN_FUNCTION( vkEnumerateInstanceLayerProperties )
GLOBAL_LEVEL_VULKAN_FUNCTION( vkCreateInstance )
GLOBAL_LEVEL_VULKAN_FUNCTION( vkEnumerateInstanceVersion )

#undef GLOBAL_LEVEL_VULKAN_FUNCTION


//
// Instance-level functions
//

#ifndef INSTANCE_LEVEL_VULKAN_FUNCTION
#define INSTANCE_LEVEL_VULKAN_FUNCTION( function )
#endif

INSTANCE_LEVEL_VULKAN_FUNCTION( vkEnumeratePhysicalDevices )
INSTANCE_LEVEL_VULKAN_FUNCTION( vkGetPhysicalDeviceFeatures2 )
INSTANCE_LEVEL_VULKAN_FUNCTION( vkGetPhysicalDeviceProperties2 )
INSTANCE_LEVEL_VULKAN_FUNCTION( vkCreateDevice )
INSTANCE_LEVEL_VULKAN_FUNCTION( vkGetDeviceProcAddr )
INSTANCE_LEVEL_VULKAN_FUNCTION( vkEnumerateDeviceExtensionProperties )
INSTANCE_LEVEL_VULKAN_FUNCTION( vkGetPhysicalDeviceQueueFamilyProperties2 )
INSTANCE_LEVEL_VULKAN_FUNCTION( vkGetPhysicalDeviceMemoryProperties2 )
INSTANCE_LEVEL_VULKAN_FUNCTION( vkGetPhysicalDeviceFormatProperties2 )
INSTANCE_LEVEL_VULKAN_FUNCTION( vkDestroyInstance )  //\
    // Host access to `instance` must be externally synchronized
    // Host access to all VkPhysicalDevice objects enumerated from `instance` must be externally
    //   synchronized



#undef INSTANCE_LEVEL_VULKAN_FUNCTION


//
// Instance-level functions from extensions
//

#ifndef INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION
#define INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( function, extension )
#endif

INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkGetPhysicalDeviceSurfaceSupportKHR, VK_KHR_SURFACE_EXTENSION_NAME )
INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkGetPhysicalDeviceSurfaceCapabilitiesKHR, VK_KHR_SURFACE_EXTENSION_NAME )
INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkGetPhysicalDeviceSurfaceFormatsKHR, VK_KHR_SURFACE_EXTENSION_NAME )
INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkGetPhysicalDeviceSurfacePresentModesKHR, VK_KHR_SURFACE_EXTENSION_NAME )

/* Let SDL3 handle this part
INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkDestroySurfaceKHR, VK_KHR_SURFACE_EXTENSION_NAME )

#ifdef CARBONARA_TARGET_PLATFORM_WIN32
INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkCreateWin32SurfaceKHR, VK_KHR_WIN32_SURFACE_EXTENSION_NAME )
//#elif defined CARBONARA_TARGET_PLATFORM_XCB
#elif defined CARBONARA_TARGET_PLATFORM_WAYLAND_OR_XCB_OR_XLIB
INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkCreateWaylandSurfaceKHR, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME )
INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkCreateXcbSurfaceKHR, VK_KHR_XCB_SURFACE_EXTENSION_NAME )
INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkCreateXlibSurfaceKHR, VK_KHR_XLIB_SURFACE_EXTENSION_NAME )
#elif defined CARBONARA_TARGET_PLATFORM_ANDROID
INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkCreateXlibSurfaceKHR, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME )
#endif
*/

#undef INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION


//
// (Logical) device-level functions
//

#ifndef DEVICE_LEVEL_VULKAN_FUNCTION
#define DEVICE_LEVEL_VULKAN_FUNCTION( function )
#endif

DEVICE_LEVEL_VULKAN_FUNCTION( vkGetDeviceQueue )
DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyDevice )  //\
    // Host access to `device` must be externally synchronized
    // Host access to all VkQueue objects created from `device` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateCommandPool )
DEVICE_LEVEL_VULKAN_FUNCTION( vkAllocateCommandBuffers )
DEVICE_LEVEL_VULKAN_FUNCTION( vkDeviceWaitIdle )  //\
    // Host access to all VkQueue objects created from `device` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyCommandPool )  //\
    // Host access to `commandPool` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateBuffer )
DEVICE_LEVEL_VULKAN_FUNCTION( vkGetBufferMemoryRequirements )
DEVICE_LEVEL_VULKAN_FUNCTION( vkAllocateMemory )
DEVICE_LEVEL_VULKAN_FUNCTION( vkBindBufferMemory )  //\
    // Host access to buffer must be externally synchronized
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdPipelineBarrier )
DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateImage )
DEVICE_LEVEL_VULKAN_FUNCTION( vkGetImageMemoryRequirements )
DEVICE_LEVEL_VULKAN_FUNCTION( vkBindImageMemory )  //\
    // Host access to image must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateImageView )
DEVICE_LEVEL_VULKAN_FUNCTION( vkMapMemory )  //\
    // Host access to `memory` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkFlushMappedMemoryRanges )
DEVICE_LEVEL_VULKAN_FUNCTION( vkUnmapMemory )  //\
    // Host access to `memory` must be externally synchronized
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdCopyBuffer )  //\
    // Host access to `commandBuffer` must be externally synchronized
    // Host access to the VkCommandPool that `commandBuffer` was allocated from must be externally
    //   synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdCopyBufferToImage )  //\
    // Host access to `commandBuffer` must be externally synchronized
    // Host access to the VkCommandPool that `commandBuffer` was allocated from must be externally
    //  synchronized
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdCopyImageToBuffer )
DEVICE_LEVEL_VULKAN_FUNCTION( vkBeginCommandBuffer )  //\
    // Host access to `commandBuffer` must be externally synchronized
    // Host access to the VkCommandPool that `commandBuffer` was allocated from must be externally
    //   synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkEndCommandBuffer )  //\
    // Host access to `commandBuffer` must be externally synchronized
    // Host access to the VkCommandPool that `commandBuffer` was allocated from must be externally
    //   synchronized
//DEVICE_LEVEL_VULKAN_FUNCTION( vkQueueSubmit )  //\
    // Host access to `queue` must be externally synchronized
    // Host access to `fence` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyImageView )  //\
    // Host access to `imageView` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyImage )  //\
    // Host access to `image` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyBuffer )  //\
    // Host access to `buffer` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkFreeMemory )  //\
    // Host access to `memory` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateSemaphore )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateFence )
DEVICE_LEVEL_VULKAN_FUNCTION( vkWaitSemaphores )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkWaitForFences )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkResetFences )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyFence )
DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroySemaphore )  //\
    // Host access to `semaphore` must be externally synchronized
//DEVICE_LEVEL_VULKAN_FUNCTION( vkResetCommandBuffer )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkFreeCommandBuffers )
DEVICE_LEVEL_VULKAN_FUNCTION( vkResetCommandPool )  //\
    // Host access to `commandPool` must be externally synchronized
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateBufferView )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyBufferView )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkQueueWaitIdle )
DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateSampler )
DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateDescriptorSetLayout )
DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateDescriptorPool )
DEVICE_LEVEL_VULKAN_FUNCTION( vkAllocateDescriptorSets )
DEVICE_LEVEL_VULKAN_FUNCTION( vkUpdateDescriptorSets )  //\
    // Host access to `pDescriptorWrites[i].dstSet` and `pDescriptorCopies[i].dstSet` must be
    //  externally synchronized unless explicitly denoted otherwise for specific flags.
DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdBindDescriptorSets )  //\
    // Host access to `commandBuffer` must be externally synchronized
    // Host access to the VkCommandPool that `commandBuffer` was allocated from must be externally
    //  synchronized
//DEVICE_LEVEL_VULKAN_FUNCTION( vkFreeDescriptorSets )
DEVICE_LEVEL_VULKAN_FUNCTION( vkResetDescriptorPool )
DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyDescriptorPool )  //\
    // Host access to `descriptorPool` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyDescriptorSetLayout )  //\
    // Host access to `descriptorSetLayout` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroySampler )  //\
    // Host access to `sampler` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateRenderPass2 )
DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateFramebuffer )
DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyFramebuffer )  //\
    // Host access to `framebuffer` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyRenderPass )  //\
    // Host access to `renderPass` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdBeginRenderPass )  //\
    // Host access to `commandBuffer` must be externally synchronized
    // Host access to the VkCommandPool that `commandBuffer` was allocated from must be externally
    //  synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdNextSubpass )  //\
    // Host access to `commandBuffer` must be externally synchronized
    // Host access to the VkCommandPool that `commandBuffer` was allocated from must be externally
    //  synchronized
DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdEndRenderPass )  //\
    // Host access to `commandBuffer` must be externally synchronized
    // Host access to the VkCommandPool that `commandBuffer` was allocated from must be externally
    //  synchronized
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCreatePipelineCache )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkGetPipelineCacheData )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkMergePipelineCaches )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyPipelineCache )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateGraphicsPipelines )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateComputePipelines )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyPipeline )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyEvent )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyQueryPool )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateShaderModule )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyShaderModule )
DEVICE_LEVEL_VULKAN_FUNCTION( vkCreatePipelineLayout )
DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyPipelineLayout )  //\
    // Host access to `pipelineLayout` must be externally synchronized
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdBindPipeline )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdSetViewport )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdSetScissor )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdBindVertexBuffers )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdDraw )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdDrawIndexed )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdDispatch )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdCopyImage )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdPushConstants )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdClearColorImage )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdClearDepthStencilImage )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdBindIndexBuffer )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdSetLineWidth )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdSetDepthBias )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdSetBlendConstants )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdExecuteCommands )
//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdClearAttachments )

#undef DEVICE_LEVEL_VULKAN_FUNCTION


//
// (Logical) device-level functions from extensions
//

#ifndef DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION
#define DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( function, extension )
#endif

DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkCreateSwapchainKHR, VK_KHR_SWAPCHAIN_EXTENSION_NAME )
DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkGetSwapchainImagesKHR, VK_KHR_SWAPCHAIN_EXTENSION_NAME )
DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkAcquireNextImageKHR, VK_KHR_SWAPCHAIN_EXTENSION_NAME )  //\
    // Host access to `swapchain` must be externally synchronized
    // Host access to `semaphore` must be externally synchronized
    // Host access to `fence` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkQueuePresentKHR, VK_KHR_SWAPCHAIN_EXTENSION_NAME )  //\
    // Host access to `queue` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkDestroySwapchainKHR, VK_KHR_SWAPCHAIN_EXTENSION_NAME )  //\
    // Host access to swapchain must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkQueueSubmit2KHR, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME )  //\
    // Host access to `queue` must be externally synchronized
    // Host access to `fence` must be externally synchronized
DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( vkCmdPipelineBarrier2KHR, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME )  //\
    // Host access to `commandBuffer` must be externally synchronized
    // Host access to the VkCommandPool that `commandBuffer` was allocated from must be externally
    //  synchronized



//DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdPipelineBarrier )
#undef DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION
