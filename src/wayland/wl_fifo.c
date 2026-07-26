/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */


#define _GNU_SOURCE
#include "fifo-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>

struct fifo_surface {
  struct wl_resource *resource;
  struct bridge_surface *bridge_surface;
  bool pending_barrier;
  bool pending_wait;
  bool barrier_active;
};

static void fifo_set_barrier(struct wl_client *client,
                             struct wl_resource *resource) {
  (void)client;
  struct fifo_surface *fifo = wl_resource_get_user_data(resource);
  if (fifo)
    fifo->pending_barrier = true;
}

static void fifo_wait_barrier(struct wl_client *client,
                              struct wl_resource *resource) {
  (void)client;
  struct fifo_surface *fifo = wl_resource_get_user_data(resource);
  if (fifo)
    fifo->pending_wait = true;
}

static void fifo_destroy(struct wl_client *client,
                         struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct wp_fifo_v1_interface fifo_impl = {
    .set_barrier = fifo_set_barrier,
    .wait_barrier = fifo_wait_barrier,
    .destroy = fifo_destroy,
};

static void fifo_resource_destroy(struct wl_resource *resource) {
  struct fifo_surface *fifo = wl_resource_get_user_data(resource);
  if (fifo) {
    if (fifo->bridge_surface)
      fifo->bridge_surface->fifo_surface = NULL;
    free(fifo);
  }
}

static void manager_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void manager_get_fifo(struct wl_client *client,
                             struct wl_resource *resource, uint32_t id,
                             struct wl_resource *surface) {
  struct bridge_surface *bs = bridge_surface_from_resource(surface);
  if (!bs) {
    wl_resource_post_error(resource, WP_FIFO_V1_ERROR_SURFACE_DESTROYED,
                           "wl_surface has no bridge_surface");
    return;
  }

  if (bs->fifo_surface) {
    wl_resource_post_error(resource, WP_FIFO_MANAGER_V1_ERROR_ALREADY_EXISTS,
                           "fifo already exists for this surface");
    return;
  }

  struct wl_resource *fifo_res = wl_resource_create(
      client, &wp_fifo_v1_interface, wl_resource_get_version(resource), id);
  if (!fifo_res) {
    wl_client_post_no_memory(client);
    return;
  }

  struct fifo_surface *fifo = calloc(1, sizeof(*fifo));
  if (!fifo) {
    wl_resource_destroy(fifo_res);
    wl_resource_post_no_memory(resource);
    return;
  }

  fifo->resource = fifo_res;
  fifo->bridge_surface = bs;
  bs->fifo_surface = fifo;

  wl_resource_set_implementation(fifo_res, &fifo_impl, fifo,
                                 fifo_resource_destroy);
}

static const struct wp_fifo_manager_v1_interface fifo_manager_impl = {
    .destroy = manager_destroy,
    .get_fifo = manager_get_fifo,
};

void bind_fifo_manager(struct wl_client *client, void *data, uint32_t version,
                       uint32_t id) {
  (void)data;
  struct wl_resource *resource =
      wl_resource_create(client, &wp_fifo_manager_v1_interface, version, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &fifo_manager_impl, NULL, NULL);
}

bool bridge_fifo_should_defer_frame(struct bridge_surface *surface) {
  struct fifo_surface *fifo = surface->fifo_surface;
  if (!fifo)
    return false;

  bool defer = fifo->pending_wait && fifo->barrier_active;

  if (fifo->pending_barrier)
    fifo->barrier_active = true;
  fifo->pending_barrier = false;
  fifo->pending_wait = false;

  return defer;
}

void bridge_fifo_on_frame_done(struct bridge_surface *surface) {
  struct fifo_surface *fifo = surface->fifo_surface;
  if (!fifo)
    return;
  fifo->barrier_active = false;
}
