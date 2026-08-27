/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef FIFO_V1_SERVER_PROTOCOL_H
#define FIFO_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_surface;
struct wp_fifo_manager_v1;
struct wp_fifo_v1;

#ifndef WP_FIFO_MANAGER_V1_INTERFACE
#define WP_FIFO_MANAGER_V1_INTERFACE

extern const struct wl_interface wp_fifo_manager_v1_interface;
#endif
#ifndef WP_FIFO_V1_INTERFACE
#define WP_FIFO_V1_INTERFACE

extern const struct wl_interface wp_fifo_v1_interface;
#endif

#ifndef WP_FIFO_MANAGER_V1_ERROR_ENUM
#define WP_FIFO_MANAGER_V1_ERROR_ENUM

enum wp_fifo_manager_v1_error {

  WP_FIFO_MANAGER_V1_ERROR_ALREADY_EXISTS = 0,
};
#endif

#ifndef WP_FIFO_MANAGER_V1_ERROR_ENUM_IS_VALID
#define WP_FIFO_MANAGER_V1_ERROR_ENUM_IS_VALID

static inline bool wp_fifo_manager_v1_error_is_valid(uint32_t value,
                                                     uint32_t version) {
  switch (value) {
  case WP_FIFO_MANAGER_V1_ERROR_ALREADY_EXISTS:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct wp_fifo_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_fifo)(struct wl_client *client, struct wl_resource *resource,
                   uint32_t id, struct wl_resource *surface);
};

#define WP_FIFO_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define WP_FIFO_MANAGER_V1_GET_FIFO_SINCE_VERSION 1

#ifndef WP_FIFO_V1_ERROR_ENUM
#define WP_FIFO_V1_ERROR_ENUM

enum wp_fifo_v1_error {

  WP_FIFO_V1_ERROR_SURFACE_DESTROYED = 0,
};
#endif

#ifndef WP_FIFO_V1_ERROR_ENUM_IS_VALID
#define WP_FIFO_V1_ERROR_ENUM_IS_VALID

static inline bool wp_fifo_v1_error_is_valid(uint32_t value, uint32_t version) {
  switch (value) {
  case WP_FIFO_V1_ERROR_SURFACE_DESTROYED:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct wp_fifo_v1_interface {

  void (*set_barrier)(struct wl_client *client, struct wl_resource *resource);

  void (*wait_barrier)(struct wl_client *client, struct wl_resource *resource);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define WP_FIFO_V1_SET_BARRIER_SINCE_VERSION 1

#define WP_FIFO_V1_WAIT_BARRIER_SINCE_VERSION 1

#define WP_FIFO_V1_DESTROY_SINCE_VERSION 1

#ifdef __cplusplus
}
#endif

#endif
