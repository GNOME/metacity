/*
 * Copyright (C) 2017 Alberts Muktupāvels
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

#ifdef HAVE_VULKAN
#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>
#endif

#include "display-private.h"
#include "meta-compositor-vulkan.h"
#include "prefs.h"
#include "util.h"

struct _MetaCompositorVulkan
{
  MetaCompositor           parent;

  gboolean                 lunarg_validation_layer;
  gboolean                 debug_report_extension;

#ifdef HAVE_VULKAN
  VkInstance               instance;

  VkDebugReportCallbackEXT debug_callback;

  VkSurfaceKHR             surface;

  VkPhysicalDevice         physical_device;
  uint32_t                 graphics_family_index;
  uint32_t                 present_family_index;

  VkDevice                 device;

  VkQueue                  graphics_queue;
  VkQueue                  present_queue;
#endif
};

G_DEFINE_TYPE (MetaCompositorVulkan, meta_compositor_vulkan, META_TYPE_COMPOSITOR)

#ifdef HAVE_VULKAN
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

      case VK_PHYSICAL_DEVICE_TYPE_RANGE_SIZE:
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
#endif

static void
meta_compositor_vulkan_finalize (GObject *object)
{
#ifdef HAVE_VULKAN
  MetaCompositorVulkan *vulkan;

  vulkan = META_COMPOSITOR_VULKAN (object);

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
#endif

  G_OBJECT_CLASS (meta_compositor_vulkan_parent_class)->finalize (object);
}

static gboolean
not_implemented_cb (MetaCompositorVulkan *vulkan)
{
  gboolean cm;

  cm = meta_prefs_get_compositing_manager ();

  g_warning ("MetaCompositorVulkan is not implemented, switching to %s...",
             cm ? "MetaCompositorXRender" : "MetaCompositorNone");

  g_unsetenv ("META_COMPOSITOR");
  meta_prefs_set_compositing_manager (!cm);
  meta_prefs_set_compositing_manager (cm);

  return G_SOURCE_REMOVE;
}

static gboolean
meta_compositor_vulkan_manage (MetaCompositor  *compositor,
                               GError         **error)
{
#ifdef HAVE_VULKAN
  MetaCompositorVulkan *vulkan;

  vulkan = META_COMPOSITOR_VULKAN (compositor);

  if (!meta_compositor_check_extensions (compositor, error))
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

  if (!create_logical_device (vulkan, error))
    return FALSE;

  g_timeout_add (10000, (GSourceFunc) not_implemented_cb, vulkan);

  return TRUE;
#else
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
               "Compiled without Vulkan support");

  return FALSE;
#endif
}

static void
meta_compositor_vulkan_add_window (MetaCompositor *compositor,
                                   MetaWindow     *window)
{
}

static void
meta_compositor_vulkan_remove_window (MetaCompositor *compositor,
                                      MetaWindow     *window)
{
}

static void
meta_compositor_vulkan_show_window (MetaCompositor *compositor,
                                    MetaWindow     *window,
                                    MetaEffectType  effect)
{
}

static void
meta_compositor_vulkan_hide_window (MetaCompositor *compositor,
                                    MetaWindow     *window,
                                    MetaEffectType  effect)
{
}

static void
meta_compositor_vulkan_window_opacity_changed (MetaCompositor *compositor,
                                               MetaWindow     *window)
{
}

static void
meta_compositor_vulkan_window_shape_changed (MetaCompositor *compositor,
                                             MetaWindow     *window)
{
}

static void
meta_compositor_vulkan_set_updates_frozen (MetaCompositor *compositor,
                                           MetaWindow     *window,
                                           gboolean        updates_frozen)
{
}

static void
meta_compositor_vulkan_process_event (MetaCompositor *compositor,
                                      XEvent         *event,
                                      MetaWindow     *window)
{
}

static cairo_surface_t *
meta_compositor_vulkan_get_window_surface (MetaCompositor *compositor,
                                           MetaWindow     *window)
{
  return NULL;
}

static void
meta_compositor_vulkan_maximize_window (MetaCompositor *compositor,
                                        MetaWindow     *window)
{
}

static void
meta_compositor_vulkan_unmaximize_window (MetaCompositor *compositor,
                                          MetaWindow     *window)
{
}

static void
meta_compositor_vulkan_sync_stack (MetaCompositor *compositor,
                                   GList          *stack)
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
  compositor_class->remove_window = meta_compositor_vulkan_remove_window;
  compositor_class->show_window = meta_compositor_vulkan_show_window;
  compositor_class->hide_window = meta_compositor_vulkan_hide_window;
  compositor_class->window_opacity_changed = meta_compositor_vulkan_window_opacity_changed;
  compositor_class->window_shape_changed = meta_compositor_vulkan_window_shape_changed;
  compositor_class->set_updates_frozen = meta_compositor_vulkan_set_updates_frozen;
  compositor_class->process_event = meta_compositor_vulkan_process_event;
  compositor_class->get_window_surface = meta_compositor_vulkan_get_window_surface;
  compositor_class->maximize_window = meta_compositor_vulkan_maximize_window;
  compositor_class->unmaximize_window = meta_compositor_vulkan_unmaximize_window;
  compositor_class->sync_stack = meta_compositor_vulkan_sync_stack;
}

static void
meta_compositor_vulkan_init (MetaCompositorVulkan *vulkan)
{
}
