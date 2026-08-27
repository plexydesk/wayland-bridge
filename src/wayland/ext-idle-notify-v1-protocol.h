/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef EXT_IDLE_NOTIFY_V1_SERVER_PROTOCOL_H
#define EXT_IDLE_NOTIFY_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct ext_idle_notification_v1;
struct ext_idle_notifier_v1;
struct wl_seat;

#ifndef EXT_IDLE_NOTIFIER_V1_INTERFACE
#define EXT_IDLE_NOTIFIER_V1_INTERFACE

extern const struct wl_interface ext_idle_notifier_v1_interface;
#endif
#ifndef EXT_IDLE_NOTIFICATION_V1_INTERFACE
#define EXT_IDLE_NOTIFICATION_V1_INTERFACE

extern const struct wl_interface ext_idle_notification_v1_interface;
#endif

struct ext_idle_notifier_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_idle_notification)(struct wl_client *client,
                                struct wl_resource *resource, uint32_t id,
                                uint32_t timeout, struct wl_resource *seat);

  void (*get_input_idle_notification)(struct wl_client *client,
                                      struct wl_resource *resource, uint32_t id,
                                      uint32_t timeout,
                                      struct wl_resource *seat);
};

#define EXT_IDLE_NOTIFIER_V1_DESTROY_SINCE_VERSION 1

#define EXT_IDLE_NOTIFIER_V1_GET_IDLE_NOTIFICATION_SINCE_VERSION 1

#define EXT_IDLE_NOTIFIER_V1_GET_INPUT_IDLE_NOTIFICATION_SINCE_VERSION 2

struct ext_idle_notification_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define EXT_IDLE_NOTIFICATION_V1_IDLED 0
#define EXT_IDLE_NOTIFICATION_V1_RESUMED 1

#define EXT_IDLE_NOTIFICATION_V1_IDLED_SINCE_VERSION 1

#define EXT_IDLE_NOTIFICATION_V1_RESUMED_SINCE_VERSION 1

#define EXT_IDLE_NOTIFICATION_V1_DESTROY_SINCE_VERSION 1

static inline void
ext_idle_notification_v1_send_idled(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, EXT_IDLE_NOTIFICATION_V1_IDLED);
}

static inline void
ext_idle_notification_v1_send_resumed(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, EXT_IDLE_NOTIFICATION_V1_RESUMED);
}

#ifdef __cplusplus
}
#endif

#endif
