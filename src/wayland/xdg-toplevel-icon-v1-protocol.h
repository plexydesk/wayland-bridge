/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef XDG_TOPLEVEL_ICON_V1_SERVER_PROTOCOL_H
#define XDG_TOPLEVEL_ICON_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_buffer;
struct xdg_toplevel;
struct xdg_toplevel_icon_manager_v1;
struct xdg_toplevel_icon_v1;

#ifndef XDG_TOPLEVEL_ICON_MANAGER_V1_INTERFACE
#define XDG_TOPLEVEL_ICON_MANAGER_V1_INTERFACE

extern const struct wl_interface xdg_toplevel_icon_manager_v1_interface;
#endif
#ifndef XDG_TOPLEVEL_ICON_V1_INTERFACE
#define XDG_TOPLEVEL_ICON_V1_INTERFACE

extern const struct wl_interface xdg_toplevel_icon_v1_interface;
#endif

struct xdg_toplevel_icon_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*create_icon)(struct wl_client *client, struct wl_resource *resource,
                      uint32_t id);

  void (*set_icon)(struct wl_client *client, struct wl_resource *resource,
                   struct wl_resource *toplevel, struct wl_resource *icon);
};

#define XDG_TOPLEVEL_ICON_MANAGER_V1_ICON_SIZE 0
#define XDG_TOPLEVEL_ICON_MANAGER_V1_DONE 1

#define XDG_TOPLEVEL_ICON_MANAGER_V1_ICON_SIZE_SINCE_VERSION 1

#define XDG_TOPLEVEL_ICON_MANAGER_V1_DONE_SINCE_VERSION 1

#define XDG_TOPLEVEL_ICON_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define XDG_TOPLEVEL_ICON_MANAGER_V1_CREATE_ICON_SINCE_VERSION 1

#define XDG_TOPLEVEL_ICON_MANAGER_V1_SET_ICON_SINCE_VERSION 1

static inline void
xdg_toplevel_icon_manager_v1_send_icon_size(struct wl_resource *resource_,
                                            int32_t size) {
  wl_resource_post_event(resource_, XDG_TOPLEVEL_ICON_MANAGER_V1_ICON_SIZE,
                         size);
}

static inline void
xdg_toplevel_icon_manager_v1_send_done(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, XDG_TOPLEVEL_ICON_MANAGER_V1_DONE);
}

#ifndef XDG_TOPLEVEL_ICON_V1_ERROR_ENUM
#define XDG_TOPLEVEL_ICON_V1_ERROR_ENUM
enum xdg_toplevel_icon_v1_error {

  XDG_TOPLEVEL_ICON_V1_ERROR_INVALID_BUFFER = 1,

  XDG_TOPLEVEL_ICON_V1_ERROR_IMMUTABLE = 2,

  XDG_TOPLEVEL_ICON_V1_ERROR_NO_BUFFER = 3,
};
#endif

#ifndef XDG_TOPLEVEL_ICON_V1_ERROR_ENUM_IS_VALID
#define XDG_TOPLEVEL_ICON_V1_ERROR_ENUM_IS_VALID

static inline bool xdg_toplevel_icon_v1_error_is_valid(uint32_t value,
                                                       uint32_t version) {
  switch (value) {
  case XDG_TOPLEVEL_ICON_V1_ERROR_INVALID_BUFFER:
    return version >= 1;
  case XDG_TOPLEVEL_ICON_V1_ERROR_IMMUTABLE:
    return version >= 1;
  case XDG_TOPLEVEL_ICON_V1_ERROR_NO_BUFFER:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct xdg_toplevel_icon_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*set_name)(struct wl_client *client, struct wl_resource *resource,
                   const char *icon_name);

  void (*add_buffer)(struct wl_client *client, struct wl_resource *resource,
                     struct wl_resource *buffer, int32_t scale);
};

#define XDG_TOPLEVEL_ICON_V1_DESTROY_SINCE_VERSION 1

#define XDG_TOPLEVEL_ICON_V1_SET_NAME_SINCE_VERSION 1

#define XDG_TOPLEVEL_ICON_V1_ADD_BUFFER_SINCE_VERSION 1

#ifdef __cplusplus
}
#endif

#endif
