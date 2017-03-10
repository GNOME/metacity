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

#include "meta-compositor-vulkan.h"
#include "util.h"

struct _MetaCompositorVulkan
{
  MetaCompositor parent;
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

  if (!meta_check_debug_flags (META_DEBUG_VULKAN))
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
      meta_topic (META_DEBUG_VULKAN, "  %s v%u.%u.%u (%s)\n",
                  layers[i].layerName,
                  VK_VERSION_MAJOR (layers[i].specVersion),
                  VK_VERSION_MINOR (layers[i].specVersion),
                  VK_VERSION_PATCH (layers[i].specVersion),
                  layers[i].description);
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

  if (!meta_check_debug_flags (META_DEBUG_VULKAN))
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
      meta_topic (META_DEBUG_VULKAN, "  %s v%u.%u.%u\n",
                  extensions[i].extensionName,
                  VK_VERSION_MAJOR (extensions[i].specVersion),
                  VK_VERSION_MINOR (extensions[i].specVersion),
                  VK_VERSION_PATCH (extensions[i].specVersion));
    }

  meta_pop_no_msg_prefix ();

  g_free (extensions);
}
#endif

static gboolean
meta_compositor_vulkan_manage (MetaCompositor  *compositor,
                               GError         **error)
{
#ifdef HAVE_VULKAN
  MetaCompositorVulkan *vulkan;

  vulkan = META_COMPOSITOR_VULKAN (compositor);

  enumerate_instance_layers (vulkan);
  enumerate_instance_extensions (vulkan);

  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Not implemented");

  return FALSE;
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
meta_compositor_vulkan_set_active_window (MetaCompositor *compositor,
                                          MetaWindow     *window)
{
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

static gboolean
meta_compositor_vulkan_is_our_xwindow (MetaCompositor *compositor,
                                       Window          xwindow)
{
  return FALSE;
}

static void
meta_compositor_vulkan_class_init (MetaCompositorVulkanClass *vulkan_class)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_CLASS (vulkan_class);

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
  compositor_class->set_active_window = meta_compositor_vulkan_set_active_window;
  compositor_class->maximize_window = meta_compositor_vulkan_maximize_window;
  compositor_class->unmaximize_window = meta_compositor_vulkan_unmaximize_window;
  compositor_class->sync_stack = meta_compositor_vulkan_sync_stack;
  compositor_class->is_our_xwindow = meta_compositor_vulkan_is_our_xwindow;
}

static void
meta_compositor_vulkan_init (MetaCompositorVulkan *vulkan)
{
}
