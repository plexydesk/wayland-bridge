/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#define _POSIX_C_SOURCE 200809L
#include "presentation-time-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>
#include <time.h>

struct presentation_feedback {
  struct wl_resource *resource;
  struct bridge_surface *surface;
  struct wl_list link;
};

static void
presentation_feedback_resource_destroy(struct wl_resource *resource) {
  struct presentation_feedback *fb = wl_resource_get_user_data(resource);
  if (fb) {
    wl_list_remove(&fb->link);
    free(fb);
  }
}

static void presentation_destroy(struct wl_client *client,
                                 struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void presentation_feedback_request(struct wl_client *client,
                                          struct wl_resource *resource,
                                          struct wl_resource *surface_resource,
                                          uint32_t callback) {
  struct bridge_surface *surface = wl_resource_get_user_data(surface_resource);

  struct wl_resource *fb_resource =
      wl_resource_create(client, &wp_presentation_feedback_interface,
                         wl_resource_get_version(resource), callback);

  if (!fb_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct presentation_feedback *fb = calloc(1, sizeof(*fb));
  if (!fb) {
    wl_resource_destroy(fb_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  fb->resource = fb_resource;
  fb->surface = surface;
  wl_list_init(&fb->link);

  wl_resource_set_implementation(fb_resource, NULL, fb,
                                 presentation_feedback_resource_destroy);

  wl_list_insert(&surface->presentation_feedbacks, &fb->link);
}

void bridge_surface_clear_presentation_feedbacks(
    struct bridge_surface *surface) {
  struct presentation_feedback *fb, *tmp;
  wl_list_for_each_safe(fb, tmp, &surface->presentation_feedbacks, link) {

    wl_list_remove(&fb->link);
    wl_list_init(&fb->link);
    fb->surface = NULL;
  }
}

void bridge_surface_send_presentation_feedback(struct bridge_surface *surface) {
  if (wl_list_empty(&surface->presentation_feedbacks))
    return;

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  uint32_t tv_sec_hi = (uint32_t)(ts.tv_sec >> 32);
  uint32_t tv_sec_lo = (uint32_t)(ts.tv_sec & 0xFFFFFFFF);
  uint32_t tv_nsec = (uint32_t)ts.tv_nsec;
  uint32_t refresh = 16666666;
  uint32_t seq_hi = 0;
  uint32_t seq_lo = 0;
  uint32_t flags = WP_PRESENTATION_FEEDBACK_KIND_VSYNC;

  struct presentation_feedback *fb, *tmp;
  wl_list_for_each_safe(fb, tmp, &surface->presentation_feedbacks, link) {
    wp_presentation_feedback_send_presented(fb->resource, tv_sec_hi, tv_sec_lo,
                                            tv_nsec, refresh, seq_hi, seq_lo,
                                            flags);
    wl_resource_destroy(fb->resource);
  }
}

static const struct wp_presentation_interface presentation_impl = {
    .destroy = presentation_destroy,
    .feedback = presentation_feedback_request,
};

void bind_presentation(struct wl_client *client, void *data, uint32_t version,
                       uint32_t id) {
  struct wl_resource *resource =
      wl_resource_create(client, &wp_presentation_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &presentation_impl, NULL, NULL);

  wp_presentation_send_clock_id(resource, CLOCK_MONOTONIC);
}
