/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef XDG_TOPLEVEL_DRAG_V1_SERVER_PROTOCOL_H
#define XDG_TOPLEVEL_DRAG_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_data_source;
struct xdg_toplevel;
struct xdg_toplevel_drag_manager_v1;
struct xdg_toplevel_drag_v1;

#ifndef XDG_TOPLEVEL_DRAG_MANAGER_V1_INTERFACE
#define XDG_TOPLEVEL_DRAG_MANAGER_V1_INTERFACE

extern const struct wl_interface xdg_toplevel_drag_manager_v1_interface;
#endif
#ifndef XDG_TOPLEVEL_DRAG_V1_INTERFACE
#define XDG_TOPLEVEL_DRAG_V1_INTERFACE

extern const struct wl_interface xdg_toplevel_drag_v1_interface;
#endif

#ifndef XDG_TOPLEVEL_DRAG_MANAGER_V1_ERROR_ENUM
#define XDG_TOPLEVEL_DRAG_MANAGER_V1_ERROR_ENUM
enum xdg_toplevel_drag_manager_v1_error {

  XDG_TOPLEVEL_DRAG_MANAGER_V1_ERROR_INVALID_SOURCE = 0,
};
#endif

#ifndef XDG_TOPLEVEL_DRAG_MANAGER_V1_ERROR_ENUM_IS_VALID
#define XDG_TOPLEVEL_DRAG_MANAGER_V1_ERROR_ENUM_IS_VALID

static inline bool
xdg_toplevel_drag_manager_v1_error_is_valid(uint32_t value, uint32_t version) {
  switch (value) {
  case XDG_TOPLEVEL_DRAG_MANAGER_V1_ERROR_INVALID_SOURCE:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct xdg_toplevel_drag_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_xdg_toplevel_drag)(struct wl_client *client,
                                struct wl_resource *resource, uint32_t id,
                                struct wl_resource *data_source);
};

#define XDG_TOPLEVEL_DRAG_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define XDG_TOPLEVEL_DRAG_MANAGER_V1_GET_XDG_TOPLEVEL_DRAG_SINCE_VERSION 1

#ifndef XDG_TOPLEVEL_DRAG_V1_ERROR_ENUM
#define XDG_TOPLEVEL_DRAG_V1_ERROR_ENUM
enum xdg_toplevel_drag_v1_error {

  XDG_TOPLEVEL_DRAG_V1_ERROR_TOPLEVEL_ATTACHED = 0,

  XDG_TOPLEVEL_DRAG_V1_ERROR_ONGOING_DRAG = 1,
};
#endif

#ifndef XDG_TOPLEVEL_DRAG_V1_ERROR_ENUM_IS_VALID
#define XDG_TOPLEVEL_DRAG_V1_ERROR_ENUM_IS_VALID

static inline bool xdg_toplevel_drag_v1_error_is_valid(uint32_t value,
                                                       uint32_t version) {
  switch (value) {
  case XDG_TOPLEVEL_DRAG_V1_ERROR_TOPLEVEL_ATTACHED:
    return version >= 1;
  case XDG_TOPLEVEL_DRAG_V1_ERROR_ONGOING_DRAG:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct xdg_toplevel_drag_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*attach)(struct wl_client *client, struct wl_resource *resource,
                 struct wl_resource *toplevel, int32_t x_offset,
                 int32_t y_offset);
};

#define XDG_TOPLEVEL_DRAG_V1_DESTROY_SINCE_VERSION 1

#define XDG_TOPLEVEL_DRAG_V1_ATTACH_SINCE_VERSION 1

#ifdef __cplusplus
}
#endif

#endif
