/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef RELATIVE_POINTER_UNSTABLE_V1_SERVER_PROTOCOL_H
#define RELATIVE_POINTER_UNSTABLE_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_pointer;
struct zwp_relative_pointer_manager_v1;
struct zwp_relative_pointer_v1;

#ifndef ZWP_RELATIVE_POINTER_MANAGER_V1_INTERFACE
#define ZWP_RELATIVE_POINTER_MANAGER_V1_INTERFACE

extern const struct wl_interface zwp_relative_pointer_manager_v1_interface;
#endif
#ifndef ZWP_RELATIVE_POINTER_V1_INTERFACE
#define ZWP_RELATIVE_POINTER_V1_INTERFACE

extern const struct wl_interface zwp_relative_pointer_v1_interface;
#endif

struct zwp_relative_pointer_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_relative_pointer)(struct wl_client *client,
                               struct wl_resource *resource, uint32_t id,
                               struct wl_resource *pointer);
};

#define ZWP_RELATIVE_POINTER_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define ZWP_RELATIVE_POINTER_MANAGER_V1_GET_RELATIVE_POINTER_SINCE_VERSION 1

struct zwp_relative_pointer_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define ZWP_RELATIVE_POINTER_V1_RELATIVE_MOTION 0

#define ZWP_RELATIVE_POINTER_V1_RELATIVE_MOTION_SINCE_VERSION 1

#define ZWP_RELATIVE_POINTER_V1_DESTROY_SINCE_VERSION 1

static inline void zwp_relative_pointer_v1_send_relative_motion(
    struct wl_resource *resource_, uint32_t utime_hi, uint32_t utime_lo,
    wl_fixed_t dx, wl_fixed_t dy, wl_fixed_t dx_unaccel,
    wl_fixed_t dy_unaccel) {
  wl_resource_post_event(resource_, ZWP_RELATIVE_POINTER_V1_RELATIVE_MOTION,
                         utime_hi, utime_lo, dx, dy, dx_unaccel, dy_unaccel);
}

#ifdef __cplusplus
}
#endif

#endif
