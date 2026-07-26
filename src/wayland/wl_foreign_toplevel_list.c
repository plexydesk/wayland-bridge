/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#include "ext-foreign-toplevel-list-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>

struct toplevel_list {
  struct wl_resource *resource;
  bool stopped;
};

static void list_stop(struct wl_client *client, struct wl_resource *resource) {
  (void)client;
  struct toplevel_list *list = wl_resource_get_user_data(resource);
  if (list)
    list->stopped = true;

  ext_foreign_toplevel_list_v1_send_finished(resource);
}

static void list_destroy(struct wl_client *client,
                         struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct ext_foreign_toplevel_list_v1_interface list_impl = {
    .stop = list_stop,
    .destroy = list_destroy,
};

static void list_resource_destroy(struct wl_resource *resource) {
  struct toplevel_list *list = wl_resource_get_user_data(resource);
  free(list);
}

static void handle_destroy(struct wl_client *client,
                           struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct ext_foreign_toplevel_handle_v1_interface handle_impl = {
    .destroy = handle_destroy,
};

void bind_foreign_toplevel_list(struct wl_client *client, void *data,
                                uint32_t version, uint32_t id) {
  (void)data;
  struct wl_resource *resource = wl_resource_create(
      client, &ext_foreign_toplevel_list_v1_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  struct toplevel_list *list = calloc(1, sizeof(*list));
  if (!list) {
    wl_resource_destroy(resource);
    wl_client_post_no_memory(client);
    return;
  }

  list->resource = resource;
  list->stopped = false;

  wl_resource_set_implementation(resource, &list_impl, list,
                                 list_resource_destroy);

  if (bridge) {
    struct bridge_surface *surface;
    wl_list_for_each(surface, &bridge->surfaces, link) {
      if (!surface->xdg_toplevel)
        continue;

      struct wl_resource *handle_resource = wl_resource_create(
          client, &ext_foreign_toplevel_handle_v1_interface, version, 0);

      if (!handle_resource)
        continue;

      wl_resource_set_implementation(handle_resource, &handle_impl, surface,
                                     NULL);

      ext_foreign_toplevel_list_v1_send_toplevel(resource, handle_resource);

      if (surface->title)
        ext_foreign_toplevel_handle_v1_send_title(handle_resource,
                                                  surface->title);

      if (surface->app_id)
        ext_foreign_toplevel_handle_v1_send_app_id(handle_resource,
                                                   surface->app_id);

      ext_foreign_toplevel_handle_v1_send_done(handle_resource);
    }
  }

  ext_foreign_toplevel_list_v1_send_finished(resource);

  LOG_DEBUG("foreign_toplevel_list: bound for client");
}
