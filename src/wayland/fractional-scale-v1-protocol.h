/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef FRACTIONAL_SCALE_V1_SERVER_PROTOCOL_H
#define FRACTIONAL_SCALE_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_surface;
struct wp_fractional_scale_manager_v1;
struct wp_fractional_scale_v1;

#ifndef WP_FRACTIONAL_SCALE_MANAGER_V1_INTERFACE
#define WP_FRACTIONAL_SCALE_MANAGER_V1_INTERFACE

extern const struct wl_interface wp_fractional_scale_manager_v1_interface;
#endif
#ifndef WP_FRACTIONAL_SCALE_V1_INTERFACE
#define WP_FRACTIONAL_SCALE_V1_INTERFACE

extern const struct wl_interface wp_fractional_scale_v1_interface;
#endif

#ifndef WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_ENUM
#define WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_ENUM
enum wp_fractional_scale_manager_v1_error {

  WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_FRACTIONAL_SCALE_EXISTS = 0,
};
#endif

#ifndef WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_ENUM_IS_VALID
#define WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_ENUM_IS_VALID

static inline bool
wp_fractional_scale_manager_v1_error_is_valid(uint32_t value,
                                              uint32_t version) {
  switch (value) {
  case WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_FRACTIONAL_SCALE_EXISTS:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct wp_fractional_scale_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_fractional_scale)(struct wl_client *client,
                               struct wl_resource *resource, uint32_t id,
                               struct wl_resource *surface);
};

#define WP_FRACTIONAL_SCALE_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define WP_FRACTIONAL_SCALE_MANAGER_V1_GET_FRACTIONAL_SCALE_SINCE_VERSION 1

struct wp_fractional_scale_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define WP_FRACTIONAL_SCALE_V1_PREFERRED_SCALE 0

#define WP_FRACTIONAL_SCALE_V1_PREFERRED_SCALE_SINCE_VERSION 1

#define WP_FRACTIONAL_SCALE_V1_DESTROY_SINCE_VERSION 1

static inline void
wp_fractional_scale_v1_send_preferred_scale(struct wl_resource *resource_,
                                            uint32_t scale) {
  wl_resource_post_event(resource_, WP_FRACTIONAL_SCALE_V1_PREFERRED_SCALE,
                         scale);
}

#ifdef __cplusplus
}
#endif

#endif
