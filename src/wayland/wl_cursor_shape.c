/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#include "cursor-shape-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>

const struct wl_interface zwp_tablet_tool_v2_interface = {
    "zwp_tablet_tool_v2", 2, 0, NULL, 0, NULL};

struct cursor_shape_device {
  struct wl_resource *resource;
  struct wl_resource *pointer;
};

static void cursor_shape_device_destroy(struct wl_client *client,
                                        struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void cursor_shape_device_set_shape(struct wl_client *client,
                                          struct wl_resource *resource,
                                          uint32_t serial, uint32_t shape) {
  (void)client;
  (void)serial;
  LOG_TRACE("cursor_shape: set_shape serial=%u shape=%u", serial, shape);

  struct bridge_surface *focus = bridge ? bridge->pointer_focus : NULL;
  if (!bridge || !bridge->plexy_conn || !focus) {
    return;
  }
  if (focus->plexy_window_id == 0) {
    return;
  }
  bridge_surface_update_cursor_shape(focus, shape);
}

static const struct wp_cursor_shape_device_v1_interface
    cursor_shape_device_impl = {
        .destroy = cursor_shape_device_destroy,
        .set_shape = cursor_shape_device_set_shape,
};

static void cursor_shape_device_resource_destroy(struct wl_resource *resource) {
  struct cursor_shape_device *dev = wl_resource_get_user_data(resource);
  if (dev) {
    free(dev);
  }
}

static void cursor_shape_manager_destroy(struct wl_client *client,
                                         struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void cursor_shape_manager_get_pointer(struct wl_client *client,
                                             struct wl_resource *resource,
                                             uint32_t id,
                                             struct wl_resource *pointer) {
  struct wl_resource *dev_resource =
      wl_resource_create(client, &wp_cursor_shape_device_v1_interface,
                         wl_resource_get_version(resource), id);

  if (!dev_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct cursor_shape_device *dev = calloc(1, sizeof(*dev));
  if (!dev) {
    wl_resource_destroy(dev_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  dev->resource = dev_resource;
  dev->pointer = pointer;

  wl_resource_set_implementation(dev_resource, &cursor_shape_device_impl, dev,
                                 cursor_shape_device_resource_destroy);
}

static void cursor_shape_manager_get_tablet_tool_v2(
    struct wl_client *client, struct wl_resource *resource, uint32_t id,
    struct wl_resource *tablet_tool) {

  struct wl_resource *dev_resource =
      wl_resource_create(client, &wp_cursor_shape_device_v1_interface,
                         wl_resource_get_version(resource), id);

  if (!dev_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct cursor_shape_device *dev = calloc(1, sizeof(*dev));
  if (!dev) {
    wl_resource_destroy(dev_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  dev->resource = dev_resource;
  dev->pointer = NULL;

  wl_resource_set_implementation(dev_resource, &cursor_shape_device_impl, dev,
                                 cursor_shape_device_resource_destroy);
}

static const struct wp_cursor_shape_manager_v1_interface
    cursor_shape_manager_impl = {
        .destroy = cursor_shape_manager_destroy,
        .get_pointer = cursor_shape_manager_get_pointer,
        .get_tablet_tool_v2 = cursor_shape_manager_get_tablet_tool_v2,
};

void bind_cursor_shape_manager(struct wl_client *client, void *data,
                               uint32_t version, uint32_t id) {
  struct wl_resource *resource = wl_resource_create(
      client, &wp_cursor_shape_manager_v1_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &cursor_shape_manager_impl, NULL,
                                 NULL);
}
