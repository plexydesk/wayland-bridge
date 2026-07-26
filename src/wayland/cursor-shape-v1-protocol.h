/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef CURSOR_SHAPE_V1_SERVER_PROTOCOL_H
#define CURSOR_SHAPE_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_pointer;
struct wp_cursor_shape_device_v1;
struct wp_cursor_shape_manager_v1;
struct zwp_tablet_tool_v2;

#ifndef WP_CURSOR_SHAPE_MANAGER_V1_INTERFACE
#define WP_CURSOR_SHAPE_MANAGER_V1_INTERFACE

extern const struct wl_interface wp_cursor_shape_manager_v1_interface;
#endif
#ifndef WP_CURSOR_SHAPE_DEVICE_V1_INTERFACE
#define WP_CURSOR_SHAPE_DEVICE_V1_INTERFACE

extern const struct wl_interface wp_cursor_shape_device_v1_interface;
#endif

struct wp_cursor_shape_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_pointer)(struct wl_client *client, struct wl_resource *resource,
                      uint32_t cursor_shape_device,
                      struct wl_resource *pointer);

  void (*get_tablet_tool_v2)(struct wl_client *client,
                             struct wl_resource *resource,
                             uint32_t cursor_shape_device,
                             struct wl_resource *tablet_tool);
};

#define WP_CURSOR_SHAPE_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define WP_CURSOR_SHAPE_MANAGER_V1_GET_POINTER_SINCE_VERSION 1

#define WP_CURSOR_SHAPE_MANAGER_V1_GET_TABLET_TOOL_V2_SINCE_VERSION 1

#ifndef WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ENUM
#define WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ENUM

enum wp_cursor_shape_device_v1_shape {

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT = 1,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CONTEXT_MENU = 2,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_HELP = 3,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER = 4,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_PROGRESS = 5,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT = 6,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CELL = 7,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR = 8,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT = 9,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_VERTICAL_TEXT = 10,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALIAS = 11,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COPY = 12,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE = 13,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NO_DROP = 14,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED = 15,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRAB = 16,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRABBING = 17,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_E_RESIZE = 18,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_N_RESIZE = 19,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NE_RESIZE = 20,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NW_RESIZE = 21,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_S_RESIZE = 22,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SE_RESIZE = 23,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SW_RESIZE = 24,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_W_RESIZE = 25,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE = 26,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE = 27,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE = 28,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE = 29,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COL_RESIZE = 30,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ROW_RESIZE = 31,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL = 32,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_IN = 33,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_OUT = 34,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DND_ASK = 35,

  WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_RESIZE = 36,
};

#define WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DND_ASK_SINCE_VERSION 2

#define WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_RESIZE_SINCE_VERSION 2
#endif

#ifndef WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ENUM_IS_VALID
#define WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ENUM_IS_VALID

static inline bool wp_cursor_shape_device_v1_shape_is_valid(uint32_t value,
                                                            uint32_t version) {
  switch (value) {
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CONTEXT_MENU:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_HELP:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_PROGRESS:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CELL:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_VERTICAL_TEXT:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALIAS:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COPY:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NO_DROP:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRAB:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRABBING:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_E_RESIZE:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_N_RESIZE:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NE_RESIZE:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NW_RESIZE:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_S_RESIZE:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SE_RESIZE:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SW_RESIZE:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_W_RESIZE:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COL_RESIZE:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ROW_RESIZE:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_IN:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_OUT:
    return version >= 1;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DND_ASK:
    return version >= 2;
  case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_RESIZE:
    return version >= 2;
  default:
    return false;
  }
}
#endif

#ifndef WP_CURSOR_SHAPE_DEVICE_V1_ERROR_ENUM
#define WP_CURSOR_SHAPE_DEVICE_V1_ERROR_ENUM
enum wp_cursor_shape_device_v1_error {

  WP_CURSOR_SHAPE_DEVICE_V1_ERROR_INVALID_SHAPE = 1,
};
#endif

#ifndef WP_CURSOR_SHAPE_DEVICE_V1_ERROR_ENUM_IS_VALID
#define WP_CURSOR_SHAPE_DEVICE_V1_ERROR_ENUM_IS_VALID

static inline bool wp_cursor_shape_device_v1_error_is_valid(uint32_t value,
                                                            uint32_t version) {
  switch (value) {
  case WP_CURSOR_SHAPE_DEVICE_V1_ERROR_INVALID_SHAPE:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct wp_cursor_shape_device_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*set_shape)(struct wl_client *client, struct wl_resource *resource,
                    uint32_t serial, uint32_t shape);
};

#define WP_CURSOR_SHAPE_DEVICE_V1_DESTROY_SINCE_VERSION 1

#define WP_CURSOR_SHAPE_DEVICE_V1_SET_SHAPE_SINCE_VERSION 1

#ifdef __cplusplus
}
#endif

#endif
