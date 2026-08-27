/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#include "fractional-scale-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>

struct fractional_scale {
  struct wl_resource *resource;
  struct bridge_surface *surface;
};

static void fractional_scale_destroy(struct wl_client *client,
                                     struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static const struct wp_fractional_scale_v1_interface fractional_scale_impl = {
    .destroy = fractional_scale_destroy,
};

static void fractional_scale_resource_destroy(struct wl_resource *resource) {
  struct fractional_scale *fs = wl_resource_get_user_data(resource);
  if (fs) {
    free(fs);
  }
}

static void fractional_scale_manager_destroy(struct wl_client *client,
                                             struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void fractional_scale_manager_get_fractional_scale(
    struct wl_client *client, struct wl_resource *resource, uint32_t id,
    struct wl_resource *surface_resource) {
  struct bridge_surface *surface = wl_resource_get_user_data(surface_resource);

  struct wl_resource *fs_resource =
      wl_resource_create(client, &wp_fractional_scale_v1_interface,
                         wl_resource_get_version(resource), id);

  if (!fs_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct fractional_scale *fs = calloc(1, sizeof(*fs));
  if (!fs) {
    wl_resource_destroy(fs_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  fs->resource = fs_resource;
  fs->surface = surface;

  wl_resource_set_implementation(fs_resource, &fractional_scale_impl, fs,
                                 fractional_scale_resource_destroy);
  float scale = bridge_surface_scale_factor(surface);
  uint32_t scale_120ths = (uint32_t)(scale * 120.0f + 0.5f);
  if (scale_120ths < 120)
    scale_120ths = 120;

  wp_fractional_scale_v1_send_preferred_scale(fs_resource, scale_120ths);

  wl_client_flush(client);

  LOG_DEBUG("fractional_scale: created for surface=%p scale=%u/120",
            (void *)surface, scale_120ths);
}

static const struct wp_fractional_scale_manager_v1_interface
    fractional_scale_manager_impl = {
        .destroy = fractional_scale_manager_destroy,
        .get_fractional_scale = fractional_scale_manager_get_fractional_scale,
};

void bind_fractional_scale_manager(struct wl_client *client, void *data,
                                   uint32_t version, uint32_t id) {
  struct wl_resource *resource = wl_resource_create(
      client, &wp_fractional_scale_manager_v1_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &fractional_scale_manager_impl, NULL,
                                 NULL);
}
