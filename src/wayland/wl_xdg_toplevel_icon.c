/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#define _POSIX_C_SOURCE 200809L
#include "wayland_bridge.h"
#include "xdg-toplevel-icon-v1-protocol.h"
#include <stdlib.h>
#include <string.h>

struct toplevel_icon {
  struct wl_resource *resource;
  char *name;
  bool immutable;
};

static void icon_destroy(struct wl_client *client,
                         struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void icon_set_name(struct wl_client *client,
                          struct wl_resource *resource, const char *name) {
  (void)client;
  struct toplevel_icon *icon = wl_resource_get_user_data(resource);
  if (!icon)
    return;

  if (icon->immutable) {
    wl_resource_post_error(resource, XDG_TOPLEVEL_ICON_V1_ERROR_IMMUTABLE,
                           "Attempted to modify an immutable toplevel icon");
    return;
  }

  free(icon->name);
  icon->name = name ? strdup(name) : NULL;
}

static void icon_add_buffer(struct wl_client *client,
                            struct wl_resource *resource,
                            struct wl_resource *buffer, int32_t scale) {
  (void)client;
  (void)buffer;
  (void)scale;

  struct toplevel_icon *icon = wl_resource_get_user_data(resource);
  if (!icon)
    return;

  if (icon->immutable) {
    wl_resource_post_error(resource, XDG_TOPLEVEL_ICON_V1_ERROR_IMMUTABLE,
                           "Attempted to modify an immutable toplevel icon");
    return;
  }
}

static const struct xdg_toplevel_icon_v1_interface icon_impl = {
    .destroy = icon_destroy,
    .set_name = icon_set_name,
    .add_buffer = icon_add_buffer,
};

static void icon_resource_destroy(struct wl_resource *resource) {
  struct toplevel_icon *icon = wl_resource_get_user_data(resource);
  if (icon) {
    free(icon->name);
    free(icon);
  }
}

static void manager_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void manager_create_icon(struct wl_client *client,
                                struct wl_resource *resource, uint32_t id) {
  struct wl_resource *icon_resource =
      wl_resource_create(client, &xdg_toplevel_icon_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!icon_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct toplevel_icon *icon = calloc(1, sizeof(*icon));
  if (!icon) {
    wl_resource_destroy(icon_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  icon->resource = icon_resource;
  wl_resource_set_implementation(icon_resource, &icon_impl, icon,
                                 icon_resource_destroy);
}

static void manager_set_icon(struct wl_client *client,
                             struct wl_resource *resource,
                             struct wl_resource *toplevel,
                             struct wl_resource *icon_res) {
  (void)client;
  (void)resource;

  struct bridge_surface *surface = wl_resource_get_user_data(toplevel);
  if (!surface)
    return;

  if (icon_res) {
    struct toplevel_icon *icon = wl_resource_get_user_data(icon_res);
    if (icon) {
      icon->immutable = true;
      free(surface->icon_name);
      surface->icon_name = icon->name ? strdup(icon->name) : NULL;
    } else {

      free(surface->icon_name);
      surface->icon_name = NULL;
    }
  } else {

    free(surface->icon_name);
    surface->icon_name = NULL;
  }

  if (surface->plexy_window) {
    plexy_window_set_icon_name(surface->plexy_window,
                               surface->icon_name ? surface->icon_name : "");
  }
}

static const struct xdg_toplevel_icon_manager_v1_interface manager_impl = {
    .destroy = manager_destroy,
    .create_icon = manager_create_icon,
    .set_icon = manager_set_icon,
};

void bind_xdg_toplevel_icon_manager(struct wl_client *client, void *data,
                                    uint32_t version, uint32_t id) {
  (void)data;
  struct wl_resource *resource = wl_resource_create(
      client, &xdg_toplevel_icon_manager_v1_interface, version, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);

  xdg_toplevel_icon_manager_v1_send_icon_size(resource, 32);
  xdg_toplevel_icon_manager_v1_send_icon_size(resource, 48);
  xdg_toplevel_icon_manager_v1_send_icon_size(resource, 64);
  xdg_toplevel_icon_manager_v1_send_icon_size(resource, 128);
  xdg_toplevel_icon_manager_v1_send_done(resource);
}
