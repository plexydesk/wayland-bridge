/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef TEARING_CONTROL_V1_SERVER_PROTOCOL_H
#define TEARING_CONTROL_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_surface;
struct wp_tearing_control_manager_v1;
struct wp_tearing_control_v1;

#ifndef WP_TEARING_CONTROL_MANAGER_V1_INTERFACE
#define WP_TEARING_CONTROL_MANAGER_V1_INTERFACE

extern const struct wl_interface wp_tearing_control_manager_v1_interface;
#endif
#ifndef WP_TEARING_CONTROL_V1_INTERFACE
#define WP_TEARING_CONTROL_V1_INTERFACE

extern const struct wl_interface wp_tearing_control_v1_interface;
#endif

#ifndef WP_TEARING_CONTROL_MANAGER_V1_ERROR_ENUM
#define WP_TEARING_CONTROL_MANAGER_V1_ERROR_ENUM
enum wp_tearing_control_manager_v1_error {

  WP_TEARING_CONTROL_MANAGER_V1_ERROR_TEARING_CONTROL_EXISTS = 0,
};
#endif

#ifndef WP_TEARING_CONTROL_MANAGER_V1_ERROR_ENUM_IS_VALID
#define WP_TEARING_CONTROL_MANAGER_V1_ERROR_ENUM_IS_VALID

static inline bool
wp_tearing_control_manager_v1_error_is_valid(uint32_t value, uint32_t version) {
  switch (value) {
  case WP_TEARING_CONTROL_MANAGER_V1_ERROR_TEARING_CONTROL_EXISTS:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct wp_tearing_control_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_tearing_control)(struct wl_client *client,
                              struct wl_resource *resource, uint32_t id,
                              struct wl_resource *surface);
};

#define WP_TEARING_CONTROL_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define WP_TEARING_CONTROL_MANAGER_V1_GET_TEARING_CONTROL_SINCE_VERSION 1

#ifndef WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ENUM
#define WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ENUM

enum wp_tearing_control_v1_presentation_hint {

  WP_TEARING_CONTROL_V1_PRESENTATION_HINT_VSYNC = 0,

  WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC = 1,
};
#endif

#ifndef WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ENUM_IS_VALID
#define WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ENUM_IS_VALID

static inline bool
wp_tearing_control_v1_presentation_hint_is_valid(uint32_t value,
                                                 uint32_t version) {
  switch (value) {
  case WP_TEARING_CONTROL_V1_PRESENTATION_HINT_VSYNC:
    return version >= 1;
  case WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct wp_tearing_control_v1_interface {

  void (*set_presentation_hint)(struct wl_client *client,
                                struct wl_resource *resource, uint32_t hint);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define WP_TEARING_CONTROL_V1_SET_PRESENTATION_HINT_SINCE_VERSION 1

#define WP_TEARING_CONTROL_V1_DESTROY_SINCE_VERSION 1

#ifdef __cplusplus
}
#endif

#endif
