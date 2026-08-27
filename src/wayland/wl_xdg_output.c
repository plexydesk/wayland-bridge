/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#include "wayland_bridge.h"
#include "xdg-output-v1-protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct xdg_output_entry {
  struct wl_list link;
  struct wl_resource *resource;
  struct wl_resource *output_resource;
  struct wl_client *client;
};

static struct wl_list xdg_output_resources;
static bool xdg_output_resources_initialized = false;

static void ensure_xdg_output_list_initialized(void) {
  if (!xdg_output_resources_initialized) {
    wl_list_init(&xdg_output_resources);
    xdg_output_resources_initialized = true;
  }
}

static void xdg_output_resource_destroy(struct wl_resource *resource) {
  if (!xdg_output_resources_initialized) {
    return;
  }
  struct xdg_output_entry *entry = NULL;
  struct xdg_output_entry *tmp = NULL;
  wl_list_for_each_safe(entry, tmp, &xdg_output_resources, link) {
    if (entry->resource == resource) {
      wl_list_remove(&entry->link);
      free(entry);
      return;
    }
  }
}

static void xdg_output_destroy(struct wl_client *client,
                               struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct zxdg_output_v1_interface xdg_output_impl = {
    .destroy = xdg_output_destroy,
};

static void send_xdg_output_info(struct wl_client *client,
                                 struct wl_resource *resource,
                                 struct wl_resource *output) {
  PlexyOutputInfo info;
  if (!bridge_wl_output_resource_get_info(output, &info)) {
    return;
  }

  int32_t logical_width = (int32_t)info.pixel_width;
  int32_t logical_height = (int32_t)info.pixel_height;
  if (!bridge_is_xwayland_client(client)) {
    const float scale = bridge_output_scale_factor(&info);
    logical_width =
        (int32_t)bridge_physical_to_logical_extent(info.pixel_width, scale);
    logical_height =
        (int32_t)bridge_physical_to_logical_extent(info.pixel_height, scale);
  }

  zxdg_output_v1_send_logical_position(resource, info.x, info.y);
  zxdg_output_v1_send_logical_size(resource, logical_width, logical_height);

  uint32_t version = wl_resource_get_version(resource);
  if (version >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION) {
    char name[64];
    snprintf(name, sizeof(name), "PLEXY-%u", info.output_id);
    zxdg_output_v1_send_name(resource, name);
  }
  if (version >= ZXDG_OUTPUT_V1_DESCRIPTION_SINCE_VERSION) {
    char description[160];
    snprintf(description, sizeof(description), "%s %s",
             info.make[0] ? info.make : "PlexyShell",
             info.model[0] ? info.model : "Virtual Output");
    zxdg_output_v1_send_description(resource, description);
  }

  if (version < 3) {
    zxdg_output_v1_send_done(resource);
  }

  wl_client_flush(client);
}

static void xdg_output_manager_destroy(struct wl_client *client,
                                       struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void xdg_output_manager_get_xdg_output(struct wl_client *client,
                                              struct wl_resource *resource,
                                              uint32_t id,
                                              struct wl_resource *output) {
  ensure_xdg_output_list_initialized();

  struct wl_resource *xdg_output = wl_resource_create(
      client, &zxdg_output_v1_interface, wl_resource_get_version(resource), id);

  if (!xdg_output) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct xdg_output_entry *entry = calloc(1, sizeof(*entry));
  if (entry) {
    entry->resource = xdg_output;
    entry->output_resource = output;
    entry->client = client;
    wl_list_insert(&xdg_output_resources, &entry->link);
  }

  wl_resource_set_implementation(xdg_output, &xdg_output_impl, NULL,
                                 xdg_output_resource_destroy);

  send_xdg_output_info(client, xdg_output, output);

  if (wl_resource_get_version(xdg_output) >= 3) {
    wl_output_send_done(output);
    wl_client_flush(client);
  }

  LOG_DEBUG("xdg_output: created");
}

static const struct zxdg_output_manager_v1_interface xdg_output_manager_impl = {
    .destroy = xdg_output_manager_destroy,
    .get_xdg_output = xdg_output_manager_get_xdg_output,
};

void bind_xdg_output_manager(struct wl_client *client, void *data,
                             uint32_t version, uint32_t id) {
  (void)data;
  struct wl_resource *resource = wl_resource_create(
      client, &zxdg_output_manager_v1_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &xdg_output_manager_impl, NULL,
                                 NULL);

  LOG_DEBUG("xdg_output_manager: bound version=%u", version);
}

void bridge_refresh_xdg_outputs(void) {
  if (!xdg_output_resources_initialized) {
    return;
  }
  struct xdg_output_entry *entry = NULL;
  wl_list_for_each(entry, &xdg_output_resources, link) {
    if (!entry->resource || !entry->output_resource) {
      continue;
    }
    send_xdg_output_info(entry->client, entry->resource,
                         entry->output_resource);
    if (wl_resource_get_version(entry->resource) >= 3) {
    }
  }
}
