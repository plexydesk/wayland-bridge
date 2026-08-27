/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef KEYBOARD_SHORTCUTS_INHIBIT_UNSTABLE_V1_SERVER_PROTOCOL_H
#define KEYBOARD_SHORTCUTS_INHIBIT_UNSTABLE_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_seat;
struct wl_surface;
struct zwp_keyboard_shortcuts_inhibit_manager_v1;
struct zwp_keyboard_shortcuts_inhibitor_v1;

#ifndef ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_V1_INTERFACE
#define ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_V1_INTERFACE

extern const struct wl_interface
    zwp_keyboard_shortcuts_inhibit_manager_v1_interface;
#endif
#ifndef ZWP_KEYBOARD_SHORTCUTS_INHIBITOR_V1_INTERFACE
#define ZWP_KEYBOARD_SHORTCUTS_INHIBITOR_V1_INTERFACE

extern const struct wl_interface zwp_keyboard_shortcuts_inhibitor_v1_interface;
#endif

#ifndef ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_V1_ERROR_ENUM
#define ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_V1_ERROR_ENUM
enum zwp_keyboard_shortcuts_inhibit_manager_v1_error {

  ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_V1_ERROR_ALREADY_INHIBITED = 0,
};
#endif

#ifndef ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_V1_ERROR_ENUM_IS_VALID
#define ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_V1_ERROR_ENUM_IS_VALID

static inline bool
zwp_keyboard_shortcuts_inhibit_manager_v1_error_is_valid(uint32_t value,
                                                         uint32_t version) {
  switch (value) {
  case ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_V1_ERROR_ALREADY_INHIBITED:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct zwp_keyboard_shortcuts_inhibit_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*inhibit_shortcuts)(struct wl_client *client,
                            struct wl_resource *resource, uint32_t id,
                            struct wl_resource *surface,
                            struct wl_resource *seat);
};

#define ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_V1_INHIBIT_SHORTCUTS_SINCE_VERSION \
  1

struct zwp_keyboard_shortcuts_inhibitor_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define ZWP_KEYBOARD_SHORTCUTS_INHIBITOR_V1_ACTIVE 0
#define ZWP_KEYBOARD_SHORTCUTS_INHIBITOR_V1_INACTIVE 1

#define ZWP_KEYBOARD_SHORTCUTS_INHIBITOR_V1_ACTIVE_SINCE_VERSION 1

#define ZWP_KEYBOARD_SHORTCUTS_INHIBITOR_V1_INACTIVE_SINCE_VERSION 1

#define ZWP_KEYBOARD_SHORTCUTS_INHIBITOR_V1_DESTROY_SINCE_VERSION 1

static inline void
zwp_keyboard_shortcuts_inhibitor_v1_send_active(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, ZWP_KEYBOARD_SHORTCUTS_INHIBITOR_V1_ACTIVE);
}

static inline void zwp_keyboard_shortcuts_inhibitor_v1_send_inactive(
    struct wl_resource *resource_) {
  wl_resource_post_event(resource_,
                         ZWP_KEYBOARD_SHORTCUTS_INHIBITOR_V1_INACTIVE);
}

#ifdef __cplusplus
}
#endif

#endif
