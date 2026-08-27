/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "wayland_bridge.h"
#include <stdlib.h>
#include <wayland-server-protocol.h>

void bridge_subsurface_enforce_zorder(struct bridge_surface *parent) {
  if (!parent || !bridge || !bridge->plexy_conn)
    return;
  struct bridge_surface *child;
  wl_list_for_each(child, &parent->subsurfaces, subsurface_link) {
    if (child->plexy_window_id)
      plexy_raise_window(bridge->plexy_conn, child->plexy_window_id);
  }
}

struct subsurface_data {
  struct bridge_surface *surface;
  struct bridge_surface *parent;
  struct wl_resource *resource;

  struct wl_listener surface_destroy_listener;
};

static void subsurface_on_surface_destroyed(struct wl_listener *listener,
                                            void *data_ptr) {
  (void)data_ptr;
  struct subsurface_data *data =
      wl_container_of(listener, data, surface_destroy_listener);

  data->surface = NULL;
}

static void subsurface_handle_destroy(struct wl_resource *resource) {
  struct subsurface_data *data = wl_resource_get_user_data(resource);
  if (data) {
    if (data->surface) {

      wl_list_remove(&data->surface_destroy_listener.link);
      data->surface->is_subsurface = false;
      if (data->surface->subsurface_parent) {
        wl_list_remove(&data->surface->subsurface_link);
        wl_list_init(&data->surface->subsurface_link);
        data->surface->subsurface_parent = NULL;
      }
    }

    free(data);
  }
}

static void subsurface_destroy(struct wl_client *client,
                               struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void subsurface_set_position(struct wl_client *client,
                                    struct wl_resource *resource, int32_t x,
                                    int32_t y) {
  struct subsurface_data *data = wl_resource_get_user_data(resource);
  if (data && data->surface) {
    data->surface->subsurface_x = x;
    data->surface->subsurface_y = y;
    LOG_TRACE("Subsurface position: surface=%p x=%d y=%d",
              (void *)data->surface, x, y);
  }
}

static void subsurface_place_above(struct wl_client *client,
                                   struct wl_resource *resource,
                                   struct wl_resource *sibling_resource) {
  (void)client;
  struct subsurface_data *data = wl_resource_get_user_data(resource);
  if (!data || !data->surface || !data->parent)
    return;

  struct bridge_surface *sib = bridge_surface_from_resource(sibling_resource);
  if (!sib)
    return;

  wl_list_remove(&data->surface->subsurface_link);
  if (sib == data->parent) {

    wl_list_insert(data->parent->subsurfaces.prev,
                   &data->surface->subsurface_link);
  } else {

    wl_list_insert(&sib->subsurface_link, &data->surface->subsurface_link);
  }

  if (!data->surface->subsurface_sync) {
    bridge_subsurface_enforce_zorder(data->parent);
  }
  LOG_TRACE("Subsurface place_above: surface=%p sib=%p", (void *)data->surface,
            (void *)sib);
}

static void subsurface_place_below(struct wl_client *client,
                                   struct wl_resource *resource,
                                   struct wl_resource *sibling_resource) {
  (void)client;
  struct subsurface_data *data = wl_resource_get_user_data(resource);
  if (!data || !data->surface || !data->parent)
    return;

  struct bridge_surface *sib = bridge_surface_from_resource(sibling_resource);
  if (!sib)
    return;

  wl_list_remove(&data->surface->subsurface_link);
  if (sib == data->parent) {

    wl_list_insert(&data->parent->subsurfaces, &data->surface->subsurface_link);
  } else {

    wl_list_insert(sib->subsurface_link.prev, &data->surface->subsurface_link);
  }

  if (!data->surface->subsurface_sync) {
    bridge_subsurface_enforce_zorder(data->parent);
  }
  LOG_TRACE("Subsurface place_below: surface=%p sib=%p", (void *)data->surface,
            (void *)sib);
}

static void subsurface_set_sync(struct wl_client *client,
                                struct wl_resource *resource) {
  struct subsurface_data *data = wl_resource_get_user_data(resource);
  if (data && data->surface) {
    data->surface->subsurface_sync = true;
    LOG_TRACE("Subsurface sync mode: surface=%p", (void *)data->surface);
  }
}

static void subsurface_set_desync(struct wl_client *client,
                                  struct wl_resource *resource) {
  struct subsurface_data *data = wl_resource_get_user_data(resource);
  if (data && data->surface) {
    data->surface->subsurface_sync = false;
    LOG_TRACE("Subsurface desync mode: surface=%p", (void *)data->surface);
  }
}

static const struct wl_subsurface_interface subsurface_implementation = {
    .destroy = subsurface_destroy,
    .set_position = subsurface_set_position,
    .place_above = subsurface_place_above,
    .place_below = subsurface_place_below,
    .set_sync = subsurface_set_sync,
    .set_desync = subsurface_set_desync,
};

static void subcompositor_destroy(struct wl_client *client,
                                  struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void subcompositor_get_subsurface(struct wl_client *client,
                                         struct wl_resource *resource,
                                         uint32_t id,
                                         struct wl_resource *surface_resource,
                                         struct wl_resource *parent_resource) {
  struct bridge_surface *surface =
      bridge_surface_from_resource(surface_resource);
  struct bridge_surface *parent = bridge_surface_from_resource(parent_resource);

  if (!surface || !parent) {
    wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                           "Invalid surface or parent");
    return;
  }

  if (surface->xdg_surface || surface->is_subsurface) {
    wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                           "Surface already has a role");
    return;
  }

  struct subsurface_data *data = calloc(1, sizeof(*data));
  if (!data) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct wl_resource *subsurface =
      wl_resource_create(client, &wl_subsurface_interface, 1, id);

  if (!subsurface) {
    free(data);
    wl_resource_post_no_memory(resource);
    return;
  }

  data->surface = surface;
  data->parent = parent;
  data->resource = subsurface;

  data->surface_destroy_listener.notify = subsurface_on_surface_destroyed;
  wl_resource_add_destroy_listener(surface_resource,
                                   &data->surface_destroy_listener);

  surface->is_subsurface = true;
  surface->subsurface_parent = parent;
  surface->subsurface_x = 0;
  surface->subsurface_y = 0;
  surface->subsurface_sync = true;

  wl_list_insert(&parent->subsurfaces, &surface->subsurface_link);

  wl_resource_set_implementation(subsurface, &subsurface_implementation, data,
                                 subsurface_handle_destroy);
  LOG_DEBUG("Subsurface created: surface=%p parent=%p parent_window=%p",
            (void *)surface, (void *)parent, (void *)parent->plexy_window);
}

static const struct wl_subcompositor_interface subcompositor_implementation = {
    .destroy = subcompositor_destroy,
    .get_subsurface = subcompositor_get_subsurface,
};

void bind_subcompositor(struct wl_client *client, void *data, uint32_t version,
                        uint32_t id) {
  struct wl_resource *resource =
      wl_resource_create(client, &wl_subcompositor_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &subcompositor_implementation, NULL,
                                 NULL);
  LOG_DEBUG("Subcompositor bound: version=%u", version);
}
