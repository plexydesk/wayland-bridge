/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef XDG_DECORATION_UNSTABLE_V1_SERVER_PROTOCOL_H
#define XDG_DECORATION_UNSTABLE_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct xdg_toplevel;
struct zxdg_decoration_manager_v1;
struct zxdg_toplevel_decoration_v1;

#ifndef ZXDG_DECORATION_MANAGER_V1_INTERFACE
#define ZXDG_DECORATION_MANAGER_V1_INTERFACE

extern const struct wl_interface zxdg_decoration_manager_v1_interface;
#endif
#ifndef ZXDG_TOPLEVEL_DECORATION_V1_INTERFACE
#define ZXDG_TOPLEVEL_DECORATION_V1_INTERFACE

extern const struct wl_interface zxdg_toplevel_decoration_v1_interface;
#endif

struct zxdg_decoration_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_toplevel_decoration)(struct wl_client *client,
                                  struct wl_resource *resource, uint32_t id,
                                  struct wl_resource *toplevel);
};

#define ZXDG_DECORATION_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define ZXDG_DECORATION_MANAGER_V1_GET_TOPLEVEL_DECORATION_SINCE_VERSION 1

#ifndef ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ENUM
#define ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ENUM
enum zxdg_toplevel_decoration_v1_error {

  ZXDG_TOPLEVEL_DECORATION_V1_ERROR_UNCONFIGURED_BUFFER = 0,

  ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ALREADY_CONSTRUCTED = 1,

  ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ORPHANED = 2,

  ZXDG_TOPLEVEL_DECORATION_V1_ERROR_INVALID_MODE = 3,
};
#endif

#ifndef ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ENUM_IS_VALID
#define ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ENUM_IS_VALID

static inline bool
zxdg_toplevel_decoration_v1_error_is_valid(uint32_t value, uint32_t version) {
  switch (value) {
  case ZXDG_TOPLEVEL_DECORATION_V1_ERROR_UNCONFIGURED_BUFFER:
    return version >= 1;
  case ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ALREADY_CONSTRUCTED:
    return version >= 1;
  case ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ORPHANED:
    return version >= 1;
  case ZXDG_TOPLEVEL_DECORATION_V1_ERROR_INVALID_MODE:
    return version >= 1;
  default:
    return false;
  }
}
#endif

#ifndef ZXDG_TOPLEVEL_DECORATION_V1_MODE_ENUM
#define ZXDG_TOPLEVEL_DECORATION_V1_MODE_ENUM

enum zxdg_toplevel_decoration_v1_mode {

  ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE = 1,

  ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE = 2,
};
#endif

#ifndef ZXDG_TOPLEVEL_DECORATION_V1_MODE_ENUM_IS_VALID
#define ZXDG_TOPLEVEL_DECORATION_V1_MODE_ENUM_IS_VALID

static inline bool zxdg_toplevel_decoration_v1_mode_is_valid(uint32_t value,
                                                             uint32_t version) {
  switch (value) {
  case ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE:
    return version >= 1;
  case ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct zxdg_toplevel_decoration_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*set_mode)(struct wl_client *client, struct wl_resource *resource,
                   uint32_t mode);

  void (*unset_mode)(struct wl_client *client, struct wl_resource *resource);
};

#define ZXDG_TOPLEVEL_DECORATION_V1_CONFIGURE 0

#define ZXDG_TOPLEVEL_DECORATION_V1_CONFIGURE_SINCE_VERSION 1

#define ZXDG_TOPLEVEL_DECORATION_V1_DESTROY_SINCE_VERSION 1

#define ZXDG_TOPLEVEL_DECORATION_V1_SET_MODE_SINCE_VERSION 1

#define ZXDG_TOPLEVEL_DECORATION_V1_UNSET_MODE_SINCE_VERSION 1

static inline void
zxdg_toplevel_decoration_v1_send_configure(struct wl_resource *resource_,
                                           uint32_t mode) {
  wl_resource_post_event(resource_, ZXDG_TOPLEVEL_DECORATION_V1_CONFIGURE,
                         mode);
}

#ifdef __cplusplus
}
#endif

#endif
