/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#include "tearing-control-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>

struct tearing_control {
  struct wl_resource *resource;
  struct bridge_surface *surface;
  uint32_t hint;
};

static void tearing_set_presentation_hint(struct wl_client *client,
                                          struct wl_resource *resource,
                                          uint32_t hint) {
  (void)client;
  struct tearing_control *tc = wl_resource_get_user_data(resource);
  if (tc)
    tc->hint = hint;
}

static void tearing_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct wp_tearing_control_v1_interface tearing_control_impl = {
    .set_presentation_hint = tearing_set_presentation_hint,
    .destroy = tearing_destroy,
};

static void tearing_resource_destroy(struct wl_resource *resource) {
  struct tearing_control *tc = wl_resource_get_user_data(resource);
  free(tc);
}

static void manager_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void manager_get_tearing_control(struct wl_client *client,
                                        struct wl_resource *resource,
                                        uint32_t id,
                                        struct wl_resource *surface_resource) {
  struct bridge_surface *surface = wl_resource_get_user_data(surface_resource);

  struct wl_resource *tc_resource =
      wl_resource_create(client, &wp_tearing_control_v1_interface,
                         wl_resource_get_version(resource), id);

  if (!tc_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct tearing_control *tc = calloc(1, sizeof(*tc));
  if (!tc) {
    wl_resource_destroy(tc_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  tc->resource = tc_resource;
  tc->surface = surface;
  tc->hint = WP_TEARING_CONTROL_V1_PRESENTATION_HINT_VSYNC;

  wl_resource_set_implementation(tc_resource, &tearing_control_impl, tc,
                                 tearing_resource_destroy);

  LOG_DEBUG("tearing_control: created for surface=%p", (void *)surface);
}

static const struct wp_tearing_control_manager_v1_interface manager_impl = {
    .destroy = manager_destroy,
    .get_tearing_control = manager_get_tearing_control,
};

void bind_tearing_control_manager(struct wl_client *client, void *data,
                                  uint32_t version, uint32_t id) {
  (void)data;
  struct wl_resource *resource = wl_resource_create(
      client, &wp_tearing_control_manager_v1_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}
