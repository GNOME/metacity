/*
 * Copyright (C) 2017-2019 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

#include "display-private.h"
#include "meta-compositor-vulkan.h"
#include "meta-enum-types.h"
#include "meta-surface-vulkan.h"
#include "prefs.h"
#include "screen.h"
#include "util.h"

struct _MetaCompositorVulkan
{
  MetaCompositor            parent;

  gboolean                  lunarg_validation_layer;
  gboolean                  debug_report_extension;

  VkInstance                instance;

  VkDebugReportCallbackEXT  debug_callback;

  VkSurfaceKHR              surface;
  VkSurfaceFormatKHR        surface_format;
  VkExtent2D                surface_extent;

  VkPhysicalDevice          physical_device;
  uint32_t                  graphics_family_index;
  uint32_t                  present_family_index;

  VkDevice                  device;

  VkQueue                   graphics_queue;
  VkQueue                   present_queue;

  VkCommandPool             command_pool;

  VkSemaphore               semaphore;

  VkSwapchainKHR            swapchain;

  uint32_t                  n_images;
  VkImage                  *images;

  uint32_t                  n_image_views;
  VkImageView              *image_views;

  VkRenderPass              render_pass;

  uint32_t                  n_framebuffers;
  VkFramebuffer            *framebuffers;

  uint32_t                  n_command_buffers;
  VkCommandBuffer          *command_buffers;
};

G_DEFINE_TYPE (MetaCompositorVulkan, meta_compositor_vulkan, META_TYPE_COMPOSITOR)

static void
destroy_command_buffers (MetaCompositorVulkan *vulkan)
{
  if (vulkan->command_buffers == NULL)
    return;

  vkFreeCommandBuffers (vulkan->device, vulkan->command_pool,
                        vulkan->n_command_buffers, vulkan->command_buffers);

  g_clear_pointer (&vulkan->command_buffers, g_free);

  vulkan->n_command_buffers = 0;
}

static gboolean
create_command_buffers (MetaCompositorVulkan  *vulkan,
                        GError               **error)
{
  VkCommandBufferAllocateInfo allocate_info;
  VkResult result;

  destroy_command_buffers (vulkan);

  vulkan->n_command_buffers = vulkan->n_images;
  vulkan->command_buffers = g_new0 (VkCommandBuffer, vulkan->n_images);

  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.pNext = NULL;
  allocate_info.commandPool = vulkan->command_pool;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = vulkan->n_command_buffers;

  result = vkAllocateCommandBuffers (vulkan->device, &allocate_info,
                                     vulkan->command_buffers);

  if (result != VK_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to allocate command buffers");

      return FALSE;
    }

  return TRUE;
}

static void
destroy_framebuffers (MetaCompositorVulkan *vulkan)
{
  uint32_t i;

  if (vulkan->framebuffers == NULL)
    return;

  for (i = 0; i < vulkan->n_framebuffers; i++)
    vkDestroyFramebuffer (vulkan->device, vulkan->framebuffers[i], NULL);

  g_clear_pointer (&vulkan->framebuffers, g_free);

  vulkan->n_framebuffers = 0;
}

static gboolean
create_framebuffers (MetaCompositorVulkan  *vulkan,
                     GError               **error)
{
  uint32_t i;
  VkFramebufferCreateInfo info;
  VkResult result;

  destroy_framebuffers (vulkan);

  vulkan->framebuffers = g_new0 (VkFramebuffer, vulkan->n_images);

  for (i = 0; i < vulkan->n_images; i++)
    {
      VkImageView attachments[1];

      attachments[0] = vulkan->image_views[i];

      info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      info.pNext = NULL;
      info.flags = 0;
      info.renderPass = vulkan->render_pass;
      info.attachmentCount = 1;
      info.pAttachments = attachments;
      info.width = vulkan->surface_extent.width;
      info.height = vulkan->surface_extent.height;
      info.layers = 1;

      result = vkCreateFramebuffer (vulkan->device, &info, NULL,
                                    &vulkan->framebuffers[i]);

      if (result != VK_SUCCESS)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Failed to create framebuffer");

          return FALSE;
        }

      vulkan->n_framebuffers++;
    }

  return TRUE;
}

static void
destroy_render_pass (MetaCompositorVulkan *vulkan)
{
  if (vulkan->render_pass != VK_NULL_HANDLE)
    {
      vkDestroyRenderPass (vulkan->device, vulkan->render_pass, NULL);
      vulkan->render_pass = VK_NULL_HANDLE;
    }
}

static gboolean
create_render_pass (MetaCompositorVulkan  *vulkan,
                    GError               **error)
{
  VkAttachmentDescription color_attachment;
  VkAttachmentReference color_attachment_ref;
  VkSubpassDescription subpass;
  VkRenderPassCreateInfo info;
  VkResult result;

  color_attachment.flags = 0;
  color_attachment.format = vulkan->surface_format.format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  subpass.flags = 0;
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.inputAttachmentCount = 0;
  subpass.pInputAttachments = NULL;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;
  subpass.pResolveAttachments = NULL;
  subpass.pDepthStencilAttachment = NULL;
  subpass.preserveAttachmentCount = 0;
  subpass.pPreserveAttachments = NULL;

  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  info.attachmentCount = 1;
  info.pAttachments = &color_attachment;
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 0;
  info.pDependencies = NULL;

  destroy_render_pass (vulkan);

  result = vkCreateRenderPass (vulkan->device, &info, NULL,
                               &vulkan->render_pass);

  if (result != VK_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to create render pass");

      return FALSE;
    }

  return TRUE;
}

static void
destroy_image_views (MetaCompositorVulkan *vulkan)
{
  uint32_t i;

  if (vulkan->image_views == NULL)
    return;

  for (i = 0; i < vulkan->n_image_views; i++)
    vkDestroyImageView (vulkan->device, vulkan->image_views[i], NULL);

  g_clear_pointer (&vulkan->image_views, g_free);

  vulkan->n_image_views = 0;
}

static gboolean
create_image_views (MetaCompositorVulkan  *vulkan,
                    GError               **error)
{
  VkComponentMapping components;
  VkImageSubresourceRange subresource_range;
  uint32_t i;
  VkImageViewCreateInfo info;
  VkResult result;

  destroy_image_views (vulkan);

  vulkan->image_views = g_new0 (VkImageView, vulkan->n_images);

  components.r = VK_COMPONENT_SWIZZLE_R;
  components.g = VK_COMPONENT_SWIZZLE_G;
  components.b = VK_COMPONENT_SWIZZLE_B;
  components.a = VK_COMPONENT_SWIZZLE_A;

  subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subresource_range.baseMipLevel = 0;
  subresource_range.levelCount = 1;
  subresource_range.baseArrayLayer = 0;
  subresource_range.layerCount = 1;

  for (i = 0; i < vulkan->n_images; i++)
    {
      info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      info.pNext = NULL;
      info.flags = 0;
      info.image = vulkan->images[i];
      info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      info.format = vulkan->surface_format.format;
      info.components = components;
      info.subresourceRange = subresource_range;

      result = vkCreateImageView (vulkan->device, &info, NULL,
                                  &vulkan->image_views[i]);

      if (result != VK_SUCCESS)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Failed to create image view");

          return FALSE;
        }

      vulkan->n_image_views++;
    }

  return TRUE;
}

static void
destroy_swapchain (MetaCompositorVulkan *vulkan)
{
  if (vulkan->swapchain != VK_NULL_HANDLE)
    {
      vkDestroySwapchainKHR (vulkan->device, vulkan->swapchain, NULL);
      vulkan->swapchain = VK_NULL_HANDLE;
    }

  g_clear_pointer (&vulkan->images, g_free);

  vulkan->n_images = 0;
}

static gboolean
create_swapchain (MetaCompositorVulkan  *vulkan,
                  GError               **error)
{
  VkSurfaceCapabilitiesKHR capabilities;
  VkSwapchainCreateInfoKHR info;
  VkSwapchainKHR swapchain;
  VkResult result;

  result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR (vulkan->physical_device,
                                                      vulkan->surface,
                                                      &capabilities);

  if (result != VK_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Could not query surface capabilities");

      return FALSE;
    }

  vulkan->surface_extent = capabilities.currentExtent;

  if (vulkan->surface_extent.width == 0xffffffff &&
      vulkan->surface_extent.height == 0xffffffff)
    {
      MetaDisplay *display;
      gint width;
      gint height;

      display = meta_compositor_get_display (META_COMPOSITOR (vulkan));

      meta_screen_get_size (display->screen, &width, &height);

      vulkan->surface_extent.width = width;
      vulkan->surface_extent.height = height;
    }

  info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  info.pNext = NULL;
  info.flags = 0;
  info.surface = vulkan->surface;
  info.minImageCount = capabilities.minImageCount;
  info.imageFormat = vulkan->surface_format.format;
  info.imageColorSpace = vulkan->surface_format.colorSpace;
  info.imageExtent = vulkan->surface_extent;
  info.imageArrayLayers = 1;
  info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  info.preTransform = capabilities.currentTransform;
  info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  info.clipped = VK_FALSE;
  info.oldSwapchain = vulkan->swapchain;

  if (vulkan->graphics_family_index != vulkan->present_family_index)
    {
      uint32_t indices[2];

      indices[0] = vulkan->graphics_family_index;
      indices[1] = vulkan->present_family_index;

      info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      info.queueFamilyIndexCount = 2;
      info.pQueueFamilyIndices = indices;
    }
  else
    {
      info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      info.queueFamilyIndexCount = 0;
      info.pQueueFamilyIndices = NULL;
    }

  result = vkCreateSwapchainKHR (vulkan->device, &info, NULL, &swapchain);

  destroy_swapchain (vulkan);

  if (result != VK_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to create swapchain");

      return FALSE;
    }

  vulkan->swapchain = swapchain;

  result = vkGetSwapchainImagesKHR (vulkan->device, vulkan->swapchain,
                                    &vulkan->n_images, NULL);

  if (result != VK_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to get swapchain images");

      return FALSE;
    }

  vulkan->images = g_new0 (VkImage, vulkan->n_images);
  result = vkGetSwapchainImagesKHR (vulkan->device, vulkan->swapchain,
                                    &vulkan->n_images, vulkan->images);

  if (result != VK_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to get swapchain images");

      destroy_swapchain (vulkan);

      return FALSE;
    }

  return TRUE;
}

static void
enumerate_instance_layers (MetaCompositorVulkan *vulkan)
{
  uint32_t n_layers;
  VkLayerProperties *layers;
  VkResult result;
  uint32_t i;

  if (!meta_check_debug_flags (META_DEBUG_VULKAN) &&
      g_getenv ("META_VULKAN_VALIDATE") == NULL)
    return;

  result = vkEnumerateInstanceLayerProperties (&n_layers, NULL);

  if (result != VK_SUCCESS)
    {
      meta_topic (META_DEBUG_VULKAN,
                  "Failed to enumerate instance layer properties\n");

      return;
    }

  layers = g_new0 (VkLayerProperties, n_layers);
  result = vkEnumerateInstanceLayerProperties (&n_layers, layers);

  if (result != VK_SUCCESS)
    {
      meta_topic (META_DEBUG_VULKAN,
                  "Failed to enumerate instance layer properties\n");

      g_free (layers);
      return;
    }

  meta_topic (META_DEBUG_VULKAN, "Available instance layers:\n");
  meta_push_no_msg_prefix ();

  for (i = 0; i < n_layers; i++)
    {
      const gchar *layer_name;

      layer_name = layers[i].layerName;

      meta_topic (META_DEBUG_VULKAN, "  %s v%u.%u.%u (%s)\n", layer_name,
                  VK_VERSION_MAJOR (layers[i].specVersion),
                  VK_VERSION_MINOR (layers[i].specVersion),
                  VK_VERSION_PATCH (layers[i].specVersion),
                  layers[i].description);

      if (g_strcmp0 (layer_name, "VK_LAYER_LUNARG_standard_validation") == 0)
        {
          vulkan->lunarg_validation_layer = TRUE;
        }
    }

  meta_pop_no_msg_prefix ();

  g_free (layers);
}

static void
enumerate_instance_extensions (MetaCompositorVulkan *vulkan)
{
  uint32_t n_extensions;
  VkExtensionProperties *extensions;
  VkResult result;
  uint32_t i;

  if (!meta_check_debug_flags (META_DEBUG_VULKAN) &&
      g_getenv ("META_VULKAN_VALIDATE") == NULL)
    return;

  result = vkEnumerateInstanceExtensionProperties (NULL, &n_extensions, NULL);

  if (result != VK_SUCCESS)
    {
      meta_topic (META_DEBUG_VULKAN,
                  "Failed to enumerate instance extension properties\n");

      return;
    }

  extensions = g_new0 (VkExtensionProperties, n_extensions);
  result = vkEnumerateInstanceExtensionProperties (NULL, &n_extensions,
                                                   extensions);

  if (result != VK_SUCCESS)
    {
      meta_topic (META_DEBUG_VULKAN,
                  "Failed to enumerate instance extension properties\n");

      g_free (extensions);
      return;
    }

  meta_topic (META_DEBUG_VULKAN, "Available instance extensions:\n");
  meta_push_no_msg_prefix ();

  for (i = 0; i < n_extensions; i++)
    {
      const gchar *extension_name;

      extension_name = extensions[i].extensionName;

      meta_topic (META_DEBUG_VULKAN, "  %s v%u.%u.%u\n", extension_name,
                  VK_VERSION_MAJOR (extensions[i].specVersion),
                  VK_VERSION_MINOR (extensions[i].specVersion),
                  VK_VERSION_PATCH (extensions[i].specVersion));

      if (g_strcmp0 (extension_name, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0)
        {
          vulkan->debug_report_extension = TRUE;
        }
    }

  meta_pop_no_msg_prefix ();

  g_free (extensions);
}

static gboolean
create_instance (MetaCompositorVulkan  *vulkan,
                 GError               **error)
{
  GPtrArray *layers;
  GPtrArray *extensions;
  VkApplicationInfo app_info;
  VkInstanceCreateInfo instance_info;
  VkResult result;

  layers = g_ptr_array_new ();
  extensions = g_ptr_array_new ();

  if (vulkan->lunarg_validation_layer)
    g_ptr_array_add (layers, (gpointer) "VK_LAYER_LUNARG_standard_validation");

  g_ptr_array_add (extensions, (gpointer) VK_KHR_SURFACE_EXTENSION_NAME);
  g_ptr_array_add (extensions, (gpointer) VK_KHR_XLIB_SURFACE_EXTENSION_NAME);

  if (vulkan->debug_report_extension)
    g_ptr_array_add (extensions, (gpointer) VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pNext = NULL;
  app_info.pApplicationName = "Metacity";
  app_info.applicationVersion = VK_MAKE_VERSION (METACITY_MAJOR_VERSION,
                                                 METACITY_MINOR_VERSION,
                                                 METACITY_MICRO_VERSION);
  app_info.pEngineName = NULL;
  app_info.engineVersion = 0;
  app_info.apiVersion = VK_API_VERSION_1_0;

  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pNext = NULL;
  instance_info.flags = 0;
  instance_info.pApplicationInfo = &app_info;
  instance_info.enabledLayerCount = layers->len;
  instance_info.ppEnabledLayerNames = (const char * const *) layers->pdata;
  instance_info.enabledExtensionCount = extensions->len;
  instance_info.ppEnabledExtensionNames = (const char * const *) extensions->pdata;

  result = vkCreateInstance (&instance_info, NULL, &vulkan->instance);

  g_ptr_array_free (layers, TRUE);
  g_ptr_array_free (extensions, TRUE);

  if (result != VK_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to create Vulkan instance");

      return FALSE;
    }

  return TRUE;
}

static VkBool32
debug_report_cb (VkDebugReportFlagsEXT       flags,
                 VkDebugReportObjectTypeEXT  objectType,
                 uint64_t                    object,
                 size_t                      location,
                 int32_t                     messageCode,
                 const char                 *pLayerPrefix,
                 const char                 *pMessage,
                 void                       *pUserData)
{
  if (!meta_check_debug_flags (META_DEBUG_VULKAN))
    {
      if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
        g_critical ("%s: %s", pLayerPrefix, pMessage);
      else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
        g_critical ("%s: %s", pLayerPrefix, pMessage);
      else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
        g_warning ("%s: %s", pLayerPrefix, pMessage);
      else
        meta_topic (META_DEBUG_VULKAN, "%s: %s\n", pLayerPrefix, pMessage);
    }
  else
    {
      meta_topic (META_DEBUG_VULKAN, "%s: %s\n", pLayerPrefix, pMessage);
    }

  return VK_FALSE;
}

static void
setup_debug_callback (MetaCompositorVulkan *vulkan)
{
  PFN_vkVoidFunction f;
  VkDebugReportFlagsEXT flags;
  VkDebugReportCallbackCreateInfoEXT info;
  VkResult result;

  if (!vulkan->lunarg_validation_layer || !vulkan->debug_report_extension)
    return;

  f = vkGetInstanceProcAddr (vulkan->instance, "vkCreateDebugReportCallbackEXT");

  if (f == VK_NULL_HANDLE)
    {
      if (!meta_check_debug_flags (META_DEBUG_VULKAN))
        g_warning ("VK_EXT_debug_report not found");
      else
        meta_topic (META_DEBUG_VULKAN, "VK_EXT_debug_report not found");

      return;
    }

  flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
          VK_DEBUG_REPORT_WARNING_BIT_EXT |
          VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
          VK_DEBUG_REPORT_ERROR_BIT_EXT |
          VK_DEBUG_REPORT_DEBUG_BIT_EXT;

  info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
  info.pNext = NULL;
  info.flags = flags;
  info.pfnCallback = debug_report_cb;
  info.pUserData = NULL;

  result = ((PFN_vkCreateDebugReportCallbackEXT) f) (vulkan->instance,
                                                     &info, NULL,
                                                     &vulkan->debug_callback);

  if (result != VK_SUCCESS)
    {
      if (!meta_check_debug_flags (META_DEBUG_VULKAN))
        g_warning ("Failed to set up debug callback");
      else
        meta_topic (META_DEBUG_VULKAN, "Failed to set up debug callback");
    }
}

static gboolean
find_surface_format (MetaCompositorVulkan  *vulkan,
                     GError               **error)
{
  uint32_t n_formats;
  VkSurfaceFormatKHR *formats;
  VkResult result;
  uint32_t i;

  result = vkGetPhysicalDeviceSurfaceFormatsKHR (vulkan->physical_device,
                                                 vulkan->surface,
                                                 &n_formats, NULL);

  if (result != VK_SUCCESS || n_formats == 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to get surface formats");

      return FALSE;
    }

  formats = g_new0 (VkSurfaceFormatKHR, n_formats);
  result = vkGetPhysicalDeviceSurfaceFormatsKHR (vulkan->physical_device,
                                                 vulkan->surface,
                                                 &n_formats, formats);

  for (i = 0; i < n_formats; i++)
    {
      if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
        {
          vulkan->surface_format = formats[i];
          break;
        }
    }

  g_free (formats);

  if (i == n_formats)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to find valid surface format");

      return FALSE;
    }

  return TRUE;
}

static gboolean
create_overlay_surface (MetaCompositorVulkan  *vulkan,
                        GError               **error)
{
  MetaCompositor *compositor;
  MetaDisplay *display;
  Window overlay;
  VkXlibSurfaceCreateInfoKHR info;
  VkResult result;

  compositor = META_COMPOSITOR (vulkan);

  display = meta_compositor_get_display (compositor);
  overlay = meta_compositor_get_overlay_window (compositor);

  info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
  info.pNext = NULL;
  info.flags = 0;
  info.dpy = display->xdisplay;
  info.window = overlay;

  result = vkCreateXlibSurfaceKHR (vulkan->instance, &info, NULL,
                                   &vulkan->surface);

  if (result != VK_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to create Vulkan surface for overlay window");

      return FALSE;
    }

  return TRUE;
}

static const gchar *
device_type_to_string (VkPhysicalDeviceType type)
{
  switch (type)
    {
      case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        return "other";
        break;

      case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "integrated";
        break;

      case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "discrete";
        break;

      case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "virtual";
        break;

      case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return "cpu";
        break;

#if VK_HEADER_VERSION < 140
      case VK_PHYSICAL_DEVICE_TYPE_RANGE_SIZE:
#endif
      case VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM:
      default:
        break;
    }

  return "unknown";
}

static gchar *
queue_flags_to_string (VkQueueFlags flags)
{
  GPtrArray *operations;
  gchar *result;

  operations = g_ptr_array_new ();

  if (flags & VK_QUEUE_GRAPHICS_BIT)
    g_ptr_array_add (operations, (gpointer) "graphics");

  if (flags & VK_QUEUE_COMPUTE_BIT)
    g_ptr_array_add (operations, (gpointer) "compute");

  if (flags & VK_QUEUE_TRANSFER_BIT)
    g_ptr_array_add (operations, (gpointer) "transfer");

  if (flags & VK_QUEUE_SPARSE_BINDING_BIT)
    g_ptr_array_add (operations, (gpointer) "sparse binding");

  g_ptr_array_add (operations, NULL);

  result = g_strjoinv (", ", (char **) operations->pdata);
  g_ptr_array_free (operations, TRUE);

  return result;
}

static gboolean
enumerate_physical_devices (MetaCompositorVulkan  *vulkan,
                            GError               **error)
{
  uint32_t n_devices;
  VkPhysicalDevice *devices;
  VkResult result;
  uint32_t i;

  result = vkEnumeratePhysicalDevices (vulkan->instance, &n_devices, NULL);

  if (result != VK_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to query the number of physical devices presents");

      return FALSE;
    }

  if (n_devices == 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to find GPUs with Vulkan support");

      return FALSE;
    }

  devices = g_new0 (VkPhysicalDevice, n_devices);
  result = vkEnumeratePhysicalDevices (vulkan->instance, &n_devices, devices);

  if (result != VK_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to enumerate physical devices present");

      g_free (devices);
      return FALSE;
    }

  meta_topic (META_DEBUG_VULKAN, "Available physical devices:\n");
  meta_push_no_msg_prefix ();

  for (i = 0; i < n_devices; i++)
    {
      uint32_t n_family_properties;
      VkQueueFamilyProperties *family_properties;
      uint32_t graphics_family_index;
      uint32_t present_family_index;
      uint32_t j;

      if (meta_check_debug_flags (META_DEBUG_VULKAN))
        {
          VkPhysicalDeviceProperties device_properties;

          vkGetPhysicalDeviceProperties (devices[i], &device_properties);

          meta_topic (META_DEBUG_VULKAN, "  %s (type - %s, driver - v%d.%d.%d, "
                      "api - v%d.%d.%d)\n", device_properties.deviceName,
                      device_type_to_string (device_properties.deviceType),
                      VK_VERSION_MAJOR (device_properties.driverVersion),
                      VK_VERSION_MINOR (device_properties.driverVersion),
                      VK_VERSION_PATCH (device_properties.driverVersion),
                      VK_VERSION_MAJOR (device_properties.apiVersion),
                      VK_VERSION_MINOR (device_properties.apiVersion),
                      VK_VERSION_PATCH (device_properties.apiVersion));
        }

      vkGetPhysicalDeviceQueueFamilyProperties (devices[i],
                                                &n_family_properties,
                                                NULL);

      family_properties = g_new0 (VkQueueFamilyProperties, n_family_properties);
      vkGetPhysicalDeviceQueueFamilyProperties (devices[i],
                                                &n_family_properties,
                                                family_properties);

      graphics_family_index = n_family_properties;
      present_family_index = n_family_properties;

      for (j = 0; j < n_family_properties; j++)
        {
          if (meta_check_debug_flags (META_DEBUG_VULKAN))
            {
              gchar *operations;

              operations = queue_flags_to_string (family_properties[j].queueFlags);

              meta_topic (META_DEBUG_VULKAN, "    queues: %d; operations: %s\n",
                          family_properties[j].queueCount, operations);

              g_free (operations);
            }

          if (family_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
              graphics_family_index == n_family_properties)
            {
              graphics_family_index = j;
            }

          if (present_family_index == n_family_properties)
            {
              VkBool32 supported;

              result = vkGetPhysicalDeviceSurfaceSupportKHR (devices[i], j,
                                                             vulkan->surface,
                                                             &supported);

              if (result == VK_SUCCESS && supported)
                present_family_index = j;
            }
        }

      if (graphics_family_index != n_family_properties &&
          present_family_index != n_family_properties &&
          vulkan->physical_device == VK_NULL_HANDLE)
        {
          vulkan->physical_device = devices[i];
          vulkan->graphics_family_index = graphics_family_index;
          vulkan->present_family_index = present_family_index;
        }

      g_free (family_properties);
    }

  meta_pop_no_msg_prefix ();

  g_free (devices);

  if (vulkan->physical_device == VK_NULL_HANDLE)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to find a suitable GPU");

      return FALSE;
    }

  return TRUE;
}

static gboolean
create_logical_device (MetaCompositorVulkan  *vulkan,
                       GError               **error)
{
  GPtrArray *layers;
  GPtrArray *extensions;
  VkDeviceCreateInfo create_info;
  VkResult result;

  layers = g_ptr_array_new ();
  extensions = g_ptr_array_new ();

  if (vulkan->lunarg_validation_layer)
    g_ptr_array_add (layers, (gpointer) "VK_LAYER_LUNARG_standard_validation");

  g_ptr_array_add (extensions, (gpointer) VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.pNext = NULL;
  create_info.flags = 0;
  create_info.enabledLayerCount = layers->len;
  create_info.ppEnabledLayerNames = (const char * const *) layers->pdata;
  create_info.enabledExtensionCount = extensions->len;
  create_info.ppEnabledExtensionNames = (const char * const *) extensions->pdata;
  create_info.pEnabledFeatures = NULL;

  if (vulkan->graphics_family_index != vulkan->present_family_index)
    {
      VkDeviceQueueCreateInfo queue_create_info[2];

      queue_create_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queue_create_info[0].pNext = NULL;
      queue_create_info[0].flags = 0;
      queue_create_info[0].queueFamilyIndex = vulkan->graphics_family_index;
      queue_create_info[0].queueCount = 1;
      queue_create_info[0].pQueuePriorities = (float []) { 1.0f };

      queue_create_info[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queue_create_info[1].pNext = NULL;
      queue_create_info[1].flags = 0;
      queue_create_info[1].queueFamilyIndex = vulkan->present_family_index;
      queue_create_info[1].queueCount = 1;
      queue_create_info[1].pQueuePriorities = (float []) { 1.0f };

      create_info.queueCreateInfoCount = G_N_ELEMENTS (queue_create_info);
      create_info.pQueueCreateInfos = queue_create_info;
    }
  else
    {
      VkDeviceQueueCreateInfo queue_create_info;

      queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queue_create_info.pNext = NULL;
      queue_create_info.flags = 0;
      queue_create_info.queueFamilyIndex = vulkan->graphics_family_index;
      queue_create_info.queueCount = 1;
      queue_create_info.pQueuePriorities = (float []) { 1.0f };

      create_info.queueCreateInfoCount = 1;
      create_info.pQueueCreateInfos = &queue_create_info;
    }

  result = vkCreateDevice (vulkan->physical_device, &create_info,
                           NULL, &vulkan->device);

  g_ptr_array_free (layers, TRUE);
  g_ptr_array_free (extensions, TRUE);

  if (result != VK_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to create logical device");

      return FALSE;
    }

  vkGetDeviceQueue (vulkan->device, vulkan->graphics_family_index,
                    0, &vulkan->graphics_queue);

  vkGetDeviceQueue (vulkan->device, vulkan->present_family_index,
                    0, &vulkan->present_queue);

  return TRUE;
}

static gboolean
create_command_pool (MetaCompositorVulkan  *vulkan,
                     GError               **error)
{
  VkCommandPoolCreateInfo info;
  VkResult result;

  info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  info.queueFamilyIndex = vulkan->graphics_family_index;

  result = vkCreateCommandPool (vulkan->device, &info, NULL,
                                &vulkan->command_pool);

  if (result != VK_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to create command pool");

      return FALSE;
    }

  return TRUE;
}

static gboolean
create_semaphore (MetaCompositorVulkan  *vulkan,
                  GError               **error)
{
  VkSemaphoreCreateInfo info;
  VkResult result;

  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;

  result = vkCreateSemaphore (vulkan->device, &info, NULL, &vulkan->semaphore);

  if (result != VK_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to create semaphore");

      return FALSE;
    }

  return TRUE;
}

static void
meta_compositor_vulkan_finalize (GObject *object)
{
  MetaCompositorVulkan *vulkan;

  vulkan = META_COMPOSITOR_VULKAN (object);

  destroy_command_buffers (vulkan);
  destroy_framebuffers (vulkan);
  destroy_render_pass (vulkan);
  destroy_image_views (vulkan);
  destroy_swapchain (vulkan);

  if (vulkan->semaphore != VK_NULL_HANDLE)
    {
      vkDestroySemaphore (vulkan->device, vulkan->semaphore, NULL);
      vulkan->semaphore = VK_NULL_HANDLE;
    }

  if (vulkan->command_pool != VK_NULL_HANDLE)
    {
      vkDestroyCommandPool (vulkan->device, vulkan->command_pool, NULL);
      vulkan->command_pool = VK_NULL_HANDLE;
    }

  if (vulkan->device != VK_NULL_HANDLE)
    {
      vkDestroyDevice (vulkan->device, NULL);
      vulkan->device = VK_NULL_HANDLE;
    }

  if (vulkan->surface != VK_NULL_HANDLE)
    {
      vkDestroySurfaceKHR (vulkan->instance, vulkan->surface, NULL);
      vulkan->surface = VK_NULL_HANDLE;
    }

  if (vulkan->debug_callback != VK_NULL_HANDLE)
    {
      PFN_vkVoidFunction f;

      f = vkGetInstanceProcAddr (vulkan->instance,
                                 "vkDestroyDebugReportCallbackEXT");

      ((PFN_vkDestroyDebugReportCallbackEXT) f) (vulkan->instance,
                                                 vulkan->debug_callback,
                                                 NULL);

      vulkan->debug_callback = VK_NULL_HANDLE;
    }

  if (vulkan->instance != VK_NULL_HANDLE)
    {
      vkDestroyInstance (vulkan->instance, NULL);
      vulkan->instance = VK_NULL_HANDLE;
    }

  G_OBJECT_CLASS (meta_compositor_vulkan_parent_class)->finalize (object);
}

static gboolean
not_implemented_cb (MetaCompositorVulkan *vulkan)
{
  MetaDisplay *display;
  MetaCompositorType type;
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  display = meta_compositor_get_display (META_COMPOSITOR (vulkan));
  type = meta_prefs_get_compositor ();

  enum_class = g_type_class_ref (META_TYPE_COMPOSITOR_TYPE);
  enum_value = g_enum_get_value (enum_class, type);
  g_assert_nonnull (enum_value);

  g_warning ("“vulkan” compositor is not implemented, switching to “%s”...",
             enum_value->value_nick);

  g_type_class_unref (enum_class);

  g_unsetenv ("META_COMPOSITOR");
  meta_display_update_compositor (display);

  return G_SOURCE_REMOVE;
}

static gboolean
meta_compositor_vulkan_manage (MetaCompositor  *compositor,
                               GError         **error)
{
  MetaCompositorVulkan *vulkan;

  vulkan = META_COMPOSITOR_VULKAN (compositor);

  if (!meta_compositor_check_common_extensions (compositor, error))
    return FALSE;

  enumerate_instance_layers (vulkan);
  enumerate_instance_extensions (vulkan);

  if (!create_instance (vulkan, error))
    return FALSE;

  setup_debug_callback (vulkan);

  if (!meta_compositor_set_selection (compositor, error))
    return FALSE;

  if (!meta_compositor_redirect_windows (compositor, error))
    return FALSE;

  if (!create_overlay_surface (vulkan, error))
    return FALSE;

  if (!enumerate_physical_devices (vulkan, error))
    return FALSE;

  if (!find_surface_format (vulkan, error))
    return FALSE;

  if (!create_logical_device (vulkan, error))
    return FALSE;

  if (!create_command_pool (vulkan, error))
    return FALSE;

  if (!create_semaphore (vulkan, error))
    return FALSE;

  if (!create_swapchain (vulkan, error))
    return FALSE;

  if (!create_image_views (vulkan, error))
    return FALSE;

  if (!create_render_pass (vulkan, error))
    return FALSE;

  if (!create_framebuffers (vulkan, error))
    return FALSE;

  if (!create_command_buffers (vulkan, error))
    return FALSE;

  g_timeout_add (10000, (GSourceFunc) not_implemented_cb, vulkan);

  return TRUE;
}

static MetaSurface *
meta_compositor_vulkan_add_window (MetaCompositor *compositor,
                                   MetaWindow     *window)
{
  MetaSurface *surface;

  surface = g_object_new (META_TYPE_SURFACE_VULKAN,
                          "compositor", compositor,
                          "window", window,
                          NULL);

  return surface;
}

static void
meta_compositor_vulkan_process_event (MetaCompositor *compositor,
                                      XEvent         *event,
                                      MetaWindow     *window)
{
}

static void
meta_compositor_vulkan_sync_screen_size (MetaCompositor *compositor)
{
}

static void
meta_compositor_vulkan_pre_paint (MetaCompositor *compositor)
{
  META_COMPOSITOR_CLASS (meta_compositor_vulkan_parent_class)->pre_paint (compositor);
}

static void
meta_compositor_vulkan_redraw (MetaCompositor *compositor,
                               XserverRegion   all_damage)
{
}

static void
meta_compositor_vulkan_class_init (MetaCompositorVulkanClass *vulkan_class)
{
  GObjectClass *object_class;
  MetaCompositorClass *compositor_class;

  object_class = G_OBJECT_CLASS (vulkan_class);
  compositor_class = META_COMPOSITOR_CLASS (vulkan_class);

  object_class->finalize = meta_compositor_vulkan_finalize;

  compositor_class->manage = meta_compositor_vulkan_manage;
  compositor_class->add_window = meta_compositor_vulkan_add_window;
  compositor_class->process_event = meta_compositor_vulkan_process_event;
  compositor_class->sync_screen_size = meta_compositor_vulkan_sync_screen_size;
  compositor_class->pre_paint = meta_compositor_vulkan_pre_paint;
  compositor_class->redraw = meta_compositor_vulkan_redraw;
}

static void
meta_compositor_vulkan_init (MetaCompositorVulkan *vulkan)
{
  meta_compositor_set_composited (META_COMPOSITOR (vulkan), TRUE);
}

MetaCompositor *
meta_compositor_vulkan_new (MetaDisplay  *display,
                            GError      **error)
{
  return g_initable_new (META_TYPE_COMPOSITOR_VULKAN, NULL, error,
                         "display", display,
                         NULL);
}
