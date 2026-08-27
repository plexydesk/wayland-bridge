/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef ALPHA_MODIFIER_V1_SERVER_PROTOCOL_H
#define ALPHA_MODIFIER_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_surface;
struct wp_alpha_modifier_surface_v1;
struct wp_alpha_modifier_v1;

#ifndef WP_ALPHA_MODIFIER_V1_INTERFACE
#define WP_ALPHA_MODIFIER_V1_INTERFACE

extern const struct wl_interface wp_alpha_modifier_v1_interface;
#endif
#ifndef WP_ALPHA_MODIFIER_SURFACE_V1_INTERFACE
#define WP_ALPHA_MODIFIER_SURFACE_V1_INTERFACE

extern const struct wl_interface wp_alpha_modifier_surface_v1_interface;
#endif

#ifndef WP_ALPHA_MODIFIER_V1_ERROR_ENUM
#define WP_ALPHA_MODIFIER_V1_ERROR_ENUM
enum wp_alpha_modifier_v1_error {

  WP_ALPHA_MODIFIER_V1_ERROR_ALREADY_CONSTRUCTED = 0,
};
#endif

#ifndef WP_ALPHA_MODIFIER_V1_ERROR_ENUM_IS_VALID
#define WP_ALPHA_MODIFIER_V1_ERROR_ENUM_IS_VALID

static inline bool wp_alpha_modifier_v1_error_is_valid(uint32_t value,
                                                       uint32_t version) {
  switch (value) {
  case WP_ALPHA_MODIFIER_V1_ERROR_ALREADY_CONSTRUCTED:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct wp_alpha_modifier_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_surface)(struct wl_client *client, struct wl_resource *resource,
                      uint32_t id, struct wl_resource *surface);
};

#define WP_ALPHA_MODIFIER_V1_DESTROY_SINCE_VERSION 1

#define WP_ALPHA_MODIFIER_V1_GET_SURFACE_SINCE_VERSION 1

#ifndef WP_ALPHA_MODIFIER_SURFACE_V1_ERROR_ENUM
#define WP_ALPHA_MODIFIER_SURFACE_V1_ERROR_ENUM
enum wp_alpha_modifier_surface_v1_error {

  WP_ALPHA_MODIFIER_SURFACE_V1_ERROR_NO_SURFACE = 0,
};
#endif

#ifndef WP_ALPHA_MODIFIER_SURFACE_V1_ERROR_ENUM_IS_VALID
#define WP_ALPHA_MODIFIER_SURFACE_V1_ERROR_ENUM_IS_VALID

static inline bool
wp_alpha_modifier_surface_v1_error_is_valid(uint32_t value, uint32_t version) {
  switch (value) {
  case WP_ALPHA_MODIFIER_SURFACE_V1_ERROR_NO_SURFACE:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct wp_alpha_modifier_surface_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*set_multiplier)(struct wl_client *client, struct wl_resource *resource,
                         uint32_t factor);
};

#define WP_ALPHA_MODIFIER_SURFACE_V1_DESTROY_SINCE_VERSION 1

#define WP_ALPHA_MODIFIER_SURFACE_V1_SET_MULTIPLIER_SINCE_VERSION 1

#ifdef __cplusplus
}
#endif

#endif
