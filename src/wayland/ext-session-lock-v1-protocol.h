/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef EXT_SESSION_LOCK_V1_SERVER_PROTOCOL_H
#define EXT_SESSION_LOCK_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct ext_session_lock_manager_v1;
struct ext_session_lock_surface_v1;
struct ext_session_lock_v1;
struct wl_output;
struct wl_surface;

#ifndef EXT_SESSION_LOCK_MANAGER_V1_INTERFACE
#define EXT_SESSION_LOCK_MANAGER_V1_INTERFACE

extern const struct wl_interface ext_session_lock_manager_v1_interface;
#endif
#ifndef EXT_SESSION_LOCK_V1_INTERFACE
#define EXT_SESSION_LOCK_V1_INTERFACE

extern const struct wl_interface ext_session_lock_v1_interface;
#endif
#ifndef EXT_SESSION_LOCK_SURFACE_V1_INTERFACE
#define EXT_SESSION_LOCK_SURFACE_V1_INTERFACE

extern const struct wl_interface ext_session_lock_surface_v1_interface;
#endif

struct ext_session_lock_manager_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*lock)(struct wl_client *client, struct wl_resource *resource,
               uint32_t id);
};

#define EXT_SESSION_LOCK_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define EXT_SESSION_LOCK_MANAGER_V1_LOCK_SINCE_VERSION 1

#ifndef EXT_SESSION_LOCK_V1_ERROR_ENUM
#define EXT_SESSION_LOCK_V1_ERROR_ENUM
enum ext_session_lock_v1_error {

  EXT_SESSION_LOCK_V1_ERROR_INVALID_DESTROY = 0,

  EXT_SESSION_LOCK_V1_ERROR_INVALID_UNLOCK = 1,

  EXT_SESSION_LOCK_V1_ERROR_ROLE = 2,

  EXT_SESSION_LOCK_V1_ERROR_DUPLICATE_OUTPUT = 3,

  EXT_SESSION_LOCK_V1_ERROR_ALREADY_CONSTRUCTED = 4,
};
#endif

#ifndef EXT_SESSION_LOCK_V1_ERROR_ENUM_IS_VALID
#define EXT_SESSION_LOCK_V1_ERROR_ENUM_IS_VALID

static inline bool ext_session_lock_v1_error_is_valid(uint32_t value,
                                                      uint32_t version) {
  switch (value) {
  case EXT_SESSION_LOCK_V1_ERROR_INVALID_DESTROY:
    return version >= 1;
  case EXT_SESSION_LOCK_V1_ERROR_INVALID_UNLOCK:
    return version >= 1;
  case EXT_SESSION_LOCK_V1_ERROR_ROLE:
    return version >= 1;
  case EXT_SESSION_LOCK_V1_ERROR_DUPLICATE_OUTPUT:
    return version >= 1;
  case EXT_SESSION_LOCK_V1_ERROR_ALREADY_CONSTRUCTED:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct ext_session_lock_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_lock_surface)(struct wl_client *client,
                           struct wl_resource *resource, uint32_t id,
                           struct wl_resource *surface,
                           struct wl_resource *output);

  void (*unlock_and_destroy)(struct wl_client *client,
                             struct wl_resource *resource);
};

#define EXT_SESSION_LOCK_V1_LOCKED 0
#define EXT_SESSION_LOCK_V1_FINISHED 1

#define EXT_SESSION_LOCK_V1_LOCKED_SINCE_VERSION 1

#define EXT_SESSION_LOCK_V1_FINISHED_SINCE_VERSION 1

#define EXT_SESSION_LOCK_V1_DESTROY_SINCE_VERSION 1

#define EXT_SESSION_LOCK_V1_GET_LOCK_SURFACE_SINCE_VERSION 1

#define EXT_SESSION_LOCK_V1_UNLOCK_AND_DESTROY_SINCE_VERSION 1

static inline void
ext_session_lock_v1_send_locked(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, EXT_SESSION_LOCK_V1_LOCKED);
}

static inline void
ext_session_lock_v1_send_finished(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, EXT_SESSION_LOCK_V1_FINISHED);
}

#ifndef EXT_SESSION_LOCK_SURFACE_V1_ERROR_ENUM
#define EXT_SESSION_LOCK_SURFACE_V1_ERROR_ENUM
enum ext_session_lock_surface_v1_error {

  EXT_SESSION_LOCK_SURFACE_V1_ERROR_COMMIT_BEFORE_FIRST_ACK = 0,

  EXT_SESSION_LOCK_SURFACE_V1_ERROR_NULL_BUFFER = 1,

  EXT_SESSION_LOCK_SURFACE_V1_ERROR_DIMENSIONS_MISMATCH = 2,

  EXT_SESSION_LOCK_SURFACE_V1_ERROR_INVALID_SERIAL = 3,
};
#endif

#ifndef EXT_SESSION_LOCK_SURFACE_V1_ERROR_ENUM_IS_VALID
#define EXT_SESSION_LOCK_SURFACE_V1_ERROR_ENUM_IS_VALID

static inline bool
ext_session_lock_surface_v1_error_is_valid(uint32_t value, uint32_t version) {
  switch (value) {
  case EXT_SESSION_LOCK_SURFACE_V1_ERROR_COMMIT_BEFORE_FIRST_ACK:
    return version >= 1;
  case EXT_SESSION_LOCK_SURFACE_V1_ERROR_NULL_BUFFER:
    return version >= 1;
  case EXT_SESSION_LOCK_SURFACE_V1_ERROR_DIMENSIONS_MISMATCH:
    return version >= 1;
  case EXT_SESSION_LOCK_SURFACE_V1_ERROR_INVALID_SERIAL:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct ext_session_lock_surface_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*ack_configure)(struct wl_client *client, struct wl_resource *resource,
                        uint32_t serial);
};

#define EXT_SESSION_LOCK_SURFACE_V1_CONFIGURE 0

#define EXT_SESSION_LOCK_SURFACE_V1_CONFIGURE_SINCE_VERSION 1

#define EXT_SESSION_LOCK_SURFACE_V1_DESTROY_SINCE_VERSION 1

#define EXT_SESSION_LOCK_SURFACE_V1_ACK_CONFIGURE_SINCE_VERSION 1

static inline void
ext_session_lock_surface_v1_send_configure(struct wl_resource *resource_,
                                           uint32_t serial, uint32_t width,
                                           uint32_t height) {
  wl_resource_post_event(resource_, EXT_SESSION_LOCK_SURFACE_V1_CONFIGURE,
                         serial, width, height);
}

#ifdef __cplusplus
}
#endif

#endif
