/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef WP_PRIMARY_SELECTION_UNSTABLE_V1_SERVER_PROTOCOL_H
#define WP_PRIMARY_SELECTION_UNSTABLE_V1_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_seat;
struct zwp_primary_selection_device_manager_v1;
struct zwp_primary_selection_device_v1;
struct zwp_primary_selection_offer_v1;
struct zwp_primary_selection_source_v1;

#ifndef ZWP_PRIMARY_SELECTION_DEVICE_MANAGER_V1_INTERFACE
#define ZWP_PRIMARY_SELECTION_DEVICE_MANAGER_V1_INTERFACE

extern const struct wl_interface
    zwp_primary_selection_device_manager_v1_interface;
#endif
#ifndef ZWP_PRIMARY_SELECTION_DEVICE_V1_INTERFACE
#define ZWP_PRIMARY_SELECTION_DEVICE_V1_INTERFACE

extern const struct wl_interface zwp_primary_selection_device_v1_interface;
#endif
#ifndef ZWP_PRIMARY_SELECTION_OFFER_V1_INTERFACE
#define ZWP_PRIMARY_SELECTION_OFFER_V1_INTERFACE

extern const struct wl_interface zwp_primary_selection_offer_v1_interface;
#endif
#ifndef ZWP_PRIMARY_SELECTION_SOURCE_V1_INTERFACE
#define ZWP_PRIMARY_SELECTION_SOURCE_V1_INTERFACE

extern const struct wl_interface zwp_primary_selection_source_v1_interface;
#endif

struct zwp_primary_selection_device_manager_v1_interface {

  void (*create_source)(struct wl_client *client, struct wl_resource *resource,
                        uint32_t id);

  void (*get_device)(struct wl_client *client, struct wl_resource *resource,
                     uint32_t id, struct wl_resource *seat);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define ZWP_PRIMARY_SELECTION_DEVICE_MANAGER_V1_CREATE_SOURCE_SINCE_VERSION 1

#define ZWP_PRIMARY_SELECTION_DEVICE_MANAGER_V1_GET_DEVICE_SINCE_VERSION 1

#define ZWP_PRIMARY_SELECTION_DEVICE_MANAGER_V1_DESTROY_SINCE_VERSION 1

struct zwp_primary_selection_device_v1_interface {

  void (*set_selection)(struct wl_client *client, struct wl_resource *resource,
                        struct wl_resource *source, uint32_t serial);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define ZWP_PRIMARY_SELECTION_DEVICE_V1_DATA_OFFER 0
#define ZWP_PRIMARY_SELECTION_DEVICE_V1_SELECTION 1

#define ZWP_PRIMARY_SELECTION_DEVICE_V1_DATA_OFFER_SINCE_VERSION 1

#define ZWP_PRIMARY_SELECTION_DEVICE_V1_SELECTION_SINCE_VERSION 1

#define ZWP_PRIMARY_SELECTION_DEVICE_V1_SET_SELECTION_SINCE_VERSION 1

#define ZWP_PRIMARY_SELECTION_DEVICE_V1_DESTROY_SINCE_VERSION 1

static inline void
zwp_primary_selection_device_v1_send_data_offer(struct wl_resource *resource_,
                                                struct wl_resource *offer) {
  wl_resource_post_event(resource_, ZWP_PRIMARY_SELECTION_DEVICE_V1_DATA_OFFER,
                         offer);
}

static inline void
zwp_primary_selection_device_v1_send_selection(struct wl_resource *resource_,
                                               struct wl_resource *id) {
  wl_resource_post_event(resource_, ZWP_PRIMARY_SELECTION_DEVICE_V1_SELECTION,
                         id);
}

struct zwp_primary_selection_offer_v1_interface {

  void (*receive)(struct wl_client *client, struct wl_resource *resource,
                  const char *mime_type, int32_t fd);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define ZWP_PRIMARY_SELECTION_OFFER_V1_OFFER 0

#define ZWP_PRIMARY_SELECTION_OFFER_V1_OFFER_SINCE_VERSION 1

#define ZWP_PRIMARY_SELECTION_OFFER_V1_RECEIVE_SINCE_VERSION 1

#define ZWP_PRIMARY_SELECTION_OFFER_V1_DESTROY_SINCE_VERSION 1

static inline void
zwp_primary_selection_offer_v1_send_offer(struct wl_resource *resource_,
                                          const char *mime_type) {
  wl_resource_post_event(resource_, ZWP_PRIMARY_SELECTION_OFFER_V1_OFFER,
                         mime_type);
}

struct zwp_primary_selection_source_v1_interface {

  void (*offer)(struct wl_client *client, struct wl_resource *resource,
                const char *mime_type);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define ZWP_PRIMARY_SELECTION_SOURCE_V1_SEND 0
#define ZWP_PRIMARY_SELECTION_SOURCE_V1_CANCELLED 1

#define ZWP_PRIMARY_SELECTION_SOURCE_V1_SEND_SINCE_VERSION 1

#define ZWP_PRIMARY_SELECTION_SOURCE_V1_CANCELLED_SINCE_VERSION 1

#define ZWP_PRIMARY_SELECTION_SOURCE_V1_OFFER_SINCE_VERSION 1

#define ZWP_PRIMARY_SELECTION_SOURCE_V1_DESTROY_SINCE_VERSION 1

static inline void
zwp_primary_selection_source_v1_send_send(struct wl_resource *resource_,
                                          const char *mime_type, int32_t fd) {
  wl_resource_post_event(resource_, ZWP_PRIMARY_SELECTION_SOURCE_V1_SEND,
                         mime_type, fd);
}

static inline void
zwp_primary_selection_source_v1_send_cancelled(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, ZWP_PRIMARY_SELECTION_SOURCE_V1_CANCELLED);
}

#ifdef __cplusplus
}
#endif

#endif
