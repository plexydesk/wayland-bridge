/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef XDG_ACTIVATION_V1_SERVER_PROTOCOL_H
#define XDG_ACTIVATION_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_seat;
struct wl_surface;
struct xdg_activation_token_v1;
struct xdg_activation_v1;

#ifndef XDG_ACTIVATION_V1_INTERFACE
#define XDG_ACTIVATION_V1_INTERFACE

extern const struct wl_interface xdg_activation_v1_interface;
#endif
#ifndef XDG_ACTIVATION_TOKEN_V1_INTERFACE
#define XDG_ACTIVATION_TOKEN_V1_INTERFACE

extern const struct wl_interface xdg_activation_token_v1_interface;
#endif

struct xdg_activation_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_activation_token)(struct wl_client *client,
                               struct wl_resource *resource, uint32_t id);

  void (*activate)(struct wl_client *client, struct wl_resource *resource,
                   const char *token, struct wl_resource *surface);
};

#define XDG_ACTIVATION_V1_DESTROY_SINCE_VERSION 1

#define XDG_ACTIVATION_V1_GET_ACTIVATION_TOKEN_SINCE_VERSION 1

#define XDG_ACTIVATION_V1_ACTIVATE_SINCE_VERSION 1

#ifndef XDG_ACTIVATION_TOKEN_V1_ERROR_ENUM
#define XDG_ACTIVATION_TOKEN_V1_ERROR_ENUM
enum xdg_activation_token_v1_error {

  XDG_ACTIVATION_TOKEN_V1_ERROR_ALREADY_USED = 0,
};
#endif

#ifndef XDG_ACTIVATION_TOKEN_V1_ERROR_ENUM_IS_VALID
#define XDG_ACTIVATION_TOKEN_V1_ERROR_ENUM_IS_VALID

static inline bool xdg_activation_token_v1_error_is_valid(uint32_t value,
                                                          uint32_t version) {
  switch (value) {
  case XDG_ACTIVATION_TOKEN_V1_ERROR_ALREADY_USED:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct xdg_activation_token_v1_interface {

  void (*set_serial)(struct wl_client *client, struct wl_resource *resource,
                     uint32_t serial, struct wl_resource *seat);

  void (*set_app_id)(struct wl_client *client, struct wl_resource *resource,
                     const char *app_id);

  void (*set_surface)(struct wl_client *client, struct wl_resource *resource,
                      struct wl_resource *surface);

  void (*commit)(struct wl_client *client, struct wl_resource *resource);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define XDG_ACTIVATION_TOKEN_V1_DONE 0

#define XDG_ACTIVATION_TOKEN_V1_DONE_SINCE_VERSION 1

#define XDG_ACTIVATION_TOKEN_V1_SET_SERIAL_SINCE_VERSION 1

#define XDG_ACTIVATION_TOKEN_V1_SET_APP_ID_SINCE_VERSION 1

#define XDG_ACTIVATION_TOKEN_V1_SET_SURFACE_SINCE_VERSION 1

#define XDG_ACTIVATION_TOKEN_V1_COMMIT_SINCE_VERSION 1

#define XDG_ACTIVATION_TOKEN_V1_DESTROY_SINCE_VERSION 1

static inline void
xdg_activation_token_v1_send_done(struct wl_resource *resource_,
                                  const char *token) {
  wl_resource_post_event(resource_, XDG_ACTIVATION_TOKEN_V1_DONE, token);
}

#ifdef __cplusplus
}
#endif

#endif
