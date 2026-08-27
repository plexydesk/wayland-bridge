/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef XWAYLAND_SHELL_V1_SERVER_PROTOCOL_H
#define XWAYLAND_SHELL_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_surface;
struct xwayland_shell_v1;
struct xwayland_surface_v1;

#ifndef XWAYLAND_SHELL_V1_INTERFACE
#define XWAYLAND_SHELL_V1_INTERFACE

extern const struct wl_interface xwayland_shell_v1_interface;
#endif
#ifndef XWAYLAND_SURFACE_V1_INTERFACE
#define XWAYLAND_SURFACE_V1_INTERFACE

extern const struct wl_interface xwayland_surface_v1_interface;
#endif

#ifndef XWAYLAND_SHELL_V1_ERROR_ENUM
#define XWAYLAND_SHELL_V1_ERROR_ENUM
enum xwayland_shell_v1_error {

  XWAYLAND_SHELL_V1_ERROR_ROLE = 0,
};
#endif

#ifndef XWAYLAND_SHELL_V1_ERROR_ENUM_IS_VALID
#define XWAYLAND_SHELL_V1_ERROR_ENUM_IS_VALID

static inline bool xwayland_shell_v1_error_is_valid(uint32_t value,
                                                    uint32_t version) {
  switch (value) {
  case XWAYLAND_SHELL_V1_ERROR_ROLE:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct xwayland_shell_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_xwayland_surface)(struct wl_client *client,
                               struct wl_resource *resource, uint32_t id,
                               struct wl_resource *surface);
};

#define XWAYLAND_SHELL_V1_DESTROY_SINCE_VERSION 1

#define XWAYLAND_SHELL_V1_GET_XWAYLAND_SURFACE_SINCE_VERSION 1

#ifndef XWAYLAND_SURFACE_V1_ERROR_ENUM
#define XWAYLAND_SURFACE_V1_ERROR_ENUM
enum xwayland_surface_v1_error {

  XWAYLAND_SURFACE_V1_ERROR_ALREADY_ASSOCIATED = 0,

  XWAYLAND_SURFACE_V1_ERROR_INVALID_SERIAL = 1,
};
#endif

#ifndef XWAYLAND_SURFACE_V1_ERROR_ENUM_IS_VALID
#define XWAYLAND_SURFACE_V1_ERROR_ENUM_IS_VALID

static inline bool xwayland_surface_v1_error_is_valid(uint32_t value,
                                                      uint32_t version) {
  switch (value) {
  case XWAYLAND_SURFACE_V1_ERROR_ALREADY_ASSOCIATED:
    return version >= 1;
  case XWAYLAND_SURFACE_V1_ERROR_INVALID_SERIAL:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct xwayland_surface_v1_interface {

  void (*set_serial)(struct wl_client *client, struct wl_resource *resource,
                     uint32_t serial_lo, uint32_t serial_hi);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define XWAYLAND_SURFACE_V1_SET_SERIAL_SINCE_VERSION 1

#define XWAYLAND_SURFACE_V1_DESTROY_SINCE_VERSION 1

#ifdef __cplusplus
}
#endif

#endif
