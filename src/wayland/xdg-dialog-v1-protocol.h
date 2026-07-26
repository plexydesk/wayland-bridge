/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef XDG_DIALOG_V1_SERVER_PROTOCOL_H
#define XDG_DIALOG_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct xdg_dialog_v1;
struct xdg_toplevel;
struct xdg_wm_dialog_v1;

#ifndef XDG_WM_DIALOG_V1_INTERFACE
#define XDG_WM_DIALOG_V1_INTERFACE

extern const struct wl_interface xdg_wm_dialog_v1_interface;
#endif
#ifndef XDG_DIALOG_V1_INTERFACE
#define XDG_DIALOG_V1_INTERFACE

extern const struct wl_interface xdg_dialog_v1_interface;
#endif

#ifndef XDG_WM_DIALOG_V1_ERROR_ENUM
#define XDG_WM_DIALOG_V1_ERROR_ENUM
enum xdg_wm_dialog_v1_error {

  XDG_WM_DIALOG_V1_ERROR_ALREADY_USED = 0,
};
#endif

#ifndef XDG_WM_DIALOG_V1_ERROR_ENUM_IS_VALID
#define XDG_WM_DIALOG_V1_ERROR_ENUM_IS_VALID

static inline bool xdg_wm_dialog_v1_error_is_valid(uint32_t value,
                                                   uint32_t version) {
  switch (value) {
  case XDG_WM_DIALOG_V1_ERROR_ALREADY_USED:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct xdg_wm_dialog_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*get_xdg_dialog)(struct wl_client *client, struct wl_resource *resource,
                         uint32_t id, struct wl_resource *toplevel);
};

#define XDG_WM_DIALOG_V1_DESTROY_SINCE_VERSION 1

#define XDG_WM_DIALOG_V1_GET_XDG_DIALOG_SINCE_VERSION 1

struct xdg_dialog_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*set_modal)(struct wl_client *client, struct wl_resource *resource);

  void (*unset_modal)(struct wl_client *client, struct wl_resource *resource);
};

#define XDG_DIALOG_V1_DESTROY_SINCE_VERSION 1

#define XDG_DIALOG_V1_SET_MODAL_SINCE_VERSION 1

#define XDG_DIALOG_V1_UNSET_MODAL_SINCE_VERSION 1

#ifdef __cplusplus
}
#endif

#endif
