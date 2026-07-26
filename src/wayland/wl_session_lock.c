/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */


#define _GNU_SOURCE
#include "ext-session-lock-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>

struct session_lock {
  struct wl_resource *resource;
  bool locked;
};

struct lock_surface {
  struct wl_resource *resource;
  struct wl_resource *wl_surface;
  struct wl_resource *output;
  bool configured;
};

static void lock_surface_destroy(struct wl_client *client,
                                 struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void lock_surface_ack_configure(struct wl_client *client,
                                       struct wl_resource *resource,
                                       uint32_t serial) {
  (void)client;
  (void)serial;
  struct lock_surface *ls = wl_resource_get_user_data(resource);
  if (ls)
    ls->configured = true;
}

static const struct ext_session_lock_surface_v1_interface lock_surface_impl = {
    .destroy = lock_surface_destroy,
    .ack_configure = lock_surface_ack_configure,
};

static void lock_surface_resource_destroy(struct wl_resource *resource) {
  struct lock_surface *ls = wl_resource_get_user_data(resource);
  free(ls);
}

static void lock_destroy(struct wl_client *client,
                         struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void lock_get_lock_surface(struct wl_client *client,
                                  struct wl_resource *resource, uint32_t id,
                                  struct wl_resource *surface,
                                  struct wl_resource *output) {
  struct wl_resource *ls_res =
      wl_resource_create(client, &ext_session_lock_surface_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!ls_res) {
    wl_client_post_no_memory(client);
    return;
  }

  struct lock_surface *ls = calloc(1, sizeof(*ls));
  if (!ls) {
    wl_resource_destroy(ls_res);
    wl_resource_post_no_memory(resource);
    return;
  }

  ls->resource = ls_res;
  ls->wl_surface = surface;
  ls->output = output;
  ls->configured = false;

  wl_resource_set_implementation(ls_res, &lock_surface_impl, ls,
                                 lock_surface_resource_destroy);

  uint32_t w = 1920, h = 1080;
  if (bridge && bridge->plexy_conn)
    plexy_get_screen_size(bridge->plexy_conn, &w, &h);

  uint32_t serial = wl_display_next_serial(bridge->display);
  ext_session_lock_surface_v1_send_configure(ls_res, serial, w, h);
}

static void lock_unlock_and_destroy(struct wl_client *client,
                                    struct wl_resource *resource) {
  (void)client;
  struct session_lock *lock = wl_resource_get_user_data(resource);
  if (lock)
    lock->locked = false;
  if (bridge) {
    bridge->session_locked = false;
    plexy_session_unlock(bridge->plexy_conn);
  }
  wl_resource_destroy(resource);
}

static const struct ext_session_lock_v1_interface session_lock_impl = {
    .destroy = lock_destroy,
    .get_lock_surface = lock_get_lock_surface,
    .unlock_and_destroy = lock_unlock_and_destroy,
};

static void session_lock_resource_destroy(struct wl_resource *resource) {
  struct session_lock *lock = wl_resource_get_user_data(resource);
  if (lock && lock->locked && bridge) {
    bridge->session_locked = false;
    plexy_session_unlock(bridge->plexy_conn);
  }
  free(lock);
}

static void manager_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void manager_lock(struct wl_client *client, struct wl_resource *resource,
                         uint32_t id) {
  struct wl_resource *lock_res =
      wl_resource_create(client, &ext_session_lock_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!lock_res) {
    wl_client_post_no_memory(client);
    return;
  }

  struct session_lock *lock = calloc(1, sizeof(*lock));
  if (!lock) {
    wl_resource_destroy(lock_res);
    wl_resource_post_no_memory(resource);
    return;
  }

  lock->resource = lock_res;
  lock->locked = true;
  wl_resource_set_implementation(lock_res, &session_lock_impl, lock,
                                 session_lock_resource_destroy);

  if (bridge) {
    bridge->session_locked = true;
    plexy_session_lock(bridge->plexy_conn);
  }

  ext_session_lock_v1_send_locked(lock_res);
}

static const struct ext_session_lock_manager_v1_interface
    session_lock_manager_impl = {
        .destroy = manager_destroy,
        .lock = manager_lock,
};

void bind_session_lock_manager(struct wl_client *client, void *data,
                               uint32_t version, uint32_t id) {
  (void)data;
  struct wl_resource *resource = wl_resource_create(
      client, &ext_session_lock_manager_v1_interface, version, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &session_lock_manager_impl, NULL,
                                 NULL);
}
