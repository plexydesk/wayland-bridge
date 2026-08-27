/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef IDLE_INHIBIT_UNSTABLE_V1_SERVER_PROTOCOL_H
#define IDLE_INHIBIT_UNSTABLE_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_surface;
struct zwp_idle_inhibit_manager_v1;
struct zwp_idle_inhibitor_v1;

#ifndef ZWP_IDLE_INHIBIT_MANAGER_V1_INTERFACE
#define ZWP_IDLE_INHIBIT_MANAGER_V1_INTERFACE

extern const struct wl_interface zwp_idle_inhibit_manager_v1_interface;
#endif
#ifndef ZWP_IDLE_INHIBITOR_V1_INTERFACE
#define ZWP_IDLE_INHIBITOR_V1_INTERFACE

extern const struct wl_interface zwp_idle_inhibitor_v1_interface;
#endif

struct zwp_idle_inhibit_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*create_inhibitor)(struct wl_client *client,
                           struct wl_resource *resource, uint32_t id,
                           struct wl_resource *surface);
};

#define ZWP_IDLE_INHIBIT_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define ZWP_IDLE_INHIBIT_MANAGER_V1_CREATE_INHIBITOR_SINCE_VERSION 1

struct zwp_idle_inhibitor_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define ZWP_IDLE_INHIBITOR_V1_DESTROY_SINCE_VERSION 1

#ifdef __cplusplus
}
#endif

#endif
