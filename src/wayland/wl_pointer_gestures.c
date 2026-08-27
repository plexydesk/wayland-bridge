/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#define _POSIX_C_SOURCE 200809L
#include "pointer-gestures-unstable-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>
#include <sys/time.h>

static uint32_t get_timestamp_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

struct gesture_swipe {
  struct wl_resource *resource;
  struct wl_resource *pointer;
  struct wl_list link;
  bool active;
};

struct gesture_pinch {
  struct wl_resource *resource;
  struct wl_resource *pointer;
  struct wl_list link;
  bool active;
};

struct gesture_hold {
  struct wl_resource *resource;
  struct wl_resource *pointer;
  struct wl_list link;
  bool active;
};

static struct wl_list swipe_gestures;
static struct wl_list pinch_gestures;
static struct wl_list hold_gestures;
static bool gestures_initialized = false;

static void ensure_gestures_init(void) {
  if (gestures_initialized)
    return;
  wl_list_init(&swipe_gestures);
  wl_list_init(&pinch_gestures);
  wl_list_init(&hold_gestures);
  gestures_initialized = true;
}

static void swipe_destroy_resource(struct wl_resource *resource) {
  struct gesture_swipe *gs = wl_resource_get_user_data(resource);
  if (!gs)
    return;
  wl_list_remove(&gs->link);
  free(gs);
}

static void swipe_destroy(struct wl_client *client,
                          struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct zwp_pointer_gesture_swipe_v1_interface swipe_impl = {
    .destroy = swipe_destroy,
};

static void pinch_destroy_resource(struct wl_resource *resource) {
  struct gesture_pinch *gp = wl_resource_get_user_data(resource);
  if (!gp)
    return;
  wl_list_remove(&gp->link);
  free(gp);
}

static void pinch_destroy(struct wl_client *client,
                          struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct zwp_pointer_gesture_pinch_v1_interface pinch_impl = {
    .destroy = pinch_destroy,
};

static void hold_destroy_resource(struct wl_resource *resource) {
  struct gesture_hold *gh = wl_resource_get_user_data(resource);
  if (!gh)
    return;
  wl_list_remove(&gh->link);
  free(gh);
}

static void hold_destroy(struct wl_client *client,
                         struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct zwp_pointer_gesture_hold_v1_interface hold_impl = {
    .destroy = hold_destroy,
};

static void gesture_manager_get_swipe_gesture(struct wl_client *client,
                                              struct wl_resource *resource,
                                              uint32_t id,
                                              struct wl_resource *pointer) {
  struct wl_resource *res =
      wl_resource_create(client, &zwp_pointer_gesture_swipe_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!res) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct gesture_swipe *gs = calloc(1, sizeof(*gs));
  if (!gs) {
    wl_resource_destroy(res);
    wl_resource_post_no_memory(resource);
    return;
  }

  gs->resource = res;
  gs->pointer = pointer;
  wl_list_insert(&swipe_gestures, &gs->link);
  wl_resource_set_implementation(res, &swipe_impl, gs, swipe_destroy_resource);
}

static void gesture_manager_get_pinch_gesture(struct wl_client *client,
                                              struct wl_resource *resource,
                                              uint32_t id,
                                              struct wl_resource *pointer) {
  struct wl_resource *res =
      wl_resource_create(client, &zwp_pointer_gesture_pinch_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!res) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct gesture_pinch *gp = calloc(1, sizeof(*gp));
  if (!gp) {
    wl_resource_destroy(res);
    wl_resource_post_no_memory(resource);
    return;
  }

  gp->resource = res;
  gp->pointer = pointer;
  wl_list_insert(&pinch_gestures, &gp->link);
  wl_resource_set_implementation(res, &pinch_impl, gp, pinch_destroy_resource);
}

static void gesture_manager_get_hold_gesture(struct wl_client *client,
                                             struct wl_resource *resource,
                                             uint32_t id,
                                             struct wl_resource *pointer) {
  struct wl_resource *res =
      wl_resource_create(client, &zwp_pointer_gesture_hold_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!res) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct gesture_hold *gh = calloc(1, sizeof(*gh));
  if (!gh) {
    wl_resource_destroy(res);
    wl_resource_post_no_memory(resource);
    return;
  }

  gh->resource = res;
  gh->pointer = pointer;
  wl_list_insert(&hold_gestures, &gh->link);
  wl_resource_set_implementation(res, &hold_impl, gh, hold_destroy_resource);
}

static void gesture_manager_release(struct wl_client *client,
                                    struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct zwp_pointer_gestures_v1_interface pointer_gestures_impl = {
    .get_swipe_gesture = gesture_manager_get_swipe_gesture,
    .get_pinch_gesture = gesture_manager_get_pinch_gesture,
    .release = gesture_manager_release,
    .get_hold_gesture = gesture_manager_get_hold_gesture,
};

void bind_pointer_gestures(struct wl_client *client, void *data,
                           uint32_t version, uint32_t id) {
  (void)data;
  ensure_gestures_init();

  struct wl_resource *resource = wl_resource_create(
      client, &zwp_pointer_gestures_v1_interface, version, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &pointer_gestures_impl, NULL, NULL);
}

static struct wl_resource *pointer_for_surface(struct bridge_surface *surface) {
  if (!surface)
    return NULL;
  return bridge_get_pointer_for_surface(surface);
}

void bridge_pointer_gesture_swipe_begin(struct bridge_surface *surface,
                                        uint32_t fingers) {
  struct wl_resource *pointer = pointer_for_surface(surface);
  if (!pointer || !surface || !surface->resource)
    return;

  uint32_t serial = wl_display_next_serial(bridge->display);
  uint32_t time = get_timestamp_ms();

  struct gesture_swipe *gs;
  wl_list_for_each(gs, &swipe_gestures, link) {
    if (gs->pointer != pointer || gs->active)
      continue;
    zwp_pointer_gesture_swipe_v1_send_begin(gs->resource, serial, time,
                                            surface->resource, fingers);
    gs->active = true;
  }
}

void bridge_pointer_gesture_swipe_update(struct bridge_surface *surface,
                                         int32_t dx, int32_t dy) {
  struct wl_resource *pointer = pointer_for_surface(surface);
  if (!pointer)
    return;

  uint32_t time = get_timestamp_ms();
  struct gesture_swipe *gs;
  wl_list_for_each(gs, &swipe_gestures, link) {
    if (gs->pointer != pointer || !gs->active)
      continue;
    zwp_pointer_gesture_swipe_v1_send_update(
        gs->resource, time, wl_fixed_from_int(dx), wl_fixed_from_int(dy));
  }
}

void bridge_pointer_gesture_swipe_end(struct bridge_surface *surface,
                                      bool cancelled) {
  struct wl_resource *pointer = pointer_for_surface(surface);
  if (!pointer)
    return;

  uint32_t serial = wl_display_next_serial(bridge->display);
  uint32_t time = get_timestamp_ms();

  struct gesture_swipe *gs;
  wl_list_for_each(gs, &swipe_gestures, link) {
    if (gs->pointer != pointer || !gs->active)
      continue;
    zwp_pointer_gesture_swipe_v1_send_end(gs->resource, serial, time,
                                          cancelled ? 1 : 0);
    gs->active = false;
  }
}

void bridge_pointer_gesture_pinch_begin(struct bridge_surface *surface,
                                        uint32_t fingers) {
  struct wl_resource *pointer = pointer_for_surface(surface);
  if (!pointer || !surface || !surface->resource)
    return;

  uint32_t serial = wl_display_next_serial(bridge->display);
  uint32_t time = get_timestamp_ms();

  struct gesture_pinch *gp;
  wl_list_for_each(gp, &pinch_gestures, link) {
    if (gp->pointer != pointer || gp->active)
      continue;
    zwp_pointer_gesture_pinch_v1_send_begin(gp->resource, serial, time,
                                            surface->resource, fingers);
    gp->active = true;
  }
}

void bridge_pointer_gesture_pinch_update(struct bridge_surface *surface,
                                         int32_t dx, int32_t dy, double scale,
                                         double rotation) {
  struct wl_resource *pointer = pointer_for_surface(surface);
  if (!pointer)
    return;

  uint32_t time = get_timestamp_ms();
  struct gesture_pinch *gp;
  wl_list_for_each(gp, &pinch_gestures, link) {
    if (gp->pointer != pointer || !gp->active)
      continue;
    zwp_pointer_gesture_pinch_v1_send_update(
        gp->resource, time, wl_fixed_from_int(dx), wl_fixed_from_int(dy),
        wl_fixed_from_double(scale), wl_fixed_from_double(rotation));
  }
}

void bridge_pointer_gesture_pinch_end(struct bridge_surface *surface,
                                      bool cancelled) {
  struct wl_resource *pointer = pointer_for_surface(surface);
  if (!pointer)
    return;

  uint32_t serial = wl_display_next_serial(bridge->display);
  uint32_t time = get_timestamp_ms();

  struct gesture_pinch *gp;
  wl_list_for_each(gp, &pinch_gestures, link) {
    if (gp->pointer != pointer || !gp->active)
      continue;
    zwp_pointer_gesture_pinch_v1_send_end(gp->resource, serial, time,
                                          cancelled ? 1 : 0);
    gp->active = false;
  }
}

void bridge_pointer_gesture_hold_begin(struct bridge_surface *surface,
                                       uint32_t fingers) {
  struct wl_resource *pointer = pointer_for_surface(surface);
  if (!pointer || !surface || !surface->resource)
    return;

  uint32_t serial = wl_display_next_serial(bridge->display);
  uint32_t time = get_timestamp_ms();

  struct gesture_hold *gh;
  wl_list_for_each(gh, &hold_gestures, link) {
    if (gh->pointer != pointer || gh->active)
      continue;
    zwp_pointer_gesture_hold_v1_send_begin(gh->resource, serial, time,
                                           surface->resource, fingers);
    gh->active = true;
  }
}

void bridge_pointer_gesture_hold_end(struct bridge_surface *surface,
                                     bool cancelled) {
  struct wl_resource *pointer = pointer_for_surface(surface);
  if (!pointer)
    return;

  uint32_t serial = wl_display_next_serial(bridge->display);
  uint32_t time = get_timestamp_ms();

  struct gesture_hold *gh;
  wl_list_for_each(gh, &hold_gestures, link) {
    if (gh->pointer != pointer || !gh->active)
      continue;
    zwp_pointer_gesture_hold_v1_send_end(gh->resource, serial, time,
                                         cancelled ? 1 : 0);
    gh->active = false;
  }
}
