/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef EXT_DATA_CONTROL_V1_SERVER_PROTOCOL_H
#define EXT_DATA_CONTROL_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct ext_data_control_device_v1;
struct ext_data_control_manager_v1;
struct ext_data_control_offer_v1;
struct ext_data_control_source_v1;
struct wl_seat;

#ifndef EXT_DATA_CONTROL_MANAGER_V1_INTERFACE
#define EXT_DATA_CONTROL_MANAGER_V1_INTERFACE

extern const struct wl_interface ext_data_control_manager_v1_interface;
#endif
#ifndef EXT_DATA_CONTROL_DEVICE_V1_INTERFACE
#define EXT_DATA_CONTROL_DEVICE_V1_INTERFACE

extern const struct wl_interface ext_data_control_device_v1_interface;
#endif
#ifndef EXT_DATA_CONTROL_SOURCE_V1_INTERFACE
#define EXT_DATA_CONTROL_SOURCE_V1_INTERFACE

extern const struct wl_interface ext_data_control_source_v1_interface;
#endif
#ifndef EXT_DATA_CONTROL_OFFER_V1_INTERFACE
#define EXT_DATA_CONTROL_OFFER_V1_INTERFACE

extern const struct wl_interface ext_data_control_offer_v1_interface;
#endif

struct ext_data_control_manager_v1_interface {

  void (*create_data_source)(struct wl_client *client,
                             struct wl_resource *resource, uint32_t id);

  void (*get_data_device)(struct wl_client *client,
                          struct wl_resource *resource, uint32_t id,
                          struct wl_resource *seat);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define EXT_DATA_CONTROL_MANAGER_V1_CREATE_DATA_SOURCE_SINCE_VERSION 1

#define EXT_DATA_CONTROL_MANAGER_V1_GET_DATA_DEVICE_SINCE_VERSION 1

#define EXT_DATA_CONTROL_MANAGER_V1_DESTROY_SINCE_VERSION 1

#ifndef EXT_DATA_CONTROL_DEVICE_V1_ERROR_ENUM
#define EXT_DATA_CONTROL_DEVICE_V1_ERROR_ENUM
enum ext_data_control_device_v1_error {

  EXT_DATA_CONTROL_DEVICE_V1_ERROR_USED_SOURCE = 1,
};

static inline bool ext_data_control_device_v1_error_is_valid(uint32_t value,
                                                             uint32_t version) {
  switch (value) {
  case EXT_DATA_CONTROL_DEVICE_V1_ERROR_USED_SOURCE:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct ext_data_control_device_v1_interface {

  void (*set_selection)(struct wl_client *client, struct wl_resource *resource,
                        struct wl_resource *source);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*set_primary_selection)(struct wl_client *client,
                                struct wl_resource *resource,
                                struct wl_resource *source);
};

#define EXT_DATA_CONTROL_DEVICE_V1_DATA_OFFER 0
#define EXT_DATA_CONTROL_DEVICE_V1_SELECTION 1
#define EXT_DATA_CONTROL_DEVICE_V1_FINISHED 2
#define EXT_DATA_CONTROL_DEVICE_V1_PRIMARY_SELECTION 3

#define EXT_DATA_CONTROL_DEVICE_V1_DATA_OFFER_SINCE_VERSION 1

#define EXT_DATA_CONTROL_DEVICE_V1_SELECTION_SINCE_VERSION 1

#define EXT_DATA_CONTROL_DEVICE_V1_FINISHED_SINCE_VERSION 1

#define EXT_DATA_CONTROL_DEVICE_V1_PRIMARY_SELECTION_SINCE_VERSION 1

#define EXT_DATA_CONTROL_DEVICE_V1_SET_SELECTION_SINCE_VERSION 1

#define EXT_DATA_CONTROL_DEVICE_V1_DESTROY_SINCE_VERSION 1

#define EXT_DATA_CONTROL_DEVICE_V1_SET_PRIMARY_SELECTION_SINCE_VERSION 1

static inline void
ext_data_control_device_v1_send_data_offer(struct wl_resource *resource_,
                                           struct wl_resource *id) {
  wl_resource_post_event(resource_, EXT_DATA_CONTROL_DEVICE_V1_DATA_OFFER, id);
}

static inline void
ext_data_control_device_v1_send_selection(struct wl_resource *resource_,
                                          struct wl_resource *id) {
  wl_resource_post_event(resource_, EXT_DATA_CONTROL_DEVICE_V1_SELECTION, id);
}

static inline void
ext_data_control_device_v1_send_finished(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, EXT_DATA_CONTROL_DEVICE_V1_FINISHED);
}

static inline void
ext_data_control_device_v1_send_primary_selection(struct wl_resource *resource_,
                                                  struct wl_resource *id) {
  wl_resource_post_event(resource_,
                         EXT_DATA_CONTROL_DEVICE_V1_PRIMARY_SELECTION, id);
}

#ifndef EXT_DATA_CONTROL_SOURCE_V1_ERROR_ENUM
#define EXT_DATA_CONTROL_SOURCE_V1_ERROR_ENUM
enum ext_data_control_source_v1_error {

  EXT_DATA_CONTROL_SOURCE_V1_ERROR_INVALID_OFFER = 1,
};

static inline bool ext_data_control_source_v1_error_is_valid(uint32_t value,
                                                             uint32_t version) {
  switch (value) {
  case EXT_DATA_CONTROL_SOURCE_V1_ERROR_INVALID_OFFER:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct ext_data_control_source_v1_interface {

  void (*offer)(struct wl_client *client, struct wl_resource *resource,
                const char *mime_type);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define EXT_DATA_CONTROL_SOURCE_V1_SEND 0
#define EXT_DATA_CONTROL_SOURCE_V1_CANCELLED 1

#define EXT_DATA_CONTROL_SOURCE_V1_SEND_SINCE_VERSION 1

#define EXT_DATA_CONTROL_SOURCE_V1_CANCELLED_SINCE_VERSION 1

#define EXT_DATA_CONTROL_SOURCE_V1_OFFER_SINCE_VERSION 1

#define EXT_DATA_CONTROL_SOURCE_V1_DESTROY_SINCE_VERSION 1

static inline void
ext_data_control_source_v1_send_send(struct wl_resource *resource_,
                                     const char *mime_type, int32_t fd) {
  wl_resource_post_event(resource_, EXT_DATA_CONTROL_SOURCE_V1_SEND, mime_type,
                         fd);
}

static inline void
ext_data_control_source_v1_send_cancelled(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, EXT_DATA_CONTROL_SOURCE_V1_CANCELLED);
}

struct ext_data_control_offer_v1_interface {

  void (*receive)(struct wl_client *client, struct wl_resource *resource,
                  const char *mime_type, int32_t fd);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define EXT_DATA_CONTROL_OFFER_V1_OFFER 0

#define EXT_DATA_CONTROL_OFFER_V1_OFFER_SINCE_VERSION 1

#define EXT_DATA_CONTROL_OFFER_V1_RECEIVE_SINCE_VERSION 1

#define EXT_DATA_CONTROL_OFFER_V1_DESTROY_SINCE_VERSION 1

static inline void
ext_data_control_offer_v1_send_offer(struct wl_resource *resource_,
                                     const char *mime_type) {
  wl_resource_post_event(resource_, EXT_DATA_CONTROL_OFFER_V1_OFFER, mime_type);
}

#ifdef __cplusplus
}
#endif

#endif
