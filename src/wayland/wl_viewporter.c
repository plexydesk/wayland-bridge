/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#include "viewporter-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>

struct viewport_data {
  struct wl_resource *surface;
};

static void viewport_destroy(struct wl_client *client,
                             struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void viewport_set_source(struct wl_client *client,
                                struct wl_resource *resource, wl_fixed_t x,
                                wl_fixed_t y, wl_fixed_t width,
                                wl_fixed_t height) {
  (void)client;
  struct viewport_data *data = wl_resource_get_user_data(resource);
  if (!data)
    return;

  struct bridge_surface *surface = bridge_surface_from_resource(data->surface);
  if (!surface)
    return;

  if (x == wl_fixed_from_int(-1) && y == wl_fixed_from_int(-1) &&
      width == wl_fixed_from_int(-1) && height == wl_fixed_from_int(-1)) {
    surface->viewport_src_set = false;
    return;
  }

  if (width <= 0 || height <= 0) {
    wl_resource_post_error(
        resource, WP_VIEWPORT_ERROR_BAD_VALUE,
        "wp_viewport.set_source: width/height must be > 0 or -1");
    return;
  }

  surface->viewport_src_set = true;
  surface->viewport_src_x = x;
  surface->viewport_src_y = y;
  surface->viewport_src_width = width;
  surface->viewport_src_height = height;
}

static void viewport_set_destination(struct wl_client *client,
                                     struct wl_resource *resource,
                                     int32_t width, int32_t height) {
  (void)client;
  struct viewport_data *data = wl_resource_get_user_data(resource);
  if (!data)
    return;

  struct bridge_surface *surface = bridge_surface_from_resource(data->surface);
  if (!surface)
    return;

  if (width == -1 && height == -1) {
    surface->viewport_dst_set = false;
    return;
  }

  if (width <= 0 || height <= 0) {
    wl_resource_post_error(
        resource, WP_VIEWPORT_ERROR_BAD_VALUE,
        "wp_viewport.set_destination: width/height must be > 0 or -1");
    return;
  }

  surface->viewport_dst_set = true;
  surface->viewport_dst_width = width;
  surface->viewport_dst_height = height;
}

static const struct wp_viewport_interface viewport_impl = {
    .destroy = viewport_destroy,
    .set_source = viewport_set_source,
    .set_destination = viewport_set_destination,
};

static void viewport_resource_destroy(struct wl_resource *resource) {
  struct viewport_data *data = wl_resource_get_user_data(resource);
  if (data && data->surface) {
    struct bridge_surface *surface =
        bridge_surface_from_resource(data->surface);
    if (surface && surface->viewport_resource == resource) {
      surface->viewport_resource = NULL;
      surface->viewport_src_set = false;
      surface->viewport_dst_set = false;
    }
  }
  free(data);
}

static void viewporter_destroy(struct wl_client *client,
                               struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void viewporter_get_viewport(struct wl_client *client,
                                    struct wl_resource *resource, uint32_t id,
                                    struct wl_resource *surface) {
  struct wl_resource *viewport = wl_resource_create(
      client, &wp_viewport_interface, wl_resource_get_version(resource), id);

  if (!viewport) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct viewport_data *data = calloc(1, sizeof(*data));
  if (!data) {
    wl_resource_destroy(viewport);
    wl_resource_post_no_memory(resource);
    return;
  }

  data->surface = surface;

  struct bridge_surface *bridge_surface = bridge_surface_from_resource(surface);
  if (!bridge_surface) {
    free(data);
    wl_resource_destroy(viewport);
    wl_resource_post_error(resource, WP_VIEWPORTER_ERROR_VIEWPORT_EXISTS,
                           "wp_viewporter.get_viewport: invalid wl_surface");
    return;
  }

  if (bridge_surface->viewport_resource) {
    free(data);
    wl_resource_destroy(viewport);
    wl_resource_post_error(
        resource, WP_VIEWPORTER_ERROR_VIEWPORT_EXISTS,
        "wp_viewporter.get_viewport: surface already has viewport");
    return;
  }
  bridge_surface->viewport_resource = viewport;

  wl_resource_set_implementation(viewport, &viewport_impl, data,
                                 viewport_resource_destroy);
}

static const struct wp_viewporter_interface viewporter_impl = {
    .destroy = viewporter_destroy,
    .get_viewport = viewporter_get_viewport,
};

void bind_viewporter(struct wl_client *client, void *data, uint32_t version,
                     uint32_t id) {
  struct wl_resource *resource =
      wl_resource_create(client, &wp_viewporter_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &viewporter_impl, NULL, NULL);
}
