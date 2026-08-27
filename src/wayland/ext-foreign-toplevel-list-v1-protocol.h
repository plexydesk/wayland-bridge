/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef EXT_FOREIGN_TOPLEVEL_LIST_V1_SERVER_PROTOCOL_H
#define EXT_FOREIGN_TOPLEVEL_LIST_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct ext_foreign_toplevel_handle_v1;
struct ext_foreign_toplevel_list_v1;

#ifndef EXT_FOREIGN_TOPLEVEL_LIST_V1_INTERFACE
#define EXT_FOREIGN_TOPLEVEL_LIST_V1_INTERFACE

extern const struct wl_interface ext_foreign_toplevel_list_v1_interface;
#endif
#ifndef EXT_FOREIGN_TOPLEVEL_HANDLE_V1_INTERFACE
#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_INTERFACE

extern const struct wl_interface ext_foreign_toplevel_handle_v1_interface;
#endif

struct ext_foreign_toplevel_list_v1_interface {

  void (*stop)(struct wl_client *client, struct wl_resource *resource);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define EXT_FOREIGN_TOPLEVEL_LIST_V1_TOPLEVEL 0
#define EXT_FOREIGN_TOPLEVEL_LIST_V1_FINISHED 1

#define EXT_FOREIGN_TOPLEVEL_LIST_V1_TOPLEVEL_SINCE_VERSION 1

#define EXT_FOREIGN_TOPLEVEL_LIST_V1_FINISHED_SINCE_VERSION 1

#define EXT_FOREIGN_TOPLEVEL_LIST_V1_STOP_SINCE_VERSION 1

#define EXT_FOREIGN_TOPLEVEL_LIST_V1_DESTROY_SINCE_VERSION 1

static inline void
ext_foreign_toplevel_list_v1_send_toplevel(struct wl_resource *resource_,
                                           struct wl_resource *toplevel) {
  wl_resource_post_event(resource_, EXT_FOREIGN_TOPLEVEL_LIST_V1_TOPLEVEL,
                         toplevel);
}

static inline void
ext_foreign_toplevel_list_v1_send_finished(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, EXT_FOREIGN_TOPLEVEL_LIST_V1_FINISHED);
}

struct ext_foreign_toplevel_handle_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_CLOSED 0
#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_DONE 1
#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_TITLE 2
#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_APP_ID 3
#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_IDENTIFIER 4

#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_CLOSED_SINCE_VERSION 1

#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_DONE_SINCE_VERSION 1

#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_TITLE_SINCE_VERSION 1

#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_APP_ID_SINCE_VERSION 1

#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_IDENTIFIER_SINCE_VERSION 1

#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_DESTROY_SINCE_VERSION 1

static inline void
ext_foreign_toplevel_handle_v1_send_closed(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, EXT_FOREIGN_TOPLEVEL_HANDLE_V1_CLOSED);
}

static inline void
ext_foreign_toplevel_handle_v1_send_done(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, EXT_FOREIGN_TOPLEVEL_HANDLE_V1_DONE);
}

static inline void
ext_foreign_toplevel_handle_v1_send_title(struct wl_resource *resource_,
                                          const char *title) {
  wl_resource_post_event(resource_, EXT_FOREIGN_TOPLEVEL_HANDLE_V1_TITLE,
                         title);
}

static inline void
ext_foreign_toplevel_handle_v1_send_app_id(struct wl_resource *resource_,
                                           const char *app_id) {
  wl_resource_post_event(resource_, EXT_FOREIGN_TOPLEVEL_HANDLE_V1_APP_ID,
                         app_id);
}

static inline void
ext_foreign_toplevel_handle_v1_send_identifier(struct wl_resource *resource_,
                                               const char *identifier) {
  wl_resource_post_event(resource_, EXT_FOREIGN_TOPLEVEL_HANDLE_V1_IDENTIFIER,
                         identifier);
}

#ifdef __cplusplus
}
#endif

#endif
