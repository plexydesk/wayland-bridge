/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef CONTENT_TYPE_V1_SERVER_PROTOCOL_H
#define CONTENT_TYPE_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_surface;
struct wp_content_type_manager_v1;
struct wp_content_type_v1;

#ifndef WP_CONTENT_TYPE_MANAGER_V1_INTERFACE
#define WP_CONTENT_TYPE_MANAGER_V1_INTERFACE

extern const struct wl_interface wp_content_type_manager_v1_interface;
#endif
#ifndef WP_CONTENT_TYPE_V1_INTERFACE
#define WP_CONTENT_TYPE_V1_INTERFACE

extern const struct wl_interface wp_content_type_v1_interface;
#endif

#ifndef WP_CONTENT_TYPE_MANAGER_V1_ERROR_ENUM
#define WP_CONTENT_TYPE_MANAGER_V1_ERROR_ENUM
enum wp_content_type_manager_v1_error {

  WP_CONTENT_TYPE_MANAGER_V1_ERROR_ALREADY_CONSTRUCTED = 0,
};
#endif

#ifndef WP_CONTENT_TYPE_MANAGER_V1_ERROR_ENUM_IS_VALID
#define WP_CONTENT_TYPE_MANAGER_V1_ERROR_ENUM_IS_VALID

static inline bool wp_content_type_manager_v1_error_is_valid(uint32_t value,
                                                             uint32_t version) {
  switch (value) {
  case WP_CONTENT_TYPE_MANAGER_V1_ERROR_ALREADY_CONSTRUCTED:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct wp_content_type_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_surface_content_type)(struct wl_client *client,
                                   struct wl_resource *resource, uint32_t id,
                                   struct wl_resource *surface);
};

#define WP_CONTENT_TYPE_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define WP_CONTENT_TYPE_MANAGER_V1_GET_SURFACE_CONTENT_TYPE_SINCE_VERSION 1

#ifndef WP_CONTENT_TYPE_V1_TYPE_ENUM
#define WP_CONTENT_TYPE_V1_TYPE_ENUM

enum wp_content_type_v1_type {

  WP_CONTENT_TYPE_V1_TYPE_NONE = 0,

  WP_CONTENT_TYPE_V1_TYPE_PHOTO = 1,

  WP_CONTENT_TYPE_V1_TYPE_VIDEO = 2,

  WP_CONTENT_TYPE_V1_TYPE_GAME = 3,
};
#endif

#ifndef WP_CONTENT_TYPE_V1_TYPE_ENUM_IS_VALID
#define WP_CONTENT_TYPE_V1_TYPE_ENUM_IS_VALID

static inline bool wp_content_type_v1_type_is_valid(uint32_t value,
                                                    uint32_t version) {
  switch (value) {
  case WP_CONTENT_TYPE_V1_TYPE_NONE:
    return version >= 1;
  case WP_CONTENT_TYPE_V1_TYPE_PHOTO:
    return version >= 1;
  case WP_CONTENT_TYPE_V1_TYPE_VIDEO:
    return version >= 1;
  case WP_CONTENT_TYPE_V1_TYPE_GAME:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct wp_content_type_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*set_content_type)(struct wl_client *client,
                           struct wl_resource *resource, uint32_t content_type);
};

#define WP_CONTENT_TYPE_V1_DESTROY_SINCE_VERSION 1

#define WP_CONTENT_TYPE_V1_SET_CONTENT_TYPE_SINCE_VERSION 1

#ifdef __cplusplus
}
#endif

#endif
