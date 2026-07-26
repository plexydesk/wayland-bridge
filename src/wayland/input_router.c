/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <wayland-server-protocol.h>

#include "input_router.h"
#include "wayland_bridge.h"

extern struct wayland_bridge *bridge;

static uint32_t get_timestamp_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void send_frame(struct wl_resource *pointer) {
  if (!pointer)
    return;
  if (wl_resource_get_version(pointer) >= WL_POINTER_FRAME_SINCE_VERSION)
    wl_pointer_send_frame(pointer);
}

struct enter_ctx {
  struct wl_resource *surface_resource;
  uint32_t serial;
  wl_fixed_t x, y;
};

struct leave_ctx {
  struct wl_resource *surface_resource;
  uint32_t serial;
};

struct motion_ctx {
  uint32_t time;
  wl_fixed_t x, y;
};

struct button_ctx {
  uint32_t serial, time, button, state;
};

struct axis_ctx {
  uint32_t time, wl_axis;
  int32_t value, discrete;
};

static void cb_enter(struct wl_resource *ptr, void *data) {
  struct enter_ctx *ctx = data;
  wl_pointer_send_enter(ptr, ctx->serial, ctx->surface_resource, ctx->x,
                        ctx->y);
  send_frame(ptr);
}

static void cb_leave(struct wl_resource *ptr, void *data) {
  struct leave_ctx *ctx = data;
  wl_pointer_send_leave(ptr, ctx->serial, ctx->surface_resource);
  send_frame(ptr);
}

static void cb_motion(struct wl_resource *ptr, void *data) {
  struct motion_ctx *ctx = data;
  wl_pointer_send_motion(ptr, ctx->time, ctx->x, ctx->y);
  send_frame(ptr);
}

static void cb_button(struct wl_resource *ptr, void *data) {
  struct button_ctx *ctx = data;
  wl_pointer_send_button(ptr, ctx->serial, ctx->time, ctx->button, ctx->state);
  send_frame(ptr);
}

static void cb_axis(struct wl_resource *ptr, void *data) {
  struct axis_ctx *ctx = data;
  int ver = wl_resource_get_version(ptr);

  if (ver >= WL_POINTER_AXIS_SOURCE_SINCE_VERSION)
    wl_pointer_send_axis_source(ptr, WL_POINTER_AXIS_SOURCE_WHEEL);

  if (ctx->value != 0) {
    wl_pointer_send_axis(ptr, ctx->time, ctx->wl_axis, (wl_fixed_t)ctx->value);
    if (ver >= WL_POINTER_AXIS_VALUE120_SINCE_VERSION && ctx->discrete != 0)
      wl_pointer_send_axis_value120(ptr, ctx->wl_axis, ctx->discrete * 120);
    else if (ver >= WL_POINTER_AXIS_DISCRETE_SINCE_VERSION &&
             ctx->discrete != 0)
      wl_pointer_send_axis_discrete(ptr, ctx->wl_axis, ctx->discrete);
  } else {
    if (ver >= WL_POINTER_AXIS_STOP_SINCE_VERSION)
      wl_pointer_send_axis_stop(ptr, ctx->time, ctx->wl_axis);
  }
  send_frame(ptr);
}

void input_router_pointer_enter(struct bridge_surface *surface, int32_t x,
                                int32_t y) {
  if (!bridge || !surface)
    return;
  if (surface->is_x11)
    return;

  if (!surface->resource) {
    surface->pending_enter = true;
    surface->pending_enter_x = x;
    surface->pending_enter_y = y;
    return;
  }

  struct wl_resource *any_ptr = bridge_get_pointer_for_surface(surface);
  if (!any_ptr) {
    surface->pending_enter = true;
    surface->pending_enter_x = x;
    surface->pending_enter_y = y;
    return;
  }
  surface->pending_enter = false;

  LOG_TRACE("input_router: ENTER surface=%p is_x11=%d at (%d,%d)",
            (void *)surface, surface->is_x11, x, y);

  if (bridge->pointer_focus && bridge->pointer_focus != surface &&
      bridge->pointer_focus->resource) {
    struct leave_ctx lctx = {
        .surface_resource = bridge->pointer_focus->resource,
        .serial = wl_display_next_serial(bridge->display),
    };
    bridge_for_each_pointer(bridge->pointer_focus, cb_leave, &lctx);
  }

  struct enter_ctx ectx = {
      .surface_resource = surface->resource,
      .serial = wl_display_next_serial(bridge->display),
      .x = wl_fixed_from_int(x),
      .y = wl_fixed_from_int(y),
  };
  bridge_for_each_pointer(surface, cb_enter, &ectx);

  bridge->seat_pointer_resource = any_ptr;
  bridge->seat_pointer_surface = surface->resource;
  bridge->pointer_focus = surface;
  surface->has_pointer_position = true;
  surface->pointer_x = x;
  surface->pointer_y = y;
  bridge_pointer_constraints_set_focus(surface, true);
}

void input_router_pointer_leave(struct bridge_surface *surface) {
  if (!bridge || !surface)
    return;
  if (surface->is_x11)
    return;

  if (bridge_pointer_constraints_is_locked(surface)) {
    return;
  }

  surface->has_pointer_position = false;
  surface->pending_enter = false;
  bridge_pointer_constraints_set_focus(surface, false);

  if (bridge->pointer_focus == surface && surface->resource) {
    struct leave_ctx lctx = {
        .surface_resource = surface->resource,
        .serial = wl_display_next_serial(bridge->display),
    };
    bridge_for_each_pointer(surface, cb_leave, &lctx);
    bridge->seat_pointer_resource = NULL;
    bridge->seat_pointer_surface = NULL;
    bridge->pointer_focus = NULL;
  }
}

static void try_deferred_enter(struct bridge_surface *surface) {
  if (!surface->pending_enter)
    return;
  input_router_pointer_enter(surface, surface->pending_enter_x,
                             surface->pending_enter_y);
}

void input_router_pointer_motion(struct bridge_surface *surface, int32_t x,
                                 int32_t y) {
  if (!bridge || !surface)
    return;
  if (surface->is_x11)
    return;

  int32_t dx = surface->has_pointer_position ? (x - surface->pointer_x) : 0;
  int32_t dy = surface->has_pointer_position ? (y - surface->pointer_y) : 0;
  surface->pointer_x = x;
  surface->pointer_y = y;
  surface->has_pointer_position = true;

  if (bridge->pointer_focus != surface) {
    try_deferred_enter(surface);
    if (bridge->pointer_focus != surface) {
      LOG_TRACE("input_router: MOTION dropped - no focus (surface=%p)",
                (void *)surface);
      return;
    }
  }

  if (bridge_pointer_constraints_is_locked(surface)) {
    if (!bridge->raw_input_active)
      bridge_relative_pointer_send_motion(surface, (double)dx, (double)dy);
    return;
  }

  struct motion_ctx mctx = {
      .time = get_timestamp_ms(),
      .x = wl_fixed_from_int(x),
      .y = wl_fixed_from_int(y),
  };
  bridge_for_each_pointer(surface, cb_motion, &mctx);
  bridge_relative_pointer_send_motion(surface, (double)dx, (double)dy);
  bridge_data_device_pointer_motion(surface, x, y);
}

void input_router_pointer_button(struct bridge_surface *surface,
                                 uint32_t button, bool pressed, int32_t x,
                                 int32_t y) {
  if (!bridge || !surface)
    return;
  if (surface->is_x11)
    return;

  if (bridge->pointer_focus != surface) {
    try_deferred_enter(surface);
    if (bridge->pointer_focus != surface)
      return;
  }

  struct button_ctx bctx = {
      .serial = wl_display_next_serial(bridge->display),
      .time = get_timestamp_ms(),
      .button = button,
      .state = pressed ? WL_POINTER_BUTTON_STATE_PRESSED
                       : WL_POINTER_BUTTON_STATE_RELEASED,
  };
  bridge_for_each_pointer(surface, cb_button, &bctx);
  bridge_data_device_pointer_button(surface, pressed, x, y);
}

void input_router_pointer_axis(struct bridge_surface *surface, int32_t axis,
                               int32_t value, int32_t discrete) {
  if (!bridge || !surface)
    return;
  if (surface->is_x11)
    return;

  if (bridge->pointer_focus != surface)
    return;

  struct axis_ctx actx = {
      .time = get_timestamp_ms(),
      .wl_axis = (axis == 0) ? WL_POINTER_AXIS_VERTICAL_SCROLL
                             : WL_POINTER_AXIS_HORIZONTAL_SCROLL,
      .value = value,
      .discrete = discrete,
  };
  bridge_for_each_pointer(surface, cb_axis, &actx);
}

void input_router_clear_focus(struct bridge_surface *surface) {
  if (!bridge || !surface)
    return;
  if (bridge->pointer_focus == surface)
    input_router_pointer_leave(surface);
}
