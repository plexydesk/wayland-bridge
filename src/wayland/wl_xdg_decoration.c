/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#include "wayland_bridge.h"
#include "xdg-decoration-v1-protocol.h"
#include <stdlib.h>

struct toplevel_decoration {
  struct wl_resource *resource;
  struct wl_resource *toplevel;
  struct bridge_surface *surface;
  uint32_t mode;
};

static void toplevel_decoration_destroy(struct wl_client *client,
                                        struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void toplevel_decoration_set_mode(struct wl_client *client,
                                         struct wl_resource *resource,
                                         uint32_t mode) {
  struct toplevel_decoration *deco = wl_resource_get_user_data(resource);
  if (deco) {
    deco->mode = mode;
    uint32_t actual_mode = mode;
    if (actual_mode != ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE &&
        actual_mode != ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE) {
      actual_mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
    }
    if (deco->surface) {
      deco->surface->xdg_decoration_mode = actual_mode;
      if (deco->surface->plexy_window) {
        bool server_side =
            actual_mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
        plexy_window_set_decorations(deco->surface->plexy_window, server_side);
      }
    }
    zxdg_toplevel_decoration_v1_send_configure(resource, actual_mode);
  }
}

static void toplevel_decoration_unset_mode(struct wl_client *client,
                                           struct wl_resource *resource) {
  struct toplevel_decoration *deco = wl_resource_get_user_data(resource);
  if (deco) {
    deco->mode = 0;
    uint32_t actual_mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
    if (deco->surface) {
      deco->surface->xdg_decoration_mode = actual_mode;
      if (deco->surface->plexy_window) {
        plexy_window_set_decorations(deco->surface->plexy_window, false);
      }
    }
    zxdg_toplevel_decoration_v1_send_configure(resource, actual_mode);
  }
}

static const struct zxdg_toplevel_decoration_v1_interface
    toplevel_decoration_impl = {
        .destroy = toplevel_decoration_destroy,
        .set_mode = toplevel_decoration_set_mode,
        .unset_mode = toplevel_decoration_unset_mode,
};

static void toplevel_decoration_resource_destroy(struct wl_resource *resource) {
  struct toplevel_decoration *deco = wl_resource_get_user_data(resource);
  if (deco) {
    if (deco->surface && deco->surface->xdg_toplevel_decoration == resource) {
      deco->surface->xdg_toplevel_decoration = NULL;
      deco->surface->xdg_decoration_mode =
          ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
    }
    free(deco);
  }
}

static void decoration_manager_destroy(struct wl_client *client,
                                       struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void decoration_manager_get_toplevel_decoration(
    struct wl_client *client, struct wl_resource *resource, uint32_t id,
    struct wl_resource *toplevel) {
  struct bridge_surface *surface = wl_resource_get_user_data(toplevel);
  if (!surface) {
    wl_resource_post_error(
        resource, ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ORPHANED,
        "toplevel decoration requested for invalid toplevel");
    return;
  }

  struct wl_resource *deco_resource =
      wl_resource_create(client, &zxdg_toplevel_decoration_v1_interface,
                         wl_resource_get_version(resource), id);

  if (!deco_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct toplevel_decoration *deco = calloc(1, sizeof(*deco));
  if (!deco) {
    wl_resource_destroy(deco_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  deco->resource = deco_resource;
  deco->toplevel = toplevel;
  deco->surface = surface;
  deco->mode = 0;

  wl_resource_set_implementation(deco_resource, &toplevel_decoration_impl, deco,
                                 toplevel_decoration_resource_destroy);

  surface->xdg_toplevel_decoration = deco_resource;
  surface->xdg_decoration_mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;

  zxdg_toplevel_decoration_v1_send_configure(
      deco_resource, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

static const struct zxdg_decoration_manager_v1_interface
    decoration_manager_impl = {
        .destroy = decoration_manager_destroy,
        .get_toplevel_decoration = decoration_manager_get_toplevel_decoration,
};

void bind_xdg_decoration_manager(struct wl_client *client, void *data,
                                 uint32_t version, uint32_t id) {
  struct wl_resource *resource = wl_resource_create(
      client, &zxdg_decoration_manager_v1_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &decoration_manager_impl, NULL,
                                 NULL);
}
