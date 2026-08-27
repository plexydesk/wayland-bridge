/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef XWAYLAND_KEYBOARD_GRAB_UNSTABLE_V1_SERVER_PROTOCOL_H
#define XWAYLAND_KEYBOARD_GRAB_UNSTABLE_V1_SERVER_PROTOCOL_H

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
struct zwp_xwayland_keyboard_grab_manager_v1;
struct zwp_xwayland_keyboard_grab_v1;

#ifndef ZWP_XWAYLAND_KEYBOARD_GRAB_MANAGER_V1_INTERFACE
#define ZWP_XWAYLAND_KEYBOARD_GRAB_MANAGER_V1_INTERFACE

extern const struct wl_interface
    zwp_xwayland_keyboard_grab_manager_v1_interface;
#endif
#ifndef ZWP_XWAYLAND_KEYBOARD_GRAB_V1_INTERFACE
#define ZWP_XWAYLAND_KEYBOARD_GRAB_V1_INTERFACE

extern const struct wl_interface zwp_xwayland_keyboard_grab_v1_interface;
#endif

struct zwp_xwayland_keyboard_grab_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*grab_keyboard)(struct wl_client *client, struct wl_resource *resource,
                        uint32_t id, struct wl_resource *surface,
                        struct wl_resource *seat);
};

#define ZWP_XWAYLAND_KEYBOARD_GRAB_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define ZWP_XWAYLAND_KEYBOARD_GRAB_MANAGER_V1_GRAB_KEYBOARD_SINCE_VERSION 1

struct zwp_xwayland_keyboard_grab_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define ZWP_XWAYLAND_KEYBOARD_GRAB_V1_DESTROY_SINCE_VERSION 1

#ifdef __cplusplus
}
#endif

#endif
