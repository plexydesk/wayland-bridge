/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef INPUT_METHOD_EXPERIMENTAL_V2_SERVER_PROTOCOL_H
#define INPUT_METHOD_EXPERIMENTAL_V2_SERVER_PROTOCOL_H

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
struct xx_input_method_manager_v2;
struct xx_input_method_v1;
struct xx_input_popup_positioner_v1;
struct xx_input_popup_surface_v2;

#ifndef XX_INPUT_METHOD_V1_INTERFACE
#define XX_INPUT_METHOD_V1_INTERFACE

extern const struct wl_interface xx_input_method_v1_interface;
#endif
#ifndef XX_INPUT_POPUP_SURFACE_V2_INTERFACE
#define XX_INPUT_POPUP_SURFACE_V2_INTERFACE

extern const struct wl_interface xx_input_popup_surface_v2_interface;
#endif
#ifndef XX_INPUT_POPUP_POSITIONER_V1_INTERFACE
#define XX_INPUT_POPUP_POSITIONER_V1_INTERFACE

extern const struct wl_interface xx_input_popup_positioner_v1_interface;
#endif
#ifndef XX_INPUT_METHOD_MANAGER_V2_INTERFACE
#define XX_INPUT_METHOD_MANAGER_V2_INTERFACE

extern const struct wl_interface xx_input_method_manager_v2_interface;
#endif

#ifndef XX_INPUT_METHOD_V1_ERROR_ENUM
#define XX_INPUT_METHOD_V1_ERROR_ENUM
enum xx_input_method_v1_error {

  XX_INPUT_METHOD_V1_ERROR_SURFACE_HAS_ROLE = 0x0,

  XX_INPUT_METHOD_V1_ERROR_INACTIVE = 0x1,
};
#endif

#ifndef XX_INPUT_METHOD_V1_ERROR_ENUM_IS_VALID
#define XX_INPUT_METHOD_V1_ERROR_ENUM_IS_VALID

static inline bool xx_input_method_v1_error_is_valid(uint32_t value,
                                                     uint32_t version) {
  switch (value) {
  case XX_INPUT_METHOD_V1_ERROR_SURFACE_HAS_ROLE:
    return version >= 1;
  case XX_INPUT_METHOD_V1_ERROR_INACTIVE:
    return version >= 1;
  default:
    return false;
  }
}
#endif

#ifndef XX_INPUT_METHOD_V1_PROTOCOL_COMPAT_ENUM
#define XX_INPUT_METHOD_V1_PROTOCOL_COMPAT_ENUM

enum xx_input_method_v1_protocol_compat {

  XX_INPUT_METHOD_V1_PROTOCOL_COMPAT_TEXT_INPUT_V3 = 0x0,

  XX_INPUT_METHOD_V1_PROTOCOL_COMPAT_XX_TEXT_INPUT = 0x1,
};
#endif

#ifndef XX_INPUT_METHOD_V1_PROTOCOL_COMPAT_ENUM_IS_VALID
#define XX_INPUT_METHOD_V1_PROTOCOL_COMPAT_ENUM_IS_VALID

static inline bool
xx_input_method_v1_protocol_compat_is_valid(uint32_t value, uint32_t version) {
  switch (value) {
  case XX_INPUT_METHOD_V1_PROTOCOL_COMPAT_TEXT_INPUT_V3:
    return version >= 1;
  case XX_INPUT_METHOD_V1_PROTOCOL_COMPAT_XX_TEXT_INPUT:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct xx_input_method_v1_interface {

  void (*perform_action)(struct wl_client *client, struct wl_resource *resource,
                         uint32_t action);

  void (*commit_string)(struct wl_client *client, struct wl_resource *resource,
                        const char *text);

  void (*set_preedit_string)(struct wl_client *client,
                             struct wl_resource *resource, const char *text,
                             int32_t cursor_begin, int32_t cursor_end);

  void (*delete_surrounding_text)(struct wl_client *client,
                                  struct wl_resource *resource,
                                  uint32_t before_length,
                                  uint32_t after_length);

  void (*move_cursor)(struct wl_client *client, struct wl_resource *resource,
                      int32_t cursor, int32_t anchor);

  void (*commit)(struct wl_client *client, struct wl_resource *resource,
                 uint32_t serial);

  void (*get_input_popup_surface)(struct wl_client *client,
                                  struct wl_resource *resource, uint32_t id,
                                  struct wl_resource *surface,
                                  struct wl_resource *positioner);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define XX_INPUT_METHOD_V1_ACTIVATE 0
#define XX_INPUT_METHOD_V1_DEACTIVATE 1
#define XX_INPUT_METHOD_V1_SURROUNDING_TEXT 2
#define XX_INPUT_METHOD_V1_TEXT_CHANGE_CAUSE 3
#define XX_INPUT_METHOD_V1_CONTENT_TYPE 4
#define XX_INPUT_METHOD_V1_SET_AVAILABLE_ACTIONS 5
#define XX_INPUT_METHOD_V1_ANNOUNCE_SUPPORTED_FEATURES 6
#define XX_INPUT_METHOD_V1_ANNOUNCE_PROTOCOL_COMPAT 7
#define XX_INPUT_METHOD_V1_DONE 8
#define XX_INPUT_METHOD_V1_UNAVAILABLE 9

#define XX_INPUT_METHOD_V1_ACTIVATE_SINCE_VERSION 1

#define XX_INPUT_METHOD_V1_DEACTIVATE_SINCE_VERSION 1

#define XX_INPUT_METHOD_V1_SURROUNDING_TEXT_SINCE_VERSION 1

#define XX_INPUT_METHOD_V1_TEXT_CHANGE_CAUSE_SINCE_VERSION 1

#define XX_INPUT_METHOD_V1_CONTENT_TYPE_SINCE_VERSION 1

#define XX_INPUT_METHOD_V1_SET_AVAILABLE_ACTIONS_SINCE_VERSION 3

#define XX_INPUT_METHOD_V1_ANNOUNCE_SUPPORTED_FEATURES_SINCE_VERSION 3

#define XX_INPUT_METHOD_V1_ANNOUNCE_PROTOCOL_COMPAT_SINCE_VERSION 3

#define XX_INPUT_METHOD_V1_DONE_SINCE_VERSION 1

#define XX_INPUT_METHOD_V1_UNAVAILABLE_SINCE_VERSION 1

#define XX_INPUT_METHOD_V1_PERFORM_ACTION_SINCE_VERSION 3

#define XX_INPUT_METHOD_V1_COMMIT_STRING_SINCE_VERSION 1

#define XX_INPUT_METHOD_V1_SET_PREEDIT_STRING_SINCE_VERSION 1

#define XX_INPUT_METHOD_V1_DELETE_SURROUNDING_TEXT_SINCE_VERSION 1

#define XX_INPUT_METHOD_V1_MOVE_CURSOR_SINCE_VERSION 3

#define XX_INPUT_METHOD_V1_COMMIT_SINCE_VERSION 1

#define XX_INPUT_METHOD_V1_GET_INPUT_POPUP_SURFACE_SINCE_VERSION 2

#define XX_INPUT_METHOD_V1_DESTROY_SINCE_VERSION 1

static inline void
xx_input_method_v1_send_activate(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, XX_INPUT_METHOD_V1_ACTIVATE);
}

static inline void
xx_input_method_v1_send_deactivate(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, XX_INPUT_METHOD_V1_DEACTIVATE);
}

static inline void
xx_input_method_v1_send_surrounding_text(struct wl_resource *resource_,
                                         const char *text, uint32_t cursor,
                                         uint32_t anchor) {
  wl_resource_post_event(resource_, XX_INPUT_METHOD_V1_SURROUNDING_TEXT, text,
                         cursor, anchor);
}

static inline void
xx_input_method_v1_send_text_change_cause(struct wl_resource *resource_,
                                          uint32_t cause) {
  wl_resource_post_event(resource_, XX_INPUT_METHOD_V1_TEXT_CHANGE_CAUSE,
                         cause);
}

static inline void
xx_input_method_v1_send_content_type(struct wl_resource *resource_,
                                     uint32_t hint, uint32_t purpose) {
  wl_resource_post_event(resource_, XX_INPUT_METHOD_V1_CONTENT_TYPE, hint,
                         purpose);
}

static inline void xx_input_method_v1_send_set_available_actions(
    struct wl_resource *resource_, struct wl_array *available_actions) {
  wl_resource_post_event(resource_, XX_INPUT_METHOD_V1_SET_AVAILABLE_ACTIONS,
                         available_actions);
}

static inline void xx_input_method_v1_send_announce_supported_features(
    struct wl_resource *resource_, uint32_t features) {
  wl_resource_post_event(
      resource_, XX_INPUT_METHOD_V1_ANNOUNCE_SUPPORTED_FEATURES, features);
}

static inline void
xx_input_method_v1_send_announce_protocol_compat(struct wl_resource *resource_,
                                                 uint32_t compat_level) {
  wl_resource_post_event(resource_, XX_INPUT_METHOD_V1_ANNOUNCE_PROTOCOL_COMPAT,
                         compat_level);
}

static inline void xx_input_method_v1_send_done(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, XX_INPUT_METHOD_V1_DONE);
}

static inline void
xx_input_method_v1_send_unavailable(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, XX_INPUT_METHOD_V1_UNAVAILABLE);
}

#ifndef XX_INPUT_POPUP_SURFACE_V2_ERROR_ENUM
#define XX_INPUT_POPUP_SURFACE_V2_ERROR_ENUM
enum xx_input_popup_surface_v2_error {

  XX_INPUT_POPUP_SURFACE_V2_ERROR_INVALID_SERIAL = 0,
};
#endif

#ifndef XX_INPUT_POPUP_SURFACE_V2_ERROR_ENUM_IS_VALID
#define XX_INPUT_POPUP_SURFACE_V2_ERROR_ENUM_IS_VALID

static inline bool xx_input_popup_surface_v2_error_is_valid(uint32_t value,
                                                            uint32_t version) {
  switch (value) {
  case XX_INPUT_POPUP_SURFACE_V2_ERROR_INVALID_SERIAL:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct xx_input_popup_surface_v2_interface {

  void (*ack_configure)(struct wl_client *client, struct wl_resource *resource,
                        uint32_t serial);

  void (*reposition)(struct wl_client *client, struct wl_resource *resource,
                     struct wl_resource *positioner, uint32_t token);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define XX_INPUT_POPUP_SURFACE_V2_START_CONFIGURE 0
#define XX_INPUT_POPUP_SURFACE_V2_REPOSITIONED 1

#define XX_INPUT_POPUP_SURFACE_V2_START_CONFIGURE_SINCE_VERSION 1

#define XX_INPUT_POPUP_SURFACE_V2_REPOSITIONED_SINCE_VERSION 1

#define XX_INPUT_POPUP_SURFACE_V2_ACK_CONFIGURE_SINCE_VERSION 1

#define XX_INPUT_POPUP_SURFACE_V2_REPOSITION_SINCE_VERSION 1

#define XX_INPUT_POPUP_SURFACE_V2_DESTROY_SINCE_VERSION 1

static inline void xx_input_popup_surface_v2_send_start_configure(
    struct wl_resource *resource_, uint32_t width, uint32_t height,
    int32_t anchor_x, int32_t anchor_y, uint32_t anchor_width,
    uint32_t anchor_height, uint32_t serial) {
  wl_resource_post_event(resource_, XX_INPUT_POPUP_SURFACE_V2_START_CONFIGURE,
                         width, height, anchor_x, anchor_y, anchor_width,
                         anchor_height, serial);
}

static inline void
xx_input_popup_surface_v2_send_repositioned(struct wl_resource *resource_,
                                            uint32_t token) {
  wl_resource_post_event(resource_, XX_INPUT_POPUP_SURFACE_V2_REPOSITIONED,
                         token);
}

#ifndef XX_INPUT_POPUP_POSITIONER_V1_ERROR_ENUM
#define XX_INPUT_POPUP_POSITIONER_V1_ERROR_ENUM
enum xx_input_popup_positioner_v1_error {

  XX_INPUT_POPUP_POSITIONER_V1_ERROR_INVALID_INPUT = 0,
};
#endif

#ifndef XX_INPUT_POPUP_POSITIONER_V1_ERROR_ENUM_IS_VALID
#define XX_INPUT_POPUP_POSITIONER_V1_ERROR_ENUM_IS_VALID

static inline bool
xx_input_popup_positioner_v1_error_is_valid(uint32_t value, uint32_t version) {
  switch (value) {
  case XX_INPUT_POPUP_POSITIONER_V1_ERROR_INVALID_INPUT:
    return version >= 1;
  default:
    return false;
  }
}
#endif

#ifndef XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_ENUM
#define XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_ENUM
enum xx_input_popup_positioner_v1_anchor {

  XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_NONE = 0,
  XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_TOP = 1,
  XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_BOTTOM = 2,
  XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_LEFT = 3,
  XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_RIGHT = 4,
  XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_TOP_LEFT = 5,
  XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_BOTTOM_LEFT = 6,
  XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_TOP_RIGHT = 7,
  XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_BOTTOM_RIGHT = 8,
};
#endif

#ifndef XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_ENUM_IS_VALID
#define XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_ENUM_IS_VALID

static inline bool
xx_input_popup_positioner_v1_anchor_is_valid(uint32_t value, uint32_t version) {
  switch (value) {
  case XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_NONE:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_TOP:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_BOTTOM:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_LEFT:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_RIGHT:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_TOP_LEFT:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_BOTTOM_LEFT:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_TOP_RIGHT:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_ANCHOR_BOTTOM_RIGHT:
    return version >= 1;
  default:
    return false;
  }
}
#endif

#ifndef XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_ENUM
#define XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_ENUM
enum xx_input_popup_positioner_v1_gravity {

  XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_NONE = 0,
  XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_TOP = 1,
  XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_BOTTOM = 2,
  XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_LEFT = 3,
  XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_RIGHT = 4,
  XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_TOP_LEFT = 5,
  XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_BOTTOM_LEFT = 6,
  XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_TOP_RIGHT = 7,
  XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_BOTTOM_RIGHT = 8,
};
#endif

#ifndef XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_ENUM_IS_VALID
#define XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_ENUM_IS_VALID

static inline bool
xx_input_popup_positioner_v1_gravity_is_valid(uint32_t value,
                                              uint32_t version) {
  switch (value) {
  case XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_NONE:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_TOP:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_BOTTOM:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_LEFT:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_RIGHT:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_TOP_LEFT:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_BOTTOM_LEFT:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_TOP_RIGHT:
    return version >= 1;
  case XX_INPUT_POPUP_POSITIONER_V1_GRAVITY_BOTTOM_RIGHT:
    return version >= 1;
  default:
    return false;
  }
}
#endif

#ifndef XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_ENUM
#define XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_ENUM

enum xx_input_popup_positioner_v1_constraint_adjustment {

  XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_NONE = 0,

  XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_SLIDE_X = 1,

  XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_SLIDE_Y = 2,

  XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_FLIP_X = 4,

  XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_FLIP_Y = 8,

  XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_RESIZE_X = 16,

  XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_RESIZE_Y = 32,
};
#endif

#ifndef XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_ENUM_IS_VALID
#define XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_ENUM_IS_VALID

static inline bool
xx_input_popup_positioner_v1_constraint_adjustment_is_valid(uint32_t value,
                                                            uint32_t version) {
  uint32_t valid = 0;
  if (version >= 1)
    valid |= XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_NONE;
  if (version >= 1)
    valid |= XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_SLIDE_X;
  if (version >= 1)
    valid |= XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
  if (version >= 1)
    valid |= XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_FLIP_X;
  if (version >= 1)
    valid |= XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_FLIP_Y;
  if (version >= 1)
    valid |= XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_RESIZE_X;
  if (version >= 1)
    valid |= XX_INPUT_POPUP_POSITIONER_V1_CONSTRAINT_ADJUSTMENT_RESIZE_Y;
  return (value & ~valid) == 0;
}
#endif

struct xx_input_popup_positioner_v1_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*set_size)(struct wl_client *client, struct wl_resource *resource,
                   uint32_t width, uint32_t height);

  void (*set_anchor)(struct wl_client *client, struct wl_resource *resource,
                     uint32_t anchor);

  void (*set_gravity)(struct wl_client *client, struct wl_resource *resource,
                      uint32_t gravity);

  void (*set_constraint_adjustment)(struct wl_client *client,
                                    struct wl_resource *resource,
                                    uint32_t constraint_adjustment);

  void (*set_offset)(struct wl_client *client, struct wl_resource *resource,
                     int32_t x, int32_t y);

  void (*set_reactive)(struct wl_client *client, struct wl_resource *resource);
};

#define XX_INPUT_POPUP_POSITIONER_V1_DESTROY_SINCE_VERSION 1

#define XX_INPUT_POPUP_POSITIONER_V1_SET_SIZE_SINCE_VERSION 1

#define XX_INPUT_POPUP_POSITIONER_V1_SET_ANCHOR_SINCE_VERSION 1

#define XX_INPUT_POPUP_POSITIONER_V1_SET_GRAVITY_SINCE_VERSION 1

#define XX_INPUT_POPUP_POSITIONER_V1_SET_CONSTRAINT_ADJUSTMENT_SINCE_VERSION 1

#define XX_INPUT_POPUP_POSITIONER_V1_SET_OFFSET_SINCE_VERSION 1

#define XX_INPUT_POPUP_POSITIONER_V1_SET_REACTIVE_SINCE_VERSION 1

struct xx_input_method_manager_v2_interface {

  void (*get_input_method)(struct wl_client *client,
                           struct wl_resource *resource,
                           struct wl_resource *seat, uint32_t input_method);

  void (*get_positioner)(struct wl_client *client, struct wl_resource *resource,
                         uint32_t id);

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

#define XX_INPUT_METHOD_MANAGER_V2_GET_INPUT_METHOD_SINCE_VERSION 1

#define XX_INPUT_METHOD_MANAGER_V2_GET_POSITIONER_SINCE_VERSION 1

#define XX_INPUT_METHOD_MANAGER_V2_DESTROY_SINCE_VERSION 1

#ifdef __cplusplus
}
#endif

#endif
