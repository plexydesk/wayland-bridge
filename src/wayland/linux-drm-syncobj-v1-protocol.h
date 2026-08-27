/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef LINUX_DRM_SYNCOBJ_V1_SERVER_PROTOCOL_H
#define LINUX_DRM_SYNCOBJ_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_surface;
struct wp_linux_drm_syncobj_manager_v1;
struct wp_linux_drm_syncobj_surface_v1;
struct wp_linux_drm_syncobj_timeline_v1;

#ifndef WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_INTERFACE
#define WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_INTERFACE

extern const struct wl_interface wp_linux_drm_syncobj_manager_v1_interface;
#endif
#ifndef WP_LINUX_DRM_SYNCOBJ_TIMELINE_V1_INTERFACE
#define WP_LINUX_DRM_SYNCOBJ_TIMELINE_V1_INTERFACE

extern const struct wl_interface wp_linux_drm_syncobj_timeline_v1_interface;
#endif
#ifndef WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_INTERFACE
#define WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_INTERFACE

extern const struct wl_interface wp_linux_drm_syncobj_surface_v1_interface;
#endif

#ifndef WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_ENUM
#define WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_ENUM
enum wp_linux_drm_syncobj_manager_v1_error {

  WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_SURFACE_EXISTS = 0,

  WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_INVALID_TIMELINE = 1,
};
#endif

#ifndef WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_ENUM_IS_VALID
#define WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_ENUM_IS_VALID

static inline bool
wp_linux_drm_syncobj_manager_v1_error_is_valid(uint32_t value,
                                               uint32_t version) {
  switch (value) {
  case WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_SURFACE_EXISTS:
    return version >= 1;
  case WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_INVALID_TIMELINE:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct wp_linux_drm_syncobj_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_surface)(struct wl_client *client, struct wl_resource *resource,
                      uint32_t id, struct wl_resource *surface);

  void (*import_timeline)(struct wl_client *client,
                          struct wl_resource *resource, uint32_t id,
                          int32_t fd);
};

#define WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_GET_SURFACE_SINCE_VERSION 1

#define WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_IMPORT_TIMELINE_SINCE_VERSION 1

struct wp_linux_drm_syncobj_timeline_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define WP_LINUX_DRM_SYNCOBJ_TIMELINE_V1_DESTROY_SINCE_VERSION 1

#ifndef WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_ENUM
#define WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_ENUM
enum wp_linux_drm_syncobj_surface_v1_error {

  WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_SURFACE = 1,

  WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_UNSUPPORTED_BUFFER = 2,

  WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_BUFFER = 3,

  WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_ACQUIRE_POINT = 4,

  WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_RELEASE_POINT = 5,

  WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_CONFLICTING_POINTS = 6,
};
#endif

#ifndef WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_ENUM_IS_VALID
#define WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_ENUM_IS_VALID

static inline bool
wp_linux_drm_syncobj_surface_v1_error_is_valid(uint32_t value,
                                               uint32_t version) {
  switch (value) {
  case WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_SURFACE:
    return version >= 1;
  case WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_UNSUPPORTED_BUFFER:
    return version >= 1;
  case WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_BUFFER:
    return version >= 1;
  case WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_ACQUIRE_POINT:
    return version >= 1;
  case WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_RELEASE_POINT:
    return version >= 1;
  case WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_CONFLICTING_POINTS:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct wp_linux_drm_syncobj_surface_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*set_acquire_point)(struct wl_client *client,
                            struct wl_resource *resource,
                            struct wl_resource *timeline, uint32_t point_hi,
                            uint32_t point_lo);

  void (*set_release_point)(struct wl_client *client,
                            struct wl_resource *resource,
                            struct wl_resource *timeline, uint32_t point_hi,
                            uint32_t point_lo);
};

#define WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_DESTROY_SINCE_VERSION 1

#define WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_SET_ACQUIRE_POINT_SINCE_VERSION 1

#define WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_SET_RELEASE_POINT_SINCE_VERSION 1

#ifdef __cplusplus
}
#endif

#endif
