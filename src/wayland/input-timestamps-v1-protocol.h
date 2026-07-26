/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef INPUT_TIMESTAMPS_UNSTABLE_V1_SERVER_PROTOCOL_H
#define INPUT_TIMESTAMPS_UNSTABLE_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_keyboard;
struct wl_pointer;
struct wl_touch;
struct zwp_input_timestamps_manager_v1;
struct zwp_input_timestamps_v1;

#ifndef ZWP_INPUT_TIMESTAMPS_MANAGER_V1_INTERFACE
#define ZWP_INPUT_TIMESTAMPS_MANAGER_V1_INTERFACE

extern const struct wl_interface zwp_input_timestamps_manager_v1_interface;
#endif
#ifndef ZWP_INPUT_TIMESTAMPS_V1_INTERFACE
#define ZWP_INPUT_TIMESTAMPS_V1_INTERFACE

extern const struct wl_interface zwp_input_timestamps_v1_interface;
#endif

struct zwp_input_timestamps_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_keyboard_timestamps)(struct wl_client *client,
                                  struct wl_resource *resource, uint32_t id,
                                  struct wl_resource *keyboard);

  void (*get_pointer_timestamps)(struct wl_client *client,
                                 struct wl_resource *resource, uint32_t id,
                                 struct wl_resource *pointer);

  void (*get_touch_timestamps)(struct wl_client *client,
                               struct wl_resource *resource, uint32_t id,
                               struct wl_resource *touch);
};

#define ZWP_INPUT_TIMESTAMPS_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define ZWP_INPUT_TIMESTAMPS_MANAGER_V1_GET_KEYBOARD_TIMESTAMPS_SINCE_VERSION 1

#define ZWP_INPUT_TIMESTAMPS_MANAGER_V1_GET_POINTER_TIMESTAMPS_SINCE_VERSION 1

#define ZWP_INPUT_TIMESTAMPS_MANAGER_V1_GET_TOUCH_TIMESTAMPS_SINCE_VERSION 1

struct zwp_input_timestamps_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define ZWP_INPUT_TIMESTAMPS_V1_TIMESTAMP 0

#define ZWP_INPUT_TIMESTAMPS_V1_TIMESTAMP_SINCE_VERSION 1

#define ZWP_INPUT_TIMESTAMPS_V1_DESTROY_SINCE_VERSION 1

static inline void
zwp_input_timestamps_v1_send_timestamp(struct wl_resource *resource_,
                                       uint32_t tv_sec_hi, uint32_t tv_sec_lo,
                                       uint32_t tv_nsec) {
  wl_resource_post_event(resource_, ZWP_INPUT_TIMESTAMPS_V1_TIMESTAMP,
                         tv_sec_hi, tv_sec_lo, tv_nsec);
}

#ifdef __cplusplus
}
#endif

#endif
