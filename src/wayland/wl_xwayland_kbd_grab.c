/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */


#define _GNU_SOURCE
#include "wayland_bridge.h"
#include "xwayland-keyboard-grab-v1-protocol.h"
#include <stdlib.h>

struct xwayland_kbd_grab {
  struct wl_resource *resource;
  struct wl_resource *surface;
  struct wl_list link;
};

static struct wl_list active_grabs;
static bool grabs_initialized = false;

static void ensure_grabs_init(void) {
  if (!grabs_initialized) {
    wl_list_init(&active_grabs);
    grabs_initialized = true;
  }
}

bool bridge_xwayland_kbd_grab_active(struct wl_resource *surface) {
  if (!grabs_initialized || !surface)
    return false;

  struct xwayland_kbd_grab *grab;
  wl_list_for_each(grab, &active_grabs, link) {
    if (grab->surface == surface)
      return true;
  }
  return false;
}

static void grab_destroy(struct wl_client *client,
                         struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct zwp_xwayland_keyboard_grab_v1_interface grab_impl = {
    .destroy = grab_destroy,
};

static void grab_resource_destroy(struct wl_resource *resource) {
  struct xwayland_kbd_grab *grab = wl_resource_get_user_data(resource);
  if (grab) {
    wl_list_remove(&grab->link);
    free(grab);
  }
}

static void manager_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void manager_grab_keyboard(struct wl_client *client,
                                  struct wl_resource *resource, uint32_t id,
                                  struct wl_resource *surface,
                                  struct wl_resource *seat) {
  (void)seat;
  ensure_grabs_init();

  struct wl_resource *grab_res =
      wl_resource_create(client, &zwp_xwayland_keyboard_grab_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!grab_res) {
    wl_client_post_no_memory(client);
    return;
  }

  struct xwayland_kbd_grab *grab = calloc(1, sizeof(*grab));
  if (!grab) {
    wl_resource_destroy(grab_res);
    wl_resource_post_no_memory(resource);
    return;
  }

  grab->resource = grab_res;
  grab->surface = surface;
  wl_list_insert(&active_grabs, &grab->link);

  wl_resource_set_implementation(grab_res, &grab_impl, grab,
                                 grab_resource_destroy);
}

static const struct zwp_xwayland_keyboard_grab_manager_v1_interface
    manager_impl = {
        .destroy = manager_destroy,
        .grab_keyboard = manager_grab_keyboard,
};

void bind_xwayland_keyboard_grab_manager(struct wl_client *client, void *data,
                                         uint32_t version, uint32_t id) {
  (void)data;
  struct wl_resource *resource = wl_resource_create(
      client, &zwp_xwayland_keyboard_grab_manager_v1_interface, version, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}
