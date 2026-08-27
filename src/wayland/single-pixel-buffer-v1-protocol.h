/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef SINGLE_PIXEL_BUFFER_V1_SERVER_PROTOCOL_H
#define SINGLE_PIXEL_BUFFER_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_buffer;
struct wp_single_pixel_buffer_manager_v1;

#ifndef WP_SINGLE_PIXEL_BUFFER_MANAGER_V1_INTERFACE
#define WP_SINGLE_PIXEL_BUFFER_MANAGER_V1_INTERFACE

extern const struct wl_interface wp_single_pixel_buffer_manager_v1_interface;
#endif

struct wp_single_pixel_buffer_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*create_u32_rgba_buffer)(struct wl_client *client,
                                 struct wl_resource *resource, uint32_t id,
                                 uint32_t r, uint32_t g, uint32_t b,
                                 uint32_t a);
};

#define WP_SINGLE_PIXEL_BUFFER_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define WP_SINGLE_PIXEL_BUFFER_MANAGER_V1_CREATE_U32_RGBA_BUFFER_SINCE_VERSION 1

#ifdef __cplusplus
}
#endif

#endif
