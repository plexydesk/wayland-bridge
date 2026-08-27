/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#include "keyboard-shortcuts-inhibit-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>

struct shortcuts_inhibitor {
  struct wl_resource *resource;
  struct bridge_surface *surface;
  struct wl_resource *seat_resource;
  bool active;
};

static void inhibitor_destroy(struct wl_client *client,
                              struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct zwp_keyboard_shortcuts_inhibitor_v1_interface
    inhibitor_impl = {
        .destroy = inhibitor_destroy,
};

static void inhibitor_resource_destroy(struct wl_resource *resource) {
  struct shortcuts_inhibitor *inhibitor = wl_resource_get_user_data(resource);
  if (inhibitor) {
    LOG_DEBUG("keyboard_shortcuts_inhibit: inhibitor destroyed for surface=%p",
              (void *)inhibitor->surface);
    free(inhibitor);
  }
}

static void manager_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void manager_inhibit_shortcuts(struct wl_client *client,
                                      struct wl_resource *resource, uint32_t id,
                                      struct wl_resource *surface_resource,
                                      struct wl_resource *seat_resource) {
  struct bridge_surface *surface = wl_resource_get_user_data(surface_resource);

  struct wl_resource *inhibitor_resource =
      wl_resource_create(client, &zwp_keyboard_shortcuts_inhibitor_v1_interface,
                         wl_resource_get_version(resource), id);

  if (!inhibitor_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct shortcuts_inhibitor *inhibitor = calloc(1, sizeof(*inhibitor));
  if (!inhibitor) {
    wl_resource_destroy(inhibitor_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  inhibitor->resource = inhibitor_resource;
  inhibitor->surface = surface;
  inhibitor->seat_resource = seat_resource;
  inhibitor->active = true;

  wl_resource_set_implementation(inhibitor_resource, &inhibitor_impl, inhibitor,
                                 inhibitor_resource_destroy);

  zwp_keyboard_shortcuts_inhibitor_v1_send_active(inhibitor_resource);

  LOG_DEBUG(
      "keyboard_shortcuts_inhibit: created for surface=%p, sending active",
      (void *)surface);
}

static const struct zwp_keyboard_shortcuts_inhibit_manager_v1_interface
    manager_impl = {
        .destroy = manager_destroy,
        .inhibit_shortcuts = manager_inhibit_shortcuts,
};

void bind_keyboard_shortcuts_inhibit_manager(struct wl_client *client,
                                             void *data, uint32_t version,
                                             uint32_t id) {
  (void)data;
  struct wl_resource *resource = wl_resource_create(
      client, &zwp_keyboard_shortcuts_inhibit_manager_v1_interface, version,
      id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}
