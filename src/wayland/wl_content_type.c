/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#include "content-type-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>

struct content_type_surface {
  struct wl_resource *resource;
  struct bridge_surface *surface;
  uint32_t content_type;
};

static void content_type_destroy(struct wl_client *client,
                                 struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void content_type_set_content_type(struct wl_client *client,
                                          struct wl_resource *resource,
                                          uint32_t content_type) {
  (void)client;
  struct content_type_surface *ct = wl_resource_get_user_data(resource);
  if (ct)
    ct->content_type = content_type;
}

static const struct wp_content_type_v1_interface content_type_impl = {
    .destroy = content_type_destroy,
    .set_content_type = content_type_set_content_type,
};

static void content_type_resource_destroy(struct wl_resource *resource) {
  struct content_type_surface *ct = wl_resource_get_user_data(resource);
  free(ct);
}

static void manager_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void
manager_get_surface_content_type(struct wl_client *client,
                                 struct wl_resource *resource, uint32_t id,
                                 struct wl_resource *surface_resource) {
  struct bridge_surface *surface = wl_resource_get_user_data(surface_resource);

  struct wl_resource *ct_resource =
      wl_resource_create(client, &wp_content_type_v1_interface,
                         wl_resource_get_version(resource), id);

  if (!ct_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct content_type_surface *ct = calloc(1, sizeof(*ct));
  if (!ct) {
    wl_resource_destroy(ct_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  ct->resource = ct_resource;
  ct->surface = surface;
  ct->content_type = WP_CONTENT_TYPE_V1_TYPE_NONE;

  wl_resource_set_implementation(ct_resource, &content_type_impl, ct,
                                 content_type_resource_destroy);

  LOG_DEBUG("content_type: created for surface=%p", (void *)surface);
}

static const struct wp_content_type_manager_v1_interface manager_impl = {
    .destroy = manager_destroy,
    .get_surface_content_type = manager_get_surface_content_type,
};

void bind_content_type_manager(struct wl_client *client, void *data,
                               uint32_t version, uint32_t id) {
  (void)data;
  struct wl_resource *resource = wl_resource_create(
      client, &wp_content_type_manager_v1_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}
