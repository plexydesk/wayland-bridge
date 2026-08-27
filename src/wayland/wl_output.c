/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "wayland_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-protocol.h>

static struct wl_list output_resources;
static bool output_resources_initialized = false;
static struct wl_list output_globals;
static bool output_globals_initialized = false;

struct output_global_entry {
  struct wl_list link;
  struct wl_global *global;
  uint32_t output_id;
};

struct output_resource {
  struct wl_list link;
  struct wl_resource *resource;
  struct wl_client *client;
  uint32_t output_id;
};

static void ensure_output_lists_initialized(void) {
  if (!output_resources_initialized) {
    wl_list_init(&output_resources);
    output_resources_initialized = true;
  }
  if (!output_globals_initialized) {
    wl_list_init(&output_globals);
    output_globals_initialized = true;
  }
}

static int32_t output_int_scale(const PlexyOutputInfo *info) {
  int32_t scale = 1;
  if (info && info->scale_factor >= 1.0f) {
    scale = (int32_t)(info->scale_factor + 0.5f);
  }
  return scale > 0 ? scale : 1;
}

static void fill_fallback_output_info(PlexyOutputInfo *info,
                                      uint32_t output_id) {
  if (!info) {
    return;
  }

  memset(info, 0, sizeof(*info));
  info->output_id = output_id;
  info->subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;
  info->transform = WL_OUTPUT_TRANSFORM_NORMAL;
  info->scale_factor = 1.0f;
  info->physical_width_mm = 600;
  info->physical_height_mm = 340;
  strncpy(info->make, "PlexyShell", sizeof(info->make) - 1);
  snprintf(info->model, sizeof(info->model), "Virtual-%u", output_id);

  if (bridge && bridge->plexy_conn) {
    plexy_get_screen_size(bridge->plexy_conn, &info->pixel_width,
                          &info->pixel_height);
    info->scale_factor = plexy_get_ui_scale(bridge->plexy_conn);
  }

  if (info->pixel_width == 0 || info->pixel_height == 0) {
    info->pixel_width = 1920;
    info->pixel_height = 1080;
  }
  if (info->scale_factor < 1.0f) {
    info->scale_factor = 1.0f;
  }
}

bool bridge_get_output_info(uint32_t output_id, PlexyOutputInfo *out_info) {
  if (!out_info) {
    return false;
  }

  if (bridge && bridge->plexy_conn) {
    const uint32_t count = plexy_get_output_count(bridge->plexy_conn);
    for (uint32_t i = 0; i < count; ++i) {
      const PlexyOutputInfo *info =
          plexy_get_output_info(bridge->plexy_conn, i);
      if (info && info->output_id == output_id) {
        *out_info = *info;
        return true;
      }
    }
    if (count == 0) {
      fill_fallback_output_info(out_info, output_id);
      return true;
    }
  }

  fill_fallback_output_info(out_info, output_id);
  return true;
}

bool bridge_get_default_output_info(PlexyOutputInfo *out_info) {
  if (!out_info) {
    return false;
  }

  if (bridge && bridge->plexy_conn &&
      plexy_get_output_count(bridge->plexy_conn) > 0) {
    const PlexyOutputInfo *info = plexy_get_output_info(bridge->plexy_conn, 0);
    if (info) {
      *out_info = *info;
      return true;
    }
  }

  fill_fallback_output_info(out_info, 1);
  return true;
}

bool bridge_wl_output_resource_get_info(struct wl_resource *output_resource,
                                        PlexyOutputInfo *out_info) {
  if (!output_resource || !out_info) {
    return false;
  }

  struct output_resource *output_res =
      wl_resource_get_user_data(output_resource);
  if (output_res && bridge_get_output_info(output_res->output_id, out_info)) {
    return true;
  }

  return bridge_get_default_output_info(out_info);
}

bool bridge_surface_get_output_info(struct bridge_surface *surface,
                                    PlexyOutputInfo *out_info) {
  if (!out_info) {
    return false;
  }

  if (surface && surface->current_output_id &&
      bridge_get_output_info(surface->current_output_id, out_info)) {
    return true;
  }

  if (surface) {
    for (uint32_t i = surface->entered_output_count; i > 0; --i) {
      if (bridge_get_output_info(surface->entered_output_ids[i - 1],
                                 out_info)) {
        return true;
      }
    }
  }

  return bridge_get_default_output_info(out_info);
}

float bridge_output_scale_factor(const PlexyOutputInfo *info) {
  if (info && info->scale_factor >= 1.0f) {
    return info->scale_factor;
  }
  return 1.0f;
}

float bridge_surface_scale_factor(struct bridge_surface *surface) {
  PlexyOutputInfo info;
  if (bridge_surface_get_output_info(surface, &info)) {
    return bridge_output_scale_factor(&info);
  }
  return 1.0f;
}

uint32_t bridge_physical_to_logical_extent(uint32_t physical_extent,
                                           float scale) {
  if (physical_extent == 0 || scale <= 1.0f) {
    return physical_extent;
  }

  uint32_t logical = (uint32_t)((float)physical_extent / scale + 0.5f);
  return logical > 0 ? logical : 1;
}

void bridge_physical_to_surface_size(struct bridge_surface *surface,
                                     uint32_t physical_width,
                                     uint32_t physical_height,
                                     uint32_t *logical_width,
                                     uint32_t *logical_height) {
  if (logical_width) {
    *logical_width = physical_width;
  }
  if (logical_height) {
    *logical_height = physical_height;
  }
  if (!surface || surface->is_x11) {
    return;
  }

  const float scale = bridge_surface_scale_factor(surface);
  if (logical_width) {
    *logical_width = bridge_physical_to_logical_extent(physical_width, scale);
  }
  if (logical_height) {
    *logical_height = bridge_physical_to_logical_extent(physical_height, scale);
  }
}

static void output_resource_destroy(struct wl_resource *resource) {
  struct output_resource *output_res = wl_resource_get_user_data(resource);
  if (output_res) {
    wl_list_remove(&output_res->link);
    free(output_res);
  }
}

static void output_release(struct wl_client *client,
                           struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static const struct wl_output_interface output_impl = {
    .release = output_release,
};

static void send_output_info_to_resource(struct output_resource *output_res,
                                         const PlexyOutputInfo *info) {
  if (!output_res || !output_res->resource || !info) {
    return;
  }

  uint32_t width = info->pixel_width;
  uint32_t height = info->pixel_height;
  if (width == 0 || height == 0) {
    width = 1920;
    height = 1080;
    LOG_WARN("wl_output: output %u reported 0x0, using fallback 1920x1080",
             info->output_id);
  }

  const int32_t int_scale = output_int_scale(info);
  const int32_t physical_width_mm = info->physical_width_mm > 0
                                        ? (int32_t)info->physical_width_mm
                                        : (int32_t)((width * 254) / 960);
  const int32_t physical_height_mm = info->physical_height_mm > 0
                                         ? (int32_t)info->physical_height_mm
                                         : (int32_t)((height * 254) / 960);

  char output_name[64];
  char description[160];
  snprintf(output_name, sizeof(output_name), "PLEXY-%u", info->output_id);
  snprintf(description, sizeof(description), "%s %s",
           info->make[0] ? info->make : "PlexyShell",
           info->model[0] ? info->model : output_name);

  const uint32_t version = wl_resource_get_version(output_res->resource);
  wl_output_send_geometry(
      output_res->resource, info->x, info->y, physical_width_mm,
      physical_height_mm,
      info->subpixel ? (int32_t)info->subpixel : WL_OUTPUT_SUBPIXEL_UNKNOWN,
      info->make[0] ? info->make : "PlexyShell",
      info->model[0] ? info->model : "Virtual Output", info->transform);

  wl_output_send_mode(output_res->resource,
                      WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED, width,
                      height, 60000);

  if (version >= WL_OUTPUT_SCALE_SINCE_VERSION) {
    wl_output_send_scale(output_res->resource, int_scale);
  }
  if (version >= WL_OUTPUT_NAME_SINCE_VERSION) {
    wl_output_send_name(output_res->resource, output_name);
  }
  if (version >= WL_OUTPUT_DESCRIPTION_SINCE_VERSION) {
    wl_output_send_description(output_res->resource, description);
  }
  if (version >= WL_OUTPUT_DONE_SINCE_VERSION) {
    wl_output_send_done(output_res->resource);
  }

  wl_client_flush(output_res->client);
}

static size_t get_desired_output_ids(uint32_t **out_ids) {
  if (!out_ids) {
    return 0;
  }

  *out_ids = NULL;
  if (bridge && bridge->plexy_conn) {
    const uint32_t count = plexy_get_output_count(bridge->plexy_conn);
    if (count > 0) {
      uint32_t *ids = calloc(count, sizeof(*ids));
      if (!ids) {
        return 0;
      }
      size_t actual = 0;
      for (uint32_t i = 0; i < count; ++i) {
        const PlexyOutputInfo *info =
            plexy_get_output_info(bridge->plexy_conn, i);
        if (!info) {
          continue;
        }
        ids[actual++] = info->output_id;
      }
      if (actual == 0) {
        free(ids);
      } else {
        *out_ids = ids;
        return actual;
      }
    }
  }

  uint32_t *ids = calloc(1, sizeof(*ids));
  if (!ids) {
    return 0;
  }
  ids[0] = 1;
  *out_ids = ids;
  return 1;
}

static bool output_globals_match(const uint32_t *desired_ids,
                                 size_t desired_count) {
  if (!output_globals_initialized) {
    return false;
  }

  size_t existing_count = 0;
  struct output_global_entry *entry = NULL;
  wl_list_for_each(entry, &output_globals, link) {
    if (existing_count >= desired_count ||
        entry->output_id != desired_ids[existing_count]) {
      return false;
    }
    existing_count++;
  }

  return existing_count == desired_count;
}

static void destroy_output_globals(void) {
  if (!output_globals_initialized) {
    if (bridge) {
      bridge->output_global = NULL;
    }
    return;
  }

  struct output_global_entry *entry = NULL;
  struct output_global_entry *tmp = NULL;
  wl_list_for_each_safe(entry, tmp, &output_globals, link) {
    wl_list_remove(&entry->link);
    if (entry->global) {
      wl_global_destroy(entry->global);
    }
    free(entry);
  }
  if (bridge) {
    bridge->output_global = NULL;
  }
}

bool bridge_create_output_globals(void) {
  if (!bridge || !bridge->display) {
    return false;
  }

  ensure_output_lists_initialized();

  uint32_t *desired_ids = NULL;
  const size_t desired_count = get_desired_output_ids(&desired_ids);
  if (desired_count == 0 || !desired_ids) {
    return false;
  }

  if (output_globals_match(desired_ids, desired_count)) {
    free(desired_ids);
    return true;
  }

  destroy_output_globals();

  for (size_t i = 0; i < desired_count; ++i) {
    struct output_global_entry *entry = calloc(1, sizeof(*entry));
    if (!entry) {
      destroy_output_globals();
      free(desired_ids);
      return false;
    }

    entry->output_id = desired_ids[i];
    entry->global = wl_global_create(bridge->display, &wl_output_interface, 4,
                                     entry, bind_output);
    if (!entry->global) {
      free(entry);
      destroy_output_globals();
      free(desired_ids);
      return false;
    }

    wl_list_insert(output_globals.prev, &entry->link);
    if (!bridge->output_global) {
      bridge->output_global = entry->global;
    }
    bridge_log("  wl_output v4 (output=%u): %p", entry->output_id,
               (void *)entry->global);
  }

  free(desired_ids);
  return bridge->output_global != NULL;
}

void bind_output(struct wl_client *client, void *data, uint32_t version,
                 uint32_t id) {
  ensure_output_lists_initialized();

  struct wl_resource *resource =
      wl_resource_create(client, &wl_output_interface, version, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  const struct output_global_entry *global_entry = data;
  struct output_resource *output_res = calloc(1, sizeof(*output_res));
  if (output_res) {
    output_res->resource = resource;
    output_res->client = client;
    output_res->output_id = global_entry ? global_entry->output_id : 1;
    wl_list_insert(&output_resources, &output_res->link);
  }

  wl_resource_set_implementation(resource, &output_impl, output_res,
                                 output_resource_destroy);

  PlexyOutputInfo info;
  if (!bridge_get_output_info(output_res ? output_res->output_id : 1, &info)) {
    fill_fallback_output_info(&info, output_res ? output_res->output_id : 1);
  }

  LOG_DEBUG("wl_output: bind version=%u output=%u (%ux%u @ %d,%d scale=%.2f)",
            version, info.output_id, info.pixel_width, info.pixel_height,
            info.x, info.y, info.scale_factor);
  send_output_info_to_resource(output_res, &info);
}

void bridge_refresh_outputs(void) {
  if (!bridge || !bridge->display) {
    return;
  }

  if (!bridge_create_output_globals()) {
    return;
  }
  if (!output_resources_initialized) {
    return;
  }

  struct output_resource *output_res;
  wl_list_for_each(output_res, &output_resources, link) {
    PlexyOutputInfo info;
    if (bridge_get_output_info(output_res->output_id, &info)) {
      send_output_info_to_resource(output_res, &info);
    }
  }

  bridge_refresh_xdg_outputs();
}
