/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef POINTER_GESTURES_UNSTABLE_V1_SERVER_PROTOCOL_H
#define POINTER_GESTURES_UNSTABLE_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_pointer;
struct wl_surface;
struct zwp_pointer_gesture_hold_v1;
struct zwp_pointer_gesture_pinch_v1;
struct zwp_pointer_gesture_swipe_v1;
struct zwp_pointer_gestures_v1;

#ifndef ZWP_POINTER_GESTURES_V1_INTERFACE
#define ZWP_POINTER_GESTURES_V1_INTERFACE

extern const struct wl_interface zwp_pointer_gestures_v1_interface;
#endif
#ifndef ZWP_POINTER_GESTURE_SWIPE_V1_INTERFACE
#define ZWP_POINTER_GESTURE_SWIPE_V1_INTERFACE

extern const struct wl_interface zwp_pointer_gesture_swipe_v1_interface;
#endif
#ifndef ZWP_POINTER_GESTURE_PINCH_V1_INTERFACE
#define ZWP_POINTER_GESTURE_PINCH_V1_INTERFACE

extern const struct wl_interface zwp_pointer_gesture_pinch_v1_interface;
#endif
#ifndef ZWP_POINTER_GESTURE_HOLD_V1_INTERFACE
#define ZWP_POINTER_GESTURE_HOLD_V1_INTERFACE

extern const struct wl_interface zwp_pointer_gesture_hold_v1_interface;
#endif

struct zwp_pointer_gestures_v1_interface {

  void (*get_swipe_gesture)(struct wl_client *client,
                            struct wl_resource *resource, uint32_t id,
                            struct wl_resource *pointer);

  void (*get_pinch_gesture)(struct wl_client *client,
                            struct wl_resource *resource, uint32_t id,
                            struct wl_resource *pointer);

  void (*release)(struct wl_client *client, struct wl_resource *resource);

  void (*get_hold_gesture)(struct wl_client *client,
                           struct wl_resource *resource, uint32_t id,
                           struct wl_resource *pointer);
};

#define ZWP_POINTER_GESTURES_V1_GET_SWIPE_GESTURE_SINCE_VERSION 1

#define ZWP_POINTER_GESTURES_V1_GET_PINCH_GESTURE_SINCE_VERSION 1

#define ZWP_POINTER_GESTURES_V1_RELEASE_SINCE_VERSION 2

#define ZWP_POINTER_GESTURES_V1_GET_HOLD_GESTURE_SINCE_VERSION 3

struct zwp_pointer_gesture_swipe_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define ZWP_POINTER_GESTURE_SWIPE_V1_BEGIN 0
#define ZWP_POINTER_GESTURE_SWIPE_V1_UPDATE 1
#define ZWP_POINTER_GESTURE_SWIPE_V1_END 2

#define ZWP_POINTER_GESTURE_SWIPE_V1_BEGIN_SINCE_VERSION 1

#define ZWP_POINTER_GESTURE_SWIPE_V1_UPDATE_SINCE_VERSION 1

#define ZWP_POINTER_GESTURE_SWIPE_V1_END_SINCE_VERSION 1

#define ZWP_POINTER_GESTURE_SWIPE_V1_DESTROY_SINCE_VERSION 1

static inline void zwp_pointer_gesture_swipe_v1_send_begin(
    struct wl_resource *resource_, uint32_t serial, uint32_t time,
    struct wl_resource *surface, uint32_t fingers) {
  wl_resource_post_event(resource_, ZWP_POINTER_GESTURE_SWIPE_V1_BEGIN, serial,
                         time, surface, fingers);
}

static inline void
zwp_pointer_gesture_swipe_v1_send_update(struct wl_resource *resource_,
                                         uint32_t time, wl_fixed_t dx,
                                         wl_fixed_t dy) {
  wl_resource_post_event(resource_, ZWP_POINTER_GESTURE_SWIPE_V1_UPDATE, time,
                         dx, dy);
}

static inline void
zwp_pointer_gesture_swipe_v1_send_end(struct wl_resource *resource_,
                                      uint32_t serial, uint32_t time,
                                      int32_t cancelled) {
  wl_resource_post_event(resource_, ZWP_POINTER_GESTURE_SWIPE_V1_END, serial,
                         time, cancelled);
}

struct zwp_pointer_gesture_pinch_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define ZWP_POINTER_GESTURE_PINCH_V1_BEGIN 0
#define ZWP_POINTER_GESTURE_PINCH_V1_UPDATE 1
#define ZWP_POINTER_GESTURE_PINCH_V1_END 2

#define ZWP_POINTER_GESTURE_PINCH_V1_BEGIN_SINCE_VERSION 1

#define ZWP_POINTER_GESTURE_PINCH_V1_UPDATE_SINCE_VERSION 1

#define ZWP_POINTER_GESTURE_PINCH_V1_END_SINCE_VERSION 1

#define ZWP_POINTER_GESTURE_PINCH_V1_DESTROY_SINCE_VERSION 1

static inline void zwp_pointer_gesture_pinch_v1_send_begin(
    struct wl_resource *resource_, uint32_t serial, uint32_t time,
    struct wl_resource *surface, uint32_t fingers) {
  wl_resource_post_event(resource_, ZWP_POINTER_GESTURE_PINCH_V1_BEGIN, serial,
                         time, surface, fingers);
}

static inline void zwp_pointer_gesture_pinch_v1_send_update(
    struct wl_resource *resource_, uint32_t time, wl_fixed_t dx, wl_fixed_t dy,
    wl_fixed_t scale, wl_fixed_t rotation) {
  wl_resource_post_event(resource_, ZWP_POINTER_GESTURE_PINCH_V1_UPDATE, time,
                         dx, dy, scale, rotation);
}

static inline void
zwp_pointer_gesture_pinch_v1_send_end(struct wl_resource *resource_,
                                      uint32_t serial, uint32_t time,
                                      int32_t cancelled) {
  wl_resource_post_event(resource_, ZWP_POINTER_GESTURE_PINCH_V1_END, serial,
                         time, cancelled);
}

struct zwp_pointer_gesture_hold_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define ZWP_POINTER_GESTURE_HOLD_V1_BEGIN 0
#define ZWP_POINTER_GESTURE_HOLD_V1_END 1

#define ZWP_POINTER_GESTURE_HOLD_V1_BEGIN_SINCE_VERSION 3

#define ZWP_POINTER_GESTURE_HOLD_V1_END_SINCE_VERSION 3

#define ZWP_POINTER_GESTURE_HOLD_V1_DESTROY_SINCE_VERSION 3

static inline void zwp_pointer_gesture_hold_v1_send_begin(
    struct wl_resource *resource_, uint32_t serial, uint32_t time,
    struct wl_resource *surface, uint32_t fingers) {
  wl_resource_post_event(resource_, ZWP_POINTER_GESTURE_HOLD_V1_BEGIN, serial,
                         time, surface, fingers);
}

static inline void
zwp_pointer_gesture_hold_v1_send_end(struct wl_resource *resource_,
                                     uint32_t serial, uint32_t time,
                                     int32_t cancelled) {
  wl_resource_post_event(resource_, ZWP_POINTER_GESTURE_HOLD_V1_END, serial,
                         time, cancelled);
}

#ifdef __cplusplus
}
#endif

#endif
