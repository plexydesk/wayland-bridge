/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#include "wayland_bridge.h"
#include "xdg-toplevel-drag-v1-protocol.h"
#include <stdlib.h>

struct toplevel_drag {
  struct wl_resource *resource;
  struct wl_resource *toplevel;
  int32_t x_offset;
  int32_t y_offset;
};

static void drag_destroy(struct wl_client *client,
                         struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void drag_attach(struct wl_client *client, struct wl_resource *resource,
                        struct wl_resource *toplevel, int32_t x_offset,
                        int32_t y_offset) {
  (void)client;
  struct toplevel_drag *drag = wl_resource_get_user_data(resource);
  if (drag) {
    drag->toplevel = toplevel;
    drag->x_offset = x_offset;
    drag->y_offset = y_offset;
  }

  LOG_DEBUG("toplevel_drag: attach toplevel=%p offset=(%d,%d)",
            (void *)toplevel, x_offset, y_offset);
}

static const struct xdg_toplevel_drag_v1_interface drag_impl = {
    .destroy = drag_destroy,
    .attach = drag_attach,
};

static void drag_resource_destroy(struct wl_resource *resource) {
  struct toplevel_drag *drag = wl_resource_get_user_data(resource);
  free(drag);
}

static void manager_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void manager_get_xdg_toplevel_drag(struct wl_client *client,
                                          struct wl_resource *resource,
                                          uint32_t id,
                                          struct wl_resource *data_source) {
  (void)data_source;

  struct wl_resource *drag_resource =
      wl_resource_create(client, &xdg_toplevel_drag_v1_interface,
                         wl_resource_get_version(resource), id);

  if (!drag_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct toplevel_drag *drag = calloc(1, sizeof(*drag));
  if (!drag) {
    wl_resource_destroy(drag_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  drag->resource = drag_resource;

  wl_resource_set_implementation(drag_resource, &drag_impl, drag,
                                 drag_resource_destroy);

  LOG_DEBUG("toplevel_drag: created for data_source=%p", (void *)data_source);
}

static const struct xdg_toplevel_drag_manager_v1_interface manager_impl = {
    .destroy = manager_destroy,
    .get_xdg_toplevel_drag = manager_get_xdg_toplevel_drag,
};

void bind_toplevel_drag_manager(struct wl_client *client, void *data,
                                uint32_t version, uint32_t id) {
  (void)data;
  struct wl_resource *resource = wl_resource_create(
      client, &xdg_toplevel_drag_manager_v1_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}
