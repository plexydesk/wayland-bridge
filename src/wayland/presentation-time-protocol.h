/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef PRESENTATION_TIME_SERVER_PROTOCOL_H
#define PRESENTATION_TIME_SERVER_PROTOCOL_H

#include "wayland-server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

struct wl_output;
struct wl_surface;
struct wp_presentation;
struct wp_presentation_feedback;

#ifndef WP_PRESENTATION_INTERFACE
#define WP_PRESENTATION_INTERFACE

extern const struct wl_interface wp_presentation_interface;
#endif
#ifndef WP_PRESENTATION_FEEDBACK_INTERFACE
#define WP_PRESENTATION_FEEDBACK_INTERFACE

extern const struct wl_interface wp_presentation_feedback_interface;
#endif

#ifndef WP_PRESENTATION_ERROR_ENUM
#define WP_PRESENTATION_ERROR_ENUM

enum wp_presentation_error {

  WP_PRESENTATION_ERROR_INVALID_TIMESTAMP = 0,

  WP_PRESENTATION_ERROR_INVALID_FLAG = 1,
};
#endif

#ifndef WP_PRESENTATION_ERROR_ENUM_IS_VALID
#define WP_PRESENTATION_ERROR_ENUM_IS_VALID

static inline bool wp_presentation_error_is_valid(uint32_t value,
                                                  uint32_t version) {
  switch (value) {
  case WP_PRESENTATION_ERROR_INVALID_TIMESTAMP:
    return version >= 1;
  case WP_PRESENTATION_ERROR_INVALID_FLAG:
    return version >= 1;
  default:
    return false;
  }
}
#endif

struct wp_presentation_interface {

  void (*destroy)(struct wl_client *client, struct wl_resource *resource);

  void (*feedback)(struct wl_client *client, struct wl_resource *resource,
                   struct wl_resource *surface, uint32_t callback);
};

#define WP_PRESENTATION_CLOCK_ID 0

#define WP_PRESENTATION_CLOCK_ID_SINCE_VERSION 1

#define WP_PRESENTATION_DESTROY_SINCE_VERSION 1

#define WP_PRESENTATION_FEEDBACK_SINCE_VERSION 1

static inline void wp_presentation_send_clock_id(struct wl_resource *resource_,
                                                 uint32_t clk_id) {
  wl_resource_post_event(resource_, WP_PRESENTATION_CLOCK_ID, clk_id);
}

#ifndef WP_PRESENTATION_FEEDBACK_KIND_ENUM
#define WP_PRESENTATION_FEEDBACK_KIND_ENUM

enum wp_presentation_feedback_kind {

  WP_PRESENTATION_FEEDBACK_KIND_VSYNC = 0x1,

  WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK = 0x2,

  WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION = 0x4,

  WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY = 0x8,
};
#endif

#ifndef WP_PRESENTATION_FEEDBACK_KIND_ENUM_IS_VALID
#define WP_PRESENTATION_FEEDBACK_KIND_ENUM_IS_VALID

static inline bool wp_presentation_feedback_kind_is_valid(uint32_t value,
                                                          uint32_t version) {
  uint32_t valid = 0;
  if (version >= 1)
    valid |= WP_PRESENTATION_FEEDBACK_KIND_VSYNC;
  if (version >= 1)
    valid |= WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK;
  if (version >= 1)
    valid |= WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION;
  if (version >= 1)
    valid |= WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY;
  return (value & ~valid) == 0;
}
#endif

#define WP_PRESENTATION_FEEDBACK_SYNC_OUTPUT 0
#define WP_PRESENTATION_FEEDBACK_PRESENTED 1
#define WP_PRESENTATION_FEEDBACK_DISCARDED 2

#define WP_PRESENTATION_FEEDBACK_SYNC_OUTPUT_SINCE_VERSION 1

#define WP_PRESENTATION_FEEDBACK_PRESENTED_SINCE_VERSION 1

#define WP_PRESENTATION_FEEDBACK_DISCARDED_SINCE_VERSION 1

static inline void
wp_presentation_feedback_send_sync_output(struct wl_resource *resource_,
                                          struct wl_resource *output) {
  wl_resource_post_event(resource_, WP_PRESENTATION_FEEDBACK_SYNC_OUTPUT,
                         output);
}

static inline void wp_presentation_feedback_send_presented(
    struct wl_resource *resource_, uint32_t tv_sec_hi, uint32_t tv_sec_lo,
    uint32_t tv_nsec, uint32_t refresh, uint32_t seq_hi, uint32_t seq_lo,
    uint32_t flags) {
  wl_resource_post_event(resource_, WP_PRESENTATION_FEEDBACK_PRESENTED,
                         tv_sec_hi, tv_sec_lo, tv_nsec, refresh, seq_hi, seq_lo,
                         flags);
}

static inline void
wp_presentation_feedback_send_discarded(struct wl_resource *resource_) {
  wl_resource_post_event(resource_, WP_PRESENTATION_FEEDBACK_DISCARDED);
}

#ifdef __cplusplus
}
#endif

#endif
