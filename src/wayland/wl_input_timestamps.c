/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */


#define _GNU_SOURCE
#include "input-timestamps-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>
#include <time.h>

struct input_timestamps {
  struct wl_resource *resource;
  struct wl_resource *input_resource;
  struct wl_list link;
};

static struct wl_list timestamp_subscriptions;
static bool ts_initialized = false;

static void ensure_ts_init(void) {
  if (!ts_initialized) {
    wl_list_init(&timestamp_subscriptions);
    ts_initialized = true;
  }
}

void bridge_input_timestamps_send(struct wl_resource *input_resource) {
  if (!ts_initialized)
    return;

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint32_t tv_sec_hi = (uint32_t)((uint64_t)ts.tv_sec >> 32);
  uint32_t tv_sec_lo = (uint32_t)(ts.tv_sec & 0xFFFFFFFF);
  uint32_t tv_nsec = (uint32_t)ts.tv_nsec;

  struct input_timestamps *sub;
  wl_list_for_each(sub, &timestamp_subscriptions, link) {
    if (sub->input_resource == input_resource)
      zwp_input_timestamps_v1_send_timestamp(sub->resource, tv_sec_hi,
                                             tv_sec_lo, tv_nsec);
  }
}

static void timestamps_destroy(struct wl_client *client,
                               struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct zwp_input_timestamps_v1_interface timestamps_impl = {
    .destroy = timestamps_destroy,
};

static void timestamps_resource_destroy(struct wl_resource *resource) {
  struct input_timestamps *sub = wl_resource_get_user_data(resource);
  if (sub) {
    wl_list_remove(&sub->link);
    free(sub);
  }
}

static void create_timestamps(struct wl_client *client,
                              struct wl_resource *resource, uint32_t id,
                              struct wl_resource *input_resource) {
  ensure_ts_init();

  struct wl_resource *ts_res =
      wl_resource_create(client, &zwp_input_timestamps_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!ts_res) {
    wl_client_post_no_memory(client);
    return;
  }

  struct input_timestamps *sub = calloc(1, sizeof(*sub));
  if (!sub) {
    wl_resource_destroy(ts_res);
    wl_resource_post_no_memory(resource);
    return;
  }

  sub->resource = ts_res;
  sub->input_resource = input_resource;
  wl_list_insert(&timestamp_subscriptions, &sub->link);

  wl_resource_set_implementation(ts_res, &timestamps_impl, sub,
                                 timestamps_resource_destroy);
}

static void manager_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void manager_get_keyboard_timestamps(struct wl_client *client,
                                            struct wl_resource *resource,
                                            uint32_t id,
                                            struct wl_resource *keyboard) {
  create_timestamps(client, resource, id, keyboard);
}

static void manager_get_pointer_timestamps(struct wl_client *client,
                                           struct wl_resource *resource,
                                           uint32_t id,
                                           struct wl_resource *pointer) {
  create_timestamps(client, resource, id, pointer);
}

static void manager_get_touch_timestamps(struct wl_client *client,
                                         struct wl_resource *resource,
                                         uint32_t id,
                                         struct wl_resource *touch) {
  create_timestamps(client, resource, id, touch);
}

static const struct zwp_input_timestamps_manager_v1_interface manager_impl = {
    .destroy = manager_destroy,
    .get_keyboard_timestamps = manager_get_keyboard_timestamps,
    .get_pointer_timestamps = manager_get_pointer_timestamps,
    .get_touch_timestamps = manager_get_touch_timestamps,
};

void bind_input_timestamps_manager(struct wl_client *client, void *data,
                                   uint32_t version, uint32_t id) {
  (void)data;
  struct wl_resource *resource = wl_resource_create(
      client, &zwp_input_timestamps_manager_v1_interface, version, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}
