/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#include "single-pixel-buffer-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>
#include <string.h>

struct single_pixel_buffer {
  uint32_t r, g, b, a;
};

static void buffer_destroy(struct wl_client *client,
                           struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct wl_buffer_interface buffer_impl = {
    .destroy = buffer_destroy,
};

static void buffer_resource_destroy(struct wl_resource *resource) {
  struct single_pixel_buffer *buf = wl_resource_get_user_data(resource);
  free(buf);
}

static void manager_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void manager_create_u32_rgba_buffer(struct wl_client *client,
                                           struct wl_resource *resource,
                                           uint32_t id, uint32_t r, uint32_t g,
                                           uint32_t b, uint32_t a) {
  struct single_pixel_buffer *buf = calloc(1, sizeof(*buf));
  if (!buf) {
    wl_resource_post_no_memory(resource);
    return;
  }

  buf->r = r;
  buf->g = g;
  buf->b = b;
  buf->a = a;

  struct wl_resource *buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, id);

  if (!buffer_resource) {
    free(buf);
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource_set_implementation(buffer_resource, &buffer_impl, buf,
                                 buffer_resource_destroy);

  LOG_DEBUG("single_pixel_buffer: created rgba(%u,%u,%u,%u)", r, g, b, a);
}

static const struct wp_single_pixel_buffer_manager_v1_interface manager_impl = {
    .destroy = manager_destroy,
    .create_u32_rgba_buffer = manager_create_u32_rgba_buffer,
};

void bind_single_pixel_buffer_manager(struct wl_client *client, void *data,
                                      uint32_t version, uint32_t id) {
  (void)data;
  struct wl_resource *resource = wl_resource_create(
      client, &wp_single_pixel_buffer_manager_v1_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}
