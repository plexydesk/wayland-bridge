/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef XDG_FOREIGN_UNSTABLE_V2_SERVER_PROTOCOL_H
#define XDG_FOREIGN_UNSTABLE_V2_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_surface;
struct zxdg_exported_v2;
struct zxdg_exporter_v2;
struct zxdg_imported_v2;
struct zxdg_importer_v2;

#ifndef ZXDG_EXPORTER_V2_INTERFACE
#define ZXDG_EXPORTER_V2_INTERFACE

extern const struct wl_interface zxdg_exporter_v2_interface;
#endif
#ifndef ZXDG_IMPORTER_V2_INTERFACE
#define ZXDG_IMPORTER_V2_INTERFACE

extern const struct wl_interface zxdg_importer_v2_interface;
#endif
#ifndef ZXDG_EXPORTED_V2_INTERFACE
#define ZXDG_EXPORTED_V2_INTERFACE

extern const struct wl_interface zxdg_exported_v2_interface;
#endif
#ifndef ZXDG_IMPORTED_V2_INTERFACE
#define ZXDG_IMPORTED_V2_INTERFACE

extern const struct wl_interface zxdg_imported_v2_interface;
#endif

#ifndef ZXDG_EXPORTER_V2_ERROR_ENUM
#define ZXDG_EXPORTER_V2_ERROR_ENUM

enum zxdg_exporter_v2_error {

  ZXDG_EXPORTER_V2_ERROR_INVALID_SURFACE = 0,
};
#endif

#ifndef ZXDG_EXPORTER_V2_ERROR_ENUM_IS_VALID
#define ZXDG_EXPORTER_V2_ERROR_ENUM_IS_VALID

static inline bool zxdg_exporter_v2_error_is_valid(uint32_t value,
                                                   uint32_t version) {
  switch (value) {
  case ZXDG_EXPORTER_V2_ERROR_INVALID_SURFACE:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct zxdg_exporter_v2_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*export_toplevel)(struct wl_client *client,
                          struct wl_resource *resource, uint32_t id,
                          struct wl_resource *surface);
};

#define ZXDG_EXPORTER_V2_DESTROY_SINCE_VERSION 1

#define ZXDG_EXPORTER_V2_EXPORT_TOPLEVEL_SINCE_VERSION 1

struct zxdg_importer_v2_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*import_toplevel)(struct wl_client *client,
                          struct wl_resource *resource, uint32_t id,
                          const char *handle);
};

#define ZXDG_IMPORTER_V2_DESTROY_SINCE_VERSION 1

#define ZXDG_IMPORTER_V2_IMPORT_TOPLEVEL_SINCE_VERSION 1

struct zxdg_exported_v2_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define ZXDG_EXPORTED_V2_HANDLE 0

#define ZXDG_EXPORTED_V2_HANDLE_SINCE_VERSION 1

#define ZXDG_EXPORTED_V2_DESTROY_SINCE_VERSION 1

static inline void zxdg_exported_v2_send_handle(struct wl_resource *resource_,
                                                const char *handle) {
  wl_resource_post_event(resource_, ZXDG_EXPORTED_V2_HANDLE, handle);
}

#ifndef ZXDG_IMPORTED_V2_ERROR_ENUM
#define ZXDG_IMPORTED_V2_ERROR_ENUM

enum zxdg_imported_v2_error {

  ZXDG_IMPORTED_V2_ERROR_INVALID_SURFACE = 0,
};
#endif

#ifndef ZXDG_IMPORTED_V2_ERROR_ENUM_IS_VALID
#define ZXDG_IMPORTED_V2_ERROR_ENUM_IS_VALID

static inline bool zxdg_imported_v2_error_is_valid(uint32_t value,
                                                   uint32_t version) {
  switch (value) {
  case ZXDG_IMPORTED_V2_ERROR_INVALID_SURFACE:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct zxdg_imported_v2_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*set_parent_of)(struct wl_client *client, struct wl_resource *resource,
                        struct wl_resource *surface);
};

#define ZXDG_IMPORTED_V2_DESTROYED 0

#define ZXDG_IMPORTED_V2_DESTROYED_SINCE_VERSION 1

#define ZXDG_IMPORTED_V2_DESTROY_SINCE_VERSION 1

#define ZXDG_IMPORTED_V2_SET_PARENT_OF_SINCE_VERSION 1

static inline void
zxdg_imported_v2_send_destroyed(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, ZXDG_IMPORTED_V2_DESTROYED);
}

#ifdef __cplusplus
}
#endif

#endif
