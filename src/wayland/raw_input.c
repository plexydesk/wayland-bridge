/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#define _POSIX_C_SOURCE 200809L
#include "input_router.h"
#include "wayland_bridge.h"
#include "xwm.h"
#include <linux/input-event-codes.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

static uint32_t bridge_time_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void send_pointer_frame(struct wl_resource *ptr) {
  if (ptr && wl_resource_get_version(ptr) >= WL_POINTER_FRAME_SINCE_VERSION)
    wl_pointer_send_frame(ptr);
}

struct raw_leave_ctx {
  struct wl_resource *surface_resource;
  uint32_t serial;
};

static void cb_raw_leave(struct wl_resource *ptr, void *data) {
  struct raw_leave_ctx *ctx = data;
  wl_pointer_send_leave(ptr, ctx->serial, ctx->surface_resource);
  send_pointer_frame(ptr);
}

struct raw_enter_ctx {
  struct wl_resource *surface_resource;
  uint32_t serial;
  wl_fixed_t x;
  wl_fixed_t y;
};

static void cb_raw_enter(struct wl_resource *ptr, void *data) {
  struct raw_enter_ctx *ctx = data;
  wl_pointer_send_enter(ptr, ctx->serial, ctx->surface_resource, ctx->x,
                        ctx->y);
  send_pointer_frame(ptr);
}

struct raw_motion_ctx {
  uint32_t time;
  wl_fixed_t x;
  wl_fixed_t y;
};

static void cb_raw_motion(struct wl_resource *ptr, void *data) {
  struct raw_motion_ctx *ctx = data;
  wl_pointer_send_motion(ptr, ctx->time, ctx->x, ctx->y);
  send_pointer_frame(ptr);
}

struct raw_button_ctx {
  uint32_t serial;
  uint32_t time;
  uint32_t button;
  uint32_t state;
};

static void cb_raw_button(struct wl_resource *ptr, void *data) {
  struct raw_button_ctx *ctx = data;
  wl_pointer_send_button(ptr, ctx->serial, ctx->time, ctx->button, ctx->state);
  send_pointer_frame(ptr);
}

struct raw_axis_ctx {
  uint32_t time;
  uint32_t axis;
  wl_fixed_t value;
  int32_t discrete;
};

static void cb_raw_frame(struct wl_resource *ptr, void *data) {
  (void)data;
  send_pointer_frame(ptr);
}

static void cb_raw_axis(struct wl_resource *ptr, void *data) {
  struct raw_axis_ctx *ctx = data;
  wl_pointer_send_axis(ptr, ctx->time, ctx->axis, ctx->value);

  int version = wl_resource_get_version(ptr);
  if (version >= WL_POINTER_AXIS_DISCRETE_SINCE_VERSION && ctx->discrete != 0) {
    wl_pointer_send_axis_discrete(ptr, ctx->axis, ctx->discrete);
  }
  send_pointer_frame(ptr);
}

static int send_pointer_enter_all(struct bridge_surface *surface,
                                  int32_t local_x, int32_t local_y) {
  if (!surface || !surface->resource) {
    return 0;
  }

  struct wl_resource *any_ptr = bridge_get_pointer_for_surface(surface);
  if (!any_ptr) {
    return 0;
  }

  if (bridge->seat_pointer_surface &&
      bridge->seat_pointer_surface != surface->resource) {
    struct bridge_surface *old_surf = NULL;
    struct bridge_surface *s;
    wl_list_for_each(s, &bridge->surfaces, link) {
      if (s->resource == bridge->seat_pointer_surface) {
        old_surf = s;
        break;
      }
    }
    if (old_surf) {
      struct raw_leave_ctx lctx = {
          .surface_resource = old_surf->resource,
          .serial = wl_display_next_serial(bridge->display),
      };
      bridge_for_each_pointer(old_surf, cb_raw_leave, &lctx);
    }
  }

  float scale = 1.0f;
  PlexyOutputInfo out_info;
  if (surface->current_output_id &&
      bridge_get_output_info(surface->current_output_id, &out_info) &&
      out_info.scale_factor > 0.0f) {
    scale = out_info.scale_factor;
  } else if (surface->entered_output_count > 0 &&
             bridge_get_output_info(
                 surface->entered_output_ids[surface->entered_output_count - 1],
                 &out_info) &&
             out_info.scale_factor > 0.0f) {
    scale = out_info.scale_factor;
  } else if (bridge_get_default_output_info(&out_info) &&
             out_info.scale_factor > 0.0f) {
    scale = out_info.scale_factor;
  }
  int32_t logical_x = (int32_t)((float)local_x / scale);
  int32_t logical_y = (int32_t)((float)local_y / scale);

  struct raw_enter_ctx ectx = {
      .surface_resource = surface->resource,
      .serial = wl_display_next_serial(bridge->display),
      .x = wl_fixed_from_int(logical_x),
      .y = wl_fixed_from_int(logical_y),
  };
  int count = bridge_for_each_pointer(surface, cb_raw_enter, &ectx);
  if (count <= 0) {
    return 0;
  }

  bridge->seat_pointer_resource = any_ptr;
  bridge->seat_pointer_surface = surface->resource;
  surface->pending_enter = false;
  surface->has_pointer_position = true;
  surface->pointer_x = logical_x;
  surface->pointer_y = logical_y;

  LOG_DEBUG("raw_input: enter surface wid=%u phys=(%d,%d) logical=(%d,%d) "
            "scale=%.2f resources=%d",
            surface->plexy_window_id, local_x, local_y, logical_x, logical_y,
            scale, count);
  return count;
}

static struct bridge_surface *hit_test(int32_t sx, int32_t sy, int32_t *local_x,
                                       int32_t *local_y) {
  struct bridge_surface *best = NULL;
  struct bridge_surface *s;

  wl_list_for_each_reverse(s, &bridge->surfaces, link) {
    if (!s->screen_pos_valid || !s->plexy_window || !s->resource)
      continue;
    if (!s->configured)
      continue;

    int32_t lx = sx - s->screen_x;
    int32_t ly = sy - s->screen_y;

    if (s->geometry_set) {
      lx += s->window_geometry.x;
      ly += s->window_geometry.y;
    }

    if (lx >= 0 && lx < s->screen_width && ly >= 0 && ly < s->screen_height) {
      best = s;
      *local_x = lx;
      *local_y = ly;

      break;
    }
  }
  return best;
}

static void set_pointer_focus(struct bridge_surface *new_focus, int32_t local_x,
                              int32_t local_y) {
  struct bridge_surface *old = bridge->pointer_focus;

  if (old == new_focus)
    return;

  if (old && bridge_pointer_constraints_is_locked(old)) {
    return;
  }

  if (old && old->resource) {
    struct raw_leave_ctx lctx = {
        .surface_resource = old->resource,
        .serial = wl_display_next_serial(bridge->display),
    };
    int left = bridge_for_each_pointer(old, cb_raw_leave, &lctx);
    if (left > 0) {
      LOG_DEBUG("raw_input: leave surface wid=%u resources=%d",
                old->plexy_window_id, left);
    }
  }

  bridge->pointer_focus = new_focus;
  bridge->seat_pointer_resource = NULL;
  bridge->seat_pointer_surface = NULL;

  if (new_focus && new_focus->resource) {
    new_focus->pending_enter_x = local_x;
    new_focus->pending_enter_y = local_y;
    if (send_pointer_enter_all(new_focus, local_x, local_y) == 0) {
      new_focus->pending_enter = true;
      LOG_DEBUG("raw_input: deferred enter for wid=%u (no pointer resource)",
                new_focus->plexy_window_id);
    }
  }
}

void bridge_raw_pointer_motion(int32_t screen_x, int32_t screen_y, double dx,
                               double dy, uint32_t window_id) {
  static int motion_count = 0;
  if (++motion_count <= 50 || motion_count % 500 == 0) {
    bridge_log("RAW_MOTION #%d: screen=(%d,%d) dx=%.2f dy=%.2f window_id=%u "
               "raw_input_active=%d",
               motion_count, screen_x, screen_y, dx, dy, window_id,
               bridge->raw_input_active);
  }

  struct bridge_surface *target = NULL;
  int32_t lx = 0, ly = 0;

  if (bridge->raw_input_active && bridge->pointer_focus &&
      bridge_pointer_constraints_has_active(bridge->pointer_focus)) {
    target = bridge->pointer_focus;
    lx = screen_x - target->screen_x;
    ly = screen_y - target->screen_y;
    if (target->geometry_set) {
      lx += target->window_geometry.x;
      ly += target->window_geometry.y;
    }
  } else if (window_id != 0) {
    target = hit_test(screen_x, screen_y, &lx, &ly);

    if (!target || target->plexy_window_id != window_id) {

      target = NULL;
      lx = 0;
      ly = 0;
      struct bridge_surface *s = bridge_surface_from_window_id(window_id);
      if (s && s->resource && s->configured) {
        target = s;
        lx = screen_x - s->screen_x;
        ly = screen_y - s->screen_y;
        if (s->geometry_set) {
          lx += s->window_geometry.x;
          ly += s->window_geometry.y;
        }
      }
    }
  }

  if (!target && window_id == 0) {
    target = hit_test(screen_x, screen_y, &lx, &ly);
  }

  if (target && !target->is_x11) {
    if (bridge_pointer_constraints_is_locked(target)) {
      bridge_relative_pointer_send_motion(target, dx, dy);

      bridge_for_each_pointer(target, cb_raw_frame, NULL);
      return;
    }
    input_router_pointer_motion(target, lx, ly);
    return;
  }

  static int raw_count = 0;
  if (++raw_count <= 20 || raw_count % 200 == 0 ||
      target != bridge->pointer_focus) {
    LOG_DEBUG("raw_motion: screen=(%d,%d) dx=%.2f dy=%.2f wid=%u target=%p%s "
              "local=(%d,%d) prev=%p",
              screen_x, screen_y, dx, dy, window_id, (void *)target,
              target ? (target->is_x11 ? " [X11]" : " [WL]") : "", lx, ly,
              (void *)bridge->pointer_focus);
  }

  set_pointer_focus(target, lx, ly);

  if (!target)
    return;

  bridge_pointer_constraints_apply_motion(target, &lx, &ly);

  if (target->pending_enter) {
    target->pending_enter_x = lx;
    target->pending_enter_y = ly;
    if (send_pointer_enter_all(target, lx, ly) == 0) {
      return;
    }
  }

  uint32_t now = bridge_time_ms();

  bool is_locked = bridge_pointer_constraints_is_locked(target);
  if (!is_locked) {
    struct raw_motion_ctx mctx = {
        .time = now,
        .x = wl_fixed_from_int(lx),
        .y = wl_fixed_from_int(ly),
    };
    if (bridge_for_each_pointer(target, cb_raw_motion, &mctx) <= 0) {
      return;
    }
  }

  bridge->seat_pointer_resource = bridge_get_pointer_for_surface(target);
  bridge->seat_pointer_surface = target->resource;

  bridge_relative_pointer_send_motion(target, dx, dy);

  bridge_for_each_pointer(target, cb_raw_frame, NULL);

  target->pointer_x = lx;
  target->pointer_y = ly;
  target->has_pointer_position = true;
}

void bridge_raw_pointer_button(uint32_t button, uint32_t state) {
  struct bridge_surface *focus = bridge->pointer_focus;
  LOG_DEBUG("raw_button: button=0x%x state=%u focus=%p%s", button, state,
            (void *)focus, focus ? (focus->is_x11 ? " [X11]" : " [WL]") : "");
  if (!focus)
    return;

  if (focus->pending_enter && !bridge_pointer_constraints_is_locked(focus) &&
      send_pointer_enter_all(focus, focus->pending_enter_x,
                             focus->pending_enter_y) == 0)
    return;

  uint32_t serial = wl_display_next_serial(bridge->display);
  uint32_t now = bridge_time_ms();
  struct raw_button_ctx bctx = {
      .serial = serial,
      .time = now,
      .button = button,
      .state = state,
  };

  if (bridge_for_each_pointer(focus, cb_raw_button, &bctx) <= 0)
    return;

  if (button == BTN_LEFT && state != 0 && !focus->is_x11 &&
      bridge->keyboard_focus != focus) {

    struct bridge_surface *old = bridge->keyboard_focus;
    if (old && old->resource) {
      struct wl_resource *old_kb = bridge_get_keyboard_for_surface(old);
      if (old_kb) {
        uint32_t s = wl_display_next_serial(bridge->display);
        wl_keyboard_send_leave(old_kb, s, old->resource);
      }
    }

    bridge->keyboard_focus = focus;
    focus->activated = true;

    struct wl_resource *kb = bridge_get_keyboard_for_surface(focus);
    if (kb && focus->resource) {
      struct wl_array keys;
      wl_array_init(&keys);
      uint32_t s = wl_display_next_serial(bridge->display);
      wl_keyboard_send_enter(kb, s, focus->resource, &keys);
      wl_array_release(&keys);
      s = wl_display_next_serial(bridge->display);
      wl_keyboard_send_modifiers(kb, s, bridge->mods_depressed,
                                 bridge->mods_latched, bridge->mods_locked,
                                 bridge->mods_group);
    }
    bridge_data_device_send_selection_to_focus();
  }

  bridge_data_device_pointer_button(focus, state != 0, focus->pointer_x,
                                    focus->pointer_y);
}

void bridge_raw_pointer_axis(int32_t axis, int32_t value, int32_t discrete) {
  struct bridge_surface *focus = bridge->pointer_focus;
  if (!focus)
    return;

  if (focus->pending_enter && !bridge_pointer_constraints_is_locked(focus) &&
      send_pointer_enter_all(focus, focus->pending_enter_x,
                             focus->pending_enter_y) == 0)
    return;

  uint32_t now = bridge_time_ms();
  struct raw_axis_ctx actx = {
      .time = now,
      .axis = (uint32_t)axis,
      .value = (wl_fixed_t)value,
      .discrete = discrete,
  };
  bridge_for_each_pointer(focus, cb_raw_axis, &actx);
}
