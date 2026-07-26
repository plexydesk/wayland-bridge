/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#include "alpha-modifier-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>

struct alpha_modifier_surface {
  struct wl_resource *resource;
  struct bridge_surface *surface;
  uint32_t multiplier;
};

static void alpha_surface_destroy(struct wl_client *client,
                                  struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void alpha_surface_set_multiplier(struct wl_client *client,
                                         struct wl_resource *resource,
                                         uint32_t factor) {
  (void)client;
  struct alpha_modifier_surface *ams = wl_resource_get_user_data(resource);
  if (ams)
    ams->multiplier = factor;
}

static const struct wp_alpha_modifier_surface_v1_interface alpha_surface_impl =
    {
        .destroy = alpha_surface_destroy,
        .set_multiplier = alpha_surface_set_multiplier,
};

static void alpha_surface_resource_destroy(struct wl_resource *resource) {
  struct alpha_modifier_surface *ams = wl_resource_get_user_data(resource);
  free(ams);
}

static void manager_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void manager_get_surface(struct wl_client *client,
                                struct wl_resource *resource, uint32_t id,
                                struct wl_resource *surface_resource) {
  struct bridge_surface *surface = wl_resource_get_user_data(surface_resource);

  struct wl_resource *ams_resource =
      wl_resource_create(client, &wp_alpha_modifier_surface_v1_interface,
                         wl_resource_get_version(resource), id);

  if (!ams_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct alpha_modifier_surface *ams = calloc(1, sizeof(*ams));
  if (!ams) {
    wl_resource_destroy(ams_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  ams->resource = ams_resource;
  ams->surface = surface;
  ams->multiplier = UINT32_MAX;

  wl_resource_set_implementation(ams_resource, &alpha_surface_impl, ams,
                                 alpha_surface_resource_destroy);

  LOG_DEBUG("alpha_modifier: created for surface=%p", (void *)surface);
}

static const struct wp_alpha_modifier_v1_interface manager_impl = {
    .destroy = manager_destroy,
    .get_surface = manager_get_surface,
};

void bind_alpha_modifier(struct wl_client *client, void *data, uint32_t version,
                         uint32_t id) {
  (void)data;
  struct wl_resource *resource =
      wl_resource_create(client, &wp_alpha_modifier_v1_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}
