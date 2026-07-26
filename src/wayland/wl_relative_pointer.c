/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#define _POSIX_C_SOURCE 200809L
#include "relative-pointer-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>
#include <time.h>

struct relative_pointer {
  struct wl_resource *resource;
  struct wl_resource *pointer;
  struct wl_client *client;
  struct wl_list link;
};

static struct wl_list relative_pointers;
static bool relative_pointers_initialized = false;

static void ensure_relative_pointers_init(void) {
  if (relative_pointers_initialized) {
    return;
  }
  wl_list_init(&relative_pointers);
  relative_pointers_initialized = true;
}

static void relative_pointer_resource_destroy(struct wl_resource *resource) {
  struct relative_pointer *rp = wl_resource_get_user_data(resource);
  if (!rp) {
    return;
  }
  wl_list_remove(&rp->link);
  free(rp);
}

static void relative_pointer_destroy(struct wl_client *client,
                                     struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct zwp_relative_pointer_v1_interface relative_pointer_impl = {
    .destroy = relative_pointer_destroy,
};

static void relative_pointer_manager_destroy(struct wl_client *client,
                                             struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void relative_pointer_manager_get_relative_pointer(
    struct wl_client *client, struct wl_resource *resource, uint32_t id,
    struct wl_resource *pointer) {
  if (!pointer) {
    wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                           "get_relative_pointer requires wl_pointer resource");
    return;
  }

  struct wl_resource *rel_pointer =
      wl_resource_create(client, &zwp_relative_pointer_v1_interface,
                         wl_resource_get_version(resource), id);

  if (!rel_pointer) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct relative_pointer *rp = calloc(1, sizeof(*rp));
  if (!rp) {
    wl_resource_destroy(rel_pointer);
    wl_resource_post_no_memory(resource);
    return;
  }

  rp->resource = rel_pointer;
  rp->pointer = pointer;
  rp->client = client;
  wl_list_insert(&relative_pointers, &rp->link);

  wl_resource_set_implementation(rel_pointer, &relative_pointer_impl, rp,
                                 relative_pointer_resource_destroy);
}

static const struct zwp_relative_pointer_manager_v1_interface
    relative_pointer_manager_impl = {
        .destroy = relative_pointer_manager_destroy,
        .get_relative_pointer = relative_pointer_manager_get_relative_pointer,
};

void bridge_relative_pointer_send_motion(struct bridge_surface *surface,
                                         double dx, double dy) {
  if (!relative_pointers_initialized || !surface || !surface->resource) {
    return;
  }

  struct wl_client *client = wl_resource_get_client(surface->resource);
  if (!client) {
    return;
  }

  static int rel_count = 0;
  if (++rel_count <= 5 || rel_count % 500 == 0) {
    bridge_log("relative_motion #%d: dx=%.3f dy=%.3f surface=%p", rel_count, dx,
               dy, (void *)surface);
  }

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t usec =
      (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
  uint32_t utime_hi = (uint32_t)(usec >> 32);
  uint32_t utime_lo = (uint32_t)(usec & 0xffffffffu);

  wl_fixed_t fdx = wl_fixed_from_double(dx);
  wl_fixed_t fdy = wl_fixed_from_double(dy);

  struct relative_pointer *rp;
  wl_list_for_each(rp, &relative_pointers, link) {
    if (rp->client != client) {
      continue;
    }
    zwp_relative_pointer_v1_send_relative_motion(rp->resource, utime_hi,
                                                 utime_lo, fdx, fdy, fdx, fdy);
  }
}

void bind_relative_pointer_manager(struct wl_client *client, void *data,
                                   uint32_t version, uint32_t id) {
  (void)data;
  ensure_relative_pointers_init();

  struct wl_resource *resource = wl_resource_create(
      client, &zwp_relative_pointer_manager_v1_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &relative_pointer_manager_impl, NULL,
                                 NULL);
}
