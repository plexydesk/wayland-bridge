/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#define _POSIX_C_SOURCE 200809L
#include "input_router.h"
#include "wayland_bridge.h"
#include "xdg-shell-protocol.h"
#include "xwm.h"
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wayland-server-protocol.h>

#define GESTURE_SWIPE_THRESHOLD_PX 10

#define GESTURE_PINCH_SCALE_DIVISOR 200.0

#define GESTURE_PINCH_ROTATION_DIVISOR 4.0

#define GESTURE_PINCH_SCALE_MIN 0.10

extern void xwm_set_focus(struct xwm *xwm, struct xwm_window *window);

static uint32_t get_timestamp_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static inline bool surface_alive(struct bridge_surface *s) {
  return s && s->magic == 0xBD5FACE0 && s->resource;
}

static bool touch_active = false;
static int32_t touch_id = 1;
static struct bridge_surface *touch_surface = NULL;

static struct bridge_surface *last_raised_surface = NULL;
static bool gesture_left_down = false;
static bool gesture_right_down = false;
static bool gesture_hold_active = false;
static bool gesture_swipe_active = false;
static bool gesture_pinch_active = false;
static struct bridge_surface *gesture_surface = NULL;
static int32_t gesture_last_x = 0;
static int32_t gesture_last_y = 0;

static void phys_to_surface_local(struct bridge_surface *surface,
                                  int32_t phys_x, int32_t phys_y, int32_t *lx,
                                  int32_t *ly) {
  float scale = bridge_surface_scale_factor(surface);
  *lx = (int32_t)((float)(phys_x - surface->screen_x) / scale);
  *ly = (int32_t)((float)(phys_y - surface->screen_y) / scale);
}

void bridge_send_xdg_toplevel_bounds(struct bridge_surface *surface) {
  if (!surface || !surface->xdg_toplevel) {
    return;
  }

  const uint32_t version = wl_resource_get_version(surface->xdg_toplevel);
  if (version < XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION) {
    return;
  }

  PlexyOutputInfo info;
  if (!bridge_surface_get_output_info(surface, &info)) {
    return;
  }

  int32_t logical_width = (int32_t)info.pixel_width;
  int32_t logical_height = (int32_t)info.pixel_height;
  if (!surface->is_x11) {
    const float scale = bridge_output_scale_factor(&info);
    logical_width =
        (int32_t)bridge_physical_to_logical_extent(info.pixel_width, scale);
    logical_height =
        (int32_t)bridge_physical_to_logical_extent(info.pixel_height, scale);
  }

  xdg_toplevel_send_configure_bounds(surface->xdg_toplevel, logical_width,
                                     logical_height);
}

static void track_surface_output_enter(struct bridge_surface *surface,
                                       uint32_t output_id) {
  if (!surface || output_id == 0) {
    return;
  }

  for (uint32_t i = 0; i < surface->entered_output_count; ++i) {
    if (surface->entered_output_ids[i] == output_id) {
      surface->current_output_id = output_id;
      return;
    }
  }

  if (surface->entered_output_count < 4) {
    surface->entered_output_ids[surface->entered_output_count++] = output_id;
  }
  surface->current_output_id = output_id;
}

static void track_surface_output_leave(struct bridge_surface *surface,
                                       uint32_t output_id) {
  if (!surface || output_id == 0) {
    return;
  }

  for (uint32_t i = 0; i < surface->entered_output_count; ++i) {
    if (surface->entered_output_ids[i] == output_id) {
      memmove(&surface->entered_output_ids[i],
              &surface->entered_output_ids[i + 1],
              (surface->entered_output_count - i - 1) * sizeof(uint32_t));
      surface->entered_output_count--;
      break;
    }
  }

  if (surface->current_output_id == output_id) {
    surface->current_output_id =
        surface->entered_output_count > 0
            ? surface->entered_output_ids[surface->entered_output_count - 1]
            : 0;
  }
}

static void touch_send_frame(struct wl_resource *touch) {
  if (touch && wl_resource_get_version(touch) >= WL_TOUCH_FRAME_SINCE_VERSION) {
    wl_touch_send_frame(touch);
  }
}

static void touch_cancel_active(struct bridge_surface *surface) {
  if (!bridge || !touch_active || !touch_surface)
    return;
  if (surface && touch_surface != surface)
    return;

  struct wl_resource *touch = bridge_get_touch_for_surface(touch_surface);
  if (touch) {
    wl_touch_send_cancel(touch);
    touch_send_frame(touch);
  }
  touch_active = false;
  touch_surface = NULL;
}

static void gesture_cancel_active(struct bridge_surface *surface) {
  if (!gesture_surface)
    return;
  if (surface && gesture_surface != surface)
    return;

  if (gesture_swipe_active) {
    bridge_pointer_gesture_swipe_end(gesture_surface, true);
  }
  if (gesture_pinch_active) {
    bridge_pointer_gesture_pinch_end(gesture_surface, true);
  }
  if (gesture_hold_active) {
    bridge_pointer_gesture_hold_end(gesture_surface, true);
  }
  gesture_swipe_active = false;
  gesture_pinch_active = false;
  gesture_hold_active = false;
  gesture_left_down = false;
  gesture_right_down = false;
  gesture_surface = NULL;
}

static void send_xdg_toplevel_configure(struct bridge_surface *surface,
                                        uint32_t width, uint32_t height) {
  if (!surface || !surface->xdg_toplevel || !surface->xdg_surface) {
    return;
  }

  bridge_send_xdg_toplevel_bounds(surface);
  uint32_t configure_width = width;
  uint32_t configure_height = height;
  bridge_physical_to_surface_size(surface, width, height, &configure_width,
                                  &configure_height);

  struct wl_array states;
  wl_array_init(&states);

  uint32_t *state = NULL;
  if (surface->maximized) {
    state = wl_array_add(&states, sizeof(*state));
    if (state)
      *state = XDG_TOPLEVEL_STATE_MAXIMIZED;
  }
  if (surface->fullscreen) {
    state = wl_array_add(&states, sizeof(*state));
    if (state)
      *state = XDG_TOPLEVEL_STATE_FULLSCREEN;
  }
  if (surface->resizing) {
    state = wl_array_add(&states, sizeof(*state));
    if (state)
      *state = XDG_TOPLEVEL_STATE_RESIZING;
  }
  if (surface->activated) {
    state = wl_array_add(&states, sizeof(*state));
    if (state)
      *state = XDG_TOPLEVEL_STATE_ACTIVATED;
  }
  if (surface->tiled_left) {
    state = wl_array_add(&states, sizeof(*state));
    if (state)
      *state = XDG_TOPLEVEL_STATE_TILED_LEFT;
  }
  if (surface->tiled_right) {
    state = wl_array_add(&states, sizeof(*state));
    if (state)
      *state = XDG_TOPLEVEL_STATE_TILED_RIGHT;
  }
  if (surface->suspended) {
    state = wl_array_add(&states, sizeof(*state));
    if (state)
      *state = XDG_TOPLEVEL_STATE_SUSPENDED;
  }

  surface->configure_serial++;
  xdg_toplevel_send_configure(surface->xdg_toplevel, configure_width,
                              configure_height, &states);
  xdg_surface_send_configure(surface->xdg_surface, surface->configure_serial);
  wl_array_release(&states);
}

static bool pointer_is_in_input_region(struct bridge_surface *surface,
                                       int32_t x, int32_t y) {
  if (!surface)
    return false;
  if (surface->is_x11) {
    return true;
  }
  return bridge_surface_input_region_contains(surface, x, y);
}

static struct bridge_surface *
resolve_pointer_target_surface(struct bridge_surface *surface, int32_t *x,
                               int32_t *y) {
  if (!surface || !x || !y) {
    return NULL;
  }

  struct bridge_surface *target = surface;
  int32_t local_x = *x;
  int32_t local_y = *y;

  while (target && !target->is_x11 &&
         !pointer_is_in_input_region(target, local_x, local_y)) {
    if (!target->subsurface_parent) {
      return NULL;
    }
    local_x += target->subsurface_x;
    local_y += target->subsurface_y;
    target = target->subsurface_parent;
  }

  if (!target) {
    return NULL;
  }

  *x = local_x;
  *y = local_y;
  return target;
}

static void plexy_local_to_wayland_local(const struct bridge_surface *surface,
                                         int32_t in_x, int32_t in_y,
                                         int32_t *out_x, int32_t *out_y) {
  if (bridge_surface_uses_logical_content_buffer(surface) ||
      (surface && !surface->is_x11 && surface->viewport_dst_set)) {
    *out_x = in_x;
    *out_y = in_y;
    if (bridge_surface_uses_window_geometry_crop(surface) &&
        !surface->viewport_dst_set) {
      *out_x += surface->window_geometry.x;
      *out_y += surface->window_geometry.y;
    }
    return;
  }

  int32_t scale = 1;
  if (surface && !surface->is_x11 && !surface->viewport_dst_set &&
      surface->buffer_scale > 1) {
    scale = (int32_t)surface->buffer_scale;
  }

  *out_x = in_x / scale;
  *out_y = in_y / scale;
}

static bool surface_is_x11_popup(const struct bridge_surface *surface) {
  if (!surface || !surface->is_x11 || !surface->xwm_window)
    return false;
  return xwm_window_is_focus_inactive((struct xwm_window *)surface->xwm_window);
}

static void raise_toplevel_chain(struct bridge_surface *surface) {
  if (!bridge || !bridge->plexy_conn || !surface)
    return;

  if (last_raised_surface == surface)
    return;
  last_raised_surface = surface;

  struct bridge_surface *chain[32];
  int n = 0;
  struct bridge_surface *cur = surface;
  while (cur && n < (int)(sizeof(chain) / sizeof(chain[0]))) {
    chain[n++] = cur;
    cur = cur->toplevel_parent;
  }

  for (int i = n - 1; i >= 0; --i) {
    struct bridge_surface *s = chain[i];
    if (!s->plexy_window)
      continue;
    if (plexy_raise_window(bridge->plexy_conn, s->plexy_window_id) < 0) {
      LOG_WARN("Failed to raise transient chain window_id=%u",
               s->plexy_window_id);
    }
  }
}

void pointer_send_frame(struct wl_resource *pointer) {
  if (!pointer)
    return;

  if (wl_resource_get_version(pointer) >= WL_POINTER_FRAME_SINCE_VERSION) {
    wl_pointer_send_frame(pointer);
  }
}

static bool ensure_x11_pointer_focus(struct bridge_surface *surface, int32_t x,
                                     int32_t y) {
  if (bridge->pointer_focus == surface)
    return true;

  struct bridge_surface *old = bridge->pointer_focus;

  if (old && old->resource) {
    struct wl_resource *ptr = bridge_get_pointer_for_surface(old);
    if (ptr) {
      uint32_t s = wl_display_next_serial(bridge->display);
      wl_pointer_send_leave(ptr, s, old->resource);
      pointer_send_frame(ptr);
    }

    if (old->is_x11 && old->xwm_window)
      xwm_send_leave_event((struct xwm_window *)old->xwm_window, old->pointer_x,
                           old->pointer_y);
  }
  bridge->pointer_focus = surface;
  bridge->seat_pointer_resource = NULL;
  bridge->seat_pointer_surface = NULL;

  if (surface->resource) {
    struct wl_resource *ptr = bridge_get_pointer_for_surface(surface);
    if (ptr) {
      uint32_t s = wl_display_next_serial(bridge->display);
      wl_pointer_send_enter(ptr, s, surface->resource, wl_fixed_from_int(x),
                            wl_fixed_from_int(y));
      pointer_send_frame(ptr);
      bridge->seat_pointer_resource = ptr;
      bridge->seat_pointer_surface = surface->resource;
      surface->has_pointer_position = true;
      surface->pointer_x = x;
      surface->pointer_y = y;

      if (surface->is_x11 && surface->xwm_window)
        xwm_send_enter_event((struct xwm_window *)surface->xwm_window, x, y);
      return true;
    }
  }
  return false;
}

void forward_pointer_enter(PlexyWindow *window, int32_t x, int32_t y,
                           void *user_data) {
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface))
    return;

  int32_t lx = x, ly = y;
  plexy_local_to_wayland_local(surface, x, y, &lx, &ly);

  LOG_TRACE("forward_pointer_enter: surface=%p is_x11=%d local=(%d,%d)",
            (void *)surface, surface->is_x11, lx, ly);

  if (surface->is_x11) {

    if (bridge->raw_input_active)
      return;
    ensure_x11_pointer_focus(surface, lx, ly);

    return;
  }

  if (bridge->raw_input_active)
    return;

  struct bridge_surface *target =
      resolve_pointer_target_surface(surface, &lx, &ly);
  if (!target) {
    return;
  }
  input_router_pointer_enter(target, lx, ly);
}

void forward_pointer_leave(PlexyWindow *window, void *user_data) {
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface))
    return;
  if (surface->is_x11) {

    if (bridge->raw_input_active)
      return;

    if (bridge->pointer_focus == surface) {
      struct wl_resource *ptr = bridge_get_pointer_for_surface(surface);
      if (ptr && surface->resource) {
        uint32_t s = wl_display_next_serial(bridge->display);
        wl_pointer_send_leave(ptr, s, surface->resource);
        pointer_send_frame(ptr);
      }

      if (surface->xwm_window)
        xwm_send_leave_event((struct xwm_window *)surface->xwm_window,
                             (int16_t)surface->pointer_x,
                             (int16_t)surface->pointer_y);
      bridge->pointer_focus = NULL;
      bridge->seat_pointer_resource = NULL;
      bridge->seat_pointer_surface = NULL;
    }
    return;
  }

  if (bridge->raw_input_active)
    return;

  touch_cancel_active(surface);
  gesture_cancel_active(surface);
  input_router_pointer_leave(surface);
}

void forward_pointer_motion(PlexyWindow *window, int32_t x, int32_t y,
                            void *user_data) {
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface))
    return;
  if (bridge->session_locked)
    return;
  bridge_idle_notify_activity();

  int32_t lx = x, ly = y;
  plexy_local_to_wayland_local(surface, x, y, &lx, &ly);

  if (surface->is_x11) {

    if (bridge->raw_input_active)
      return;
    ensure_x11_pointer_focus(surface, lx, ly);
    struct wl_resource *ptr = bridge->seat_pointer_resource;
    if (ptr) {
      if (!bridge_pointer_constraints_is_locked(surface)) {
        uint32_t time = get_timestamp_ms();
        wl_pointer_send_motion(ptr, time, wl_fixed_from_int(lx),
                               wl_fixed_from_int(ly));
        pointer_send_frame(ptr);
        bridge_input_timestamps_send(ptr);
      }
      surface->pointer_x = lx;
      surface->pointer_y = ly;
    }
    return;
  }

  if (bridge->raw_input_active)
    return;

  LOG_TRACE("forward_pointer_motion: surface=%p is_x11=%d local=(%d,%d)",
            (void *)surface, surface->is_x11, lx, ly);

  struct bridge_surface *target =
      resolve_pointer_target_surface(surface, &lx, &ly);
  if (!target) {
    if (bridge->pointer_focus == surface) {
      input_router_pointer_leave(surface);
    }
    return;
  }

  if (target != surface && bridge->pointer_focus == surface) {
    input_router_pointer_leave(surface);
  }
  surface = target;

  if (bridge->pointer_focus != surface) {
    input_router_pointer_enter(surface, lx, ly);
  }

  if ((gesture_left_down || gesture_right_down) && gesture_surface == surface) {
    int32_t dx_raw = lx - gesture_last_x;
    int32_t dy_raw = ly - gesture_last_y;
    if (gesture_left_down) {
      if (!gesture_swipe_active) {
        int32_t adx = dx_raw < 0 ? -dx_raw : dx_raw;
        int32_t ady = dy_raw < 0 ? -dy_raw : dy_raw;
        if (adx + ady >= GESTURE_SWIPE_THRESHOLD_PX) {
          if (gesture_hold_active) {
            bridge_pointer_gesture_hold_end(surface, true);
            gesture_hold_active = false;
          }
          bridge_pointer_gesture_swipe_begin(surface, 1);
          gesture_swipe_active = true;
        }
      }
      if (gesture_swipe_active)
        bridge_pointer_gesture_swipe_update(surface, dx_raw, dy_raw);
    }
    if (gesture_right_down) {
      if (!gesture_pinch_active) {
        int32_t adx = dx_raw < 0 ? -dx_raw : dx_raw;
        int32_t ady = dy_raw < 0 ? -dy_raw : dy_raw;
        if (adx + ady >= GESTURE_SWIPE_THRESHOLD_PX) {
          bridge_pointer_gesture_pinch_begin(surface, 2);
          gesture_pinch_active = true;
        }
      }
      if (gesture_pinch_active) {
        double scale = 1.0 + ((double)(-dy_raw) / GESTURE_PINCH_SCALE_DIVISOR);
        if (scale < GESTURE_PINCH_SCALE_MIN)
          scale = GESTURE_PINCH_SCALE_MIN;
        double rotation = (double)dx_raw / GESTURE_PINCH_ROTATION_DIVISOR;
        bridge_pointer_gesture_pinch_update(surface, dx_raw, dy_raw, scale,
                                            rotation);
      }
    }
    gesture_last_x = lx;
    gesture_last_y = ly;
  }

  input_router_pointer_motion(surface, lx, ly);

  struct wl_resource *motion_ptr = bridge_get_pointer_for_surface(surface);
  if (motion_ptr)
    bridge_input_timestamps_send(motion_ptr);
}

void forward_pointer_button(PlexyWindow *window, uint32_t button, bool pressed,
                            int32_t x, int32_t y, void *user_data) {
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface))
    return;
  if (bridge->session_locked)
    return;
  bridge_idle_notify_activity();

  int32_t lx = x, ly = y;
  plexy_local_to_wayland_local(surface, x, y, &lx, &ly);

  if (pressed && bridge->grabbed_popup && bridge->grabbed_popup != surface &&
      surface->popup_parent != bridge->grabbed_popup) {
    struct bridge_surface *gp = bridge->grabbed_popup;
    bridge->grabbed_popup = NULL;
    if (gp->xdg_popup)
      xdg_popup_send_popup_done(gp->xdg_popup);
  }

  if (!surface->is_x11) {
    if (bridge->raw_input_active)
      return;
    struct bridge_surface *target =
        resolve_pointer_target_surface(surface, &lx, &ly);
    if (!target) {
      if (bridge->pointer_focus == surface) {
        input_router_pointer_leave(surface);
      }
      return;
    }

    if (target != surface && bridge->pointer_focus == surface) {
      input_router_pointer_leave(surface);
    }
    surface = target;

    if (bridge->pointer_focus != surface) {
      input_router_pointer_enter(surface, lx, ly);
    }
  }

  if (surface->is_x11)
    ensure_x11_pointer_focus(surface, lx, ly);

  if (surface->is_x11 && bridge->raw_input_active)
    return;

  bool is_x11_or_popup = (surface->is_x11 && surface->toplevel_parent) ||
                         surface_is_x11_popup(surface);
  if (!surface->xdg_popup && !is_x11_or_popup && pressed &&
      bridge->keyboard_focus != surface) {
    if (bridge->keyboard_focus && bridge->keyboard_focus->resource) {
      bridge_text_input_notify_focus(bridge->keyboard_focus, false);
      struct kb_leave_ctx {
        struct wl_resource *surf;
        uint32_t serial;
      } klc = {
          .surf = bridge->keyboard_focus->resource,
          .serial = wl_display_next_serial(bridge->display),
      };
      void kb_leave_cb(struct wl_resource * kb, void *d) {
        struct kb_leave_ctx *c = d;
        wl_keyboard_send_leave(kb, c->serial, c->surf);
      }
      bridge_for_each_keyboard(bridge->keyboard_focus, kb_leave_cb, &klc);
    }
    bridge->keyboard_focus = surface;
    if (surface->is_x11 && surface->xwm_window) {
      extern struct xwm *xwm_get_instance(void);
      struct xwm *xwm = xwm_get_instance();
      if (xwm)
        xwm_set_focus(xwm, (struct xwm_window *)surface->xwm_window);
    }
    bridge_text_input_notify_focus(surface, true);
    raise_toplevel_chain(surface);
    if (surface->resource) {
      struct kb_enter_ctx {
        struct wl_resource *surf;
        uint32_t serial;
        uint32_t dep, lat, loc, grp;
      } kec = {
          .surf = surface->resource,
          .serial = wl_display_next_serial(bridge->display),
          .dep = bridge->mods_depressed,
          .lat = bridge->mods_latched,
          .loc = bridge->mods_locked,
          .grp = bridge->mods_group,
      };
      void kb_enter_cb(struct wl_resource * kb, void *d) {
        struct kb_enter_ctx *c = d;
        struct wl_array keys;
        wl_array_init(&keys);
        wl_keyboard_send_enter(kb, c->serial, c->surf, &keys);
        wl_array_release(&keys);
        uint32_t ms = wl_display_next_serial(
            wl_client_get_display(wl_resource_get_client(kb)));
        wl_keyboard_send_modifiers(kb, ms, c->dep, c->lat, c->loc, c->grp);
      }
      bridge_for_each_keyboard(surface, kb_enter_cb, &kec);
    }
    bridge_data_device_send_selection_to_focus();
  }

  if (!surface->is_x11 && !bridge_pointer_constraints_is_locked(surface)) {
    if (button == BTN_LEFT) {
      if (pressed) {
        gesture_surface = surface;
        gesture_last_x = lx;
        gesture_last_y = ly;
        gesture_left_down = true;
        if (!gesture_hold_active) {
          bridge_pointer_gesture_hold_begin(surface, 1);
          gesture_hold_active = true;
        }
      } else if (gesture_surface == surface) {
        if (gesture_swipe_active) {
          bridge_pointer_gesture_swipe_end(surface, false);
          gesture_swipe_active = false;
        }
        if (gesture_hold_active) {
          bridge_pointer_gesture_hold_end(surface, false);
          gesture_hold_active = false;
        }
        gesture_left_down = false;
        if (!gesture_right_down)
          gesture_surface = NULL;
      }
    } else if (button == BTN_RIGHT) {
      if (pressed) {
        gesture_surface = surface;
        gesture_last_x = lx;
        gesture_last_y = ly;
        gesture_right_down = true;
      } else if (gesture_surface == surface) {
        if (gesture_pinch_active) {
          bridge_pointer_gesture_pinch_end(surface, false);
          gesture_pinch_active = false;
        }
        gesture_right_down = false;
        if (!gesture_left_down)
          gesture_surface = NULL;
      }
    }
  }

  if (surface->is_x11) {
    struct wl_resource *ptr = bridge->seat_pointer_resource;
    if (ptr) {
      uint32_t serial = wl_display_next_serial(bridge->display);
      uint32_t time = get_timestamp_ms();
      uint32_t state = pressed ? WL_POINTER_BUTTON_STATE_PRESSED
                               : WL_POINTER_BUTTON_STATE_RELEASED;
      wl_pointer_send_button(ptr, serial, time, button, state);
      pointer_send_frame(ptr);
      bridge_input_timestamps_send(ptr);
      bridge_data_device_pointer_button(surface, pressed, lx, ly);
    }
  } else {
    input_router_pointer_button(surface, button, pressed, lx, ly);
    struct wl_resource *btn_ptr = bridge_get_pointer_for_surface(surface);
    if (btn_ptr)
      bridge_input_timestamps_send(btn_ptr);
  }

  if (!surface->is_x11 && button == BTN_LEFT) {
    struct wl_resource *touch = bridge_get_touch_for_surface(surface);
    struct wl_resource *ptr = bridge_get_pointer_for_surface(surface);
    if (touch && surface->resource && !ptr) {
      bridge_idle_notify_activity();

      uint32_t time = get_timestamp_ms();
      if (pressed) {
        uint32_t ser = wl_display_next_serial(bridge->display);
        wl_touch_send_down(touch, ser, time, surface->resource, touch_id,
                           wl_fixed_from_int(lx), wl_fixed_from_int(ly));
        touch_active = true;
        touch_surface = surface;
      } else if (touch_active && touch_surface == surface) {
        uint32_t ser = wl_display_next_serial(bridge->display);
        wl_touch_send_up(touch, ser, time, touch_id);
        touch_active = false;
        touch_surface = NULL;
      }
      touch_send_frame(touch);
      bridge_input_timestamps_send(touch);
    }
  }
}

void forward_pointer_axis(PlexyWindow *window, int32_t axis, int32_t value,
                          int32_t discrete, void *user_data) {
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface))
    return;
  if (bridge->session_locked)
    return;

  if (surface->is_x11) {

    if (bridge->raw_input_active)
      return;
    ensure_x11_pointer_focus(
        surface, surface->has_pointer_position ? surface->pointer_x : 0,
        surface->has_pointer_position ? surface->pointer_y : 0);
    struct wl_resource *ptr = bridge->seat_pointer_resource;
    if (ptr) {
      uint32_t time = get_timestamp_ms();
      uint32_t wl_axis = (axis == 0) ? WL_POINTER_AXIS_VERTICAL_SCROLL
                                     : WL_POINTER_AXIS_HORIZONTAL_SCROLL;
      wl_pointer_send_axis(ptr, time, wl_axis, (wl_fixed_t)value);
      int ver = wl_resource_get_version(ptr);
      if (ver >= WL_POINTER_AXIS_DISCRETE_SINCE_VERSION && discrete != 0)
        wl_pointer_send_axis_discrete(ptr, wl_axis, discrete);
      pointer_send_frame(ptr);
      bridge_input_timestamps_send(ptr);
    }
    return;
  }

  if (bridge->raw_input_active)
    return;

  if (bridge->pointer_focus != surface) {

    int32_t ax, ay;
    if (surface->has_pointer_position) {
      ax = surface->pointer_x;
      ay = surface->pointer_y;
    } else {
      float _scale = bridge_surface_scale_factor(surface);
      ax = (int32_t)((float)surface->screen_width / _scale / 2.0f);
      ay = (int32_t)((float)surface->screen_height / _scale / 2.0f);
    }
    input_router_pointer_enter(surface, ax, ay);
    if (bridge->pointer_focus != surface)
      return;
  }
  input_router_pointer_axis(surface, axis, value, discrete);
}

void forward_touch_down(PlexyWindow *window, int32_t touch_id, int32_t x,
                        int32_t y, uint32_t time_ms, void *user_data) {
  (void)window;
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface) || surface->is_x11)
    return;
  if (bridge->session_locked)
    return;
  bridge_idle_notify_activity();

  struct wl_resource *touch = bridge_get_touch_for_surface(surface);
  if (!touch || !surface->resource)
    return;

  int32_t lx = x, ly = y;
  plexy_local_to_wayland_local(surface, x, y, &lx, &ly);
  uint32_t serial = wl_display_next_serial(bridge->display);
  uint32_t time = time_ms ? time_ms : get_timestamp_ms();
  wl_touch_send_down(touch, serial, time, surface->resource, touch_id,
                     wl_fixed_from_int(lx), wl_fixed_from_int(ly));
  bridge_input_timestamps_send(touch);
}

void forward_touch_motion(PlexyWindow *window, int32_t touch_id, int32_t x,
                          int32_t y, uint32_t time_ms, void *user_data) {
  (void)window;
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface) || surface->is_x11)
    return;
  if (bridge->session_locked)
    return;
  bridge_idle_notify_activity();

  struct wl_resource *touch = bridge_get_touch_for_surface(surface);
  if (!touch)
    return;

  int32_t lx = x, ly = y;
  plexy_local_to_wayland_local(surface, x, y, &lx, &ly);
  uint32_t time = time_ms ? time_ms : get_timestamp_ms();
  wl_touch_send_motion(touch, time, touch_id, wl_fixed_from_int(lx),
                       wl_fixed_from_int(ly));
  bridge_input_timestamps_send(touch);
}

void forward_touch_up(PlexyWindow *window, int32_t touch_id, uint32_t time_ms,
                      void *user_data) {
  (void)window;
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface) || surface->is_x11)
    return;
  if (bridge->session_locked)
    return;
  bridge_idle_notify_activity();

  struct wl_resource *touch = bridge_get_touch_for_surface(surface);
  if (!touch)
    return;

  uint32_t serial = wl_display_next_serial(bridge->display);
  uint32_t time = time_ms ? time_ms : get_timestamp_ms();
  wl_touch_send_up(touch, serial, time, touch_id);
  bridge_input_timestamps_send(touch);
}

void forward_touch_cancel(PlexyWindow *window, void *user_data) {
  (void)window;
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface) || surface->is_x11)
    return;

  struct wl_resource *touch = bridge_get_touch_for_surface(surface);
  if (!touch)
    return;

  wl_touch_send_cancel(touch);
  touch_send_frame(touch);
  bridge_input_timestamps_send(touch);
}

void forward_touch_frame(PlexyWindow *window, void *user_data) {
  (void)window;
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface) || surface->is_x11)
    return;

  struct wl_resource *touch = bridge_get_touch_for_surface(surface);
  touch_send_frame(touch);
  if (touch)
    bridge_input_timestamps_send(touch);
}

void forward_key(PlexyWindow *window, uint32_t keycode, bool pressed,
                 uint32_t modifiers, void *user_data) {
  struct bridge_surface *surface = (struct bridge_surface *)user_data;

  if (!bridge || !surface_alive(surface)) {
    return;
  }
  if (bridge->session_locked)
    return;
  bridge_idle_notify_activity();

  if (surface->is_x11 && surface->resource &&
      bridge_xwayland_kbd_grab_active(surface->resource)) {
    struct wl_resource *keyboard = bridge_get_keyboard_for_surface(surface);
    if (keyboard) {
      uint32_t serial = wl_display_next_serial(bridge->display);
      uint32_t time = get_timestamp_ms();
      uint32_t state = pressed ? WL_KEYBOARD_KEY_STATE_PRESSED
                               : WL_KEYBOARD_KEY_STATE_RELEASED;
      wl_keyboard_send_key(keyboard, serial, time, keycode, state);
      bridge_input_timestamps_send(keyboard);
    }
    return;
  }

  if (bridge->keyboard_focus != surface) {
    LOG_INFO("forward_key: DROPPED key=%u for surface=%p (kb_focus=%p)",
             keycode, (void *)surface, (void *)bridge->keyboard_focus);
    return;
  }

  struct key_ctx {
    uint32_t serial, time, keycode, state;
  } kctx = {
      .serial = wl_display_next_serial(bridge->display),
      .time = get_timestamp_ms(),
      .keycode = keycode,
      .state = pressed ? WL_KEYBOARD_KEY_STATE_PRESSED
                       : WL_KEYBOARD_KEY_STATE_RELEASED,
  };
  void key_cb(struct wl_resource * kb, void *d) {
    struct key_ctx *c = d;
    wl_keyboard_send_key(kb, c->serial, c->time, c->keycode, c->state);
    bridge_input_timestamps_send(kb);
  }
  int sent = bridge_for_each_keyboard(surface, key_cb, &kctx);
  LOG_INFO("forward_key: sent key=%u %s to surface=%p is_x11=%d clients=%d",
           keycode, pressed ? "press" : "release", (void *)surface,
           surface->is_x11, sent);
  if (pressed && !surface->is_x11) {
    bridge_text_input_try_commit_key(surface, keycode, modifiers);
  }
}

void forward_modifiers(PlexyWindow *window, uint32_t depressed,
                       uint32_t latched, uint32_t locked, uint32_t group,
                       void *user_data) {
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface)) {
    return;
  }

  depressed = plexy_mods_to_xkb(depressed);
  latched = plexy_mods_to_xkb(latched);
  locked = plexy_mods_to_xkb(locked);

  bridge->mods_depressed = depressed;
  bridge->mods_latched = latched;
  bridge->mods_locked = locked;
  bridge->mods_group = group;

  struct mod_ctx {
    uint32_t serial, dep, lat, loc, grp;
  } mctx = {
      .serial = wl_display_next_serial(bridge->display),
      .dep = depressed,
      .lat = latched,
      .loc = locked,
      .grp = group,
  };
  void mod_cb(struct wl_resource * kb, void *d) {
    struct mod_ctx *c = d;
    wl_keyboard_send_modifiers(kb, c->serial, c->dep, c->lat, c->loc, c->grp);
  }
  bridge_for_each_keyboard(surface, mod_cb, &mctx);
}

void forward_focus_in(PlexyWindow *window, void *user_data) {
  struct bridge_surface *surface = (struct bridge_surface *)user_data;

  if (!bridge || !surface_alive(surface)) {
    return;
  }

  extern struct xwm *xwm_get_instance(void);
  extern void xwm_send_focus_in_event(struct xwm_window * window);
  extern void xwm_send_focus_out_event(struct xwm_window * window);
  extern bool xwm_window_is_focus_inactive(struct xwm_window * window);

  LOG_DEBUG(
      "forward_focus_in: surface=%p is_x11=%d xwm_window=%p old_kb_focus=%p",
      (void *)surface, surface->is_x11, surface->xwm_window,
      (void *)bridge->keyboard_focus);

  if (surface->is_x11 && surface->xwm_window &&
      xwm_window_is_focus_inactive((struct xwm_window *)surface->xwm_window)) {
    return;
  }

  if (surface->xdg_popup) {
    return;
  }

  if (bridge->keyboard_focus == surface) {
    LOG_INFO("forward_focus_in: already focused surface=%p is_x11=%d",
             (void *)surface, surface->is_x11);
    if (surface->is_x11 && surface->xwm_window) {
      struct xwm *xwm = xwm_get_instance();
      struct xwm_window *x11_window = (struct xwm_window *)surface->xwm_window;
      if (xwm && xwm->focus_window != x11_window) {
        LOG_INFO("forward_focus_in: re-sync XWM focus to window=%u",
                 x11_window->id);
        xwm_set_focus(xwm, x11_window);
        xwm_send_focus_in_event(x11_window);
      }
    }
    return;
  }

  struct bridge_surface *old = bridge->keyboard_focus;
  if (old && old != surface) {
    old->activated = false;
    bridge_text_input_notify_focus(old, false);

    if (old->is_x11 && old->xwm_window) {
      xwm_send_focus_out_event((struct xwm_window *)old->xwm_window);
    }

    struct kb_leave_fctx {
      struct wl_resource *surf;
      uint32_t serial;
    } klf = {
        .surf = old->resource,
        .serial = wl_display_next_serial(bridge->display),
    };
    void kbl_focus_cb(struct wl_resource * kb, void *d) {
      struct kb_leave_fctx *c = d;
      wl_keyboard_send_leave(kb, c->serial, c->surf);
    }
    if (old->resource) {
      bridge_for_each_keyboard(old, kbl_focus_cb, &klf);
    }

    if (old->xdg_toplevel && old->xdg_surface) {
      uint32_t ow = 0, oh = 0;
      if (old->plexy_window)
        plexy_window_get_size(old->plexy_window, &ow, &oh);
      send_xdg_toplevel_configure(old, ow, oh);
    }
  }

  bridge->keyboard_focus = surface;
  surface->activated = true;

  LOG_INFO("forward_focus_in: focus changed to surface=%p is_x11=%d "
           "xwm_window=%p old=%p",
           (void *)surface, surface->is_x11, surface->xwm_window, (void *)old);

  bridge_data_device_send_selection_to_focus();
  bridge_text_input_notify_focus(surface, true);

  if (surface->is_x11 && surface->xwm_window) {
    struct xwm *xwm = xwm_get_instance();
    if (xwm) {
      xwm_set_focus(xwm, (struct xwm_window *)surface->xwm_window);
    }
    xwm_send_focus_in_event((struct xwm_window *)surface->xwm_window);
  }

  if (!surface_alive(surface))
    return;

  struct kb_enter_fctx {
    struct wl_resource *surf;
    uint32_t serial, dep, lat, loc, grp;
  } kef = {
      .surf = surface->resource,
      .serial = wl_display_next_serial(bridge->display),
      .dep = bridge->mods_depressed,
      .lat = bridge->mods_latched,
      .loc = bridge->mods_locked,
      .grp = bridge->mods_group,
  };
  void kbe_focus_cb(struct wl_resource * kb, void *d) {
    struct kb_enter_fctx *c = d;
    struct wl_array keys;
    wl_array_init(&keys);
    wl_keyboard_send_enter(kb, c->serial, c->surf, &keys);
    wl_array_release(&keys);
    uint32_t ms = wl_display_next_serial(
        wl_client_get_display(wl_resource_get_client(kb)));
    wl_keyboard_send_modifiers(kb, ms, c->dep, c->lat, c->loc, c->grp);
  }
  int kbd_count = bridge_for_each_keyboard(surface, kbe_focus_cb, &kef);
  LOG_INFO("forward_focus_in: sent keyboard enter to %d keyboard resources "
           "for surface=%p",
           kbd_count, (void *)surface);
  if (kbd_count == 0)
    return;

  uint32_t width = 0, height = 0;
  if (surface->plexy_window) {
    plexy_window_get_size(surface->plexy_window, &width, &height);
  }
  send_xdg_toplevel_configure(surface, width, height);
}

void forward_focus_out(PlexyWindow *window, void *user_data) {
  struct bridge_surface *surface = (struct bridge_surface *)user_data;

  if (!bridge || !surface_alive(surface)) {
    return;
  }

  if (bridge->keyboard_focus == surface) {
    bridge->keyboard_focus = NULL;
  }
  surface->activated = false;
  bridge_text_input_notify_focus(surface, false);
  touch_cancel_active(surface);
  gesture_cancel_active(surface);
  last_raised_surface = NULL;

  if (surface->is_x11 && surface->xwm_window) {
    extern struct xwm *xwm_get_instance(void);
    extern void xwm_send_focus_out_event(struct xwm_window * window);
    xwm_send_focus_out_event((struct xwm_window *)surface->xwm_window);
    struct xwm *xwm = xwm_get_instance();
    if (xwm)
      xwm_set_focus(xwm, NULL);
  }

  if (!surface_alive(surface))
    return;

  if (surface->xdg_popup && !surface->is_x11) {
    xdg_popup_send_popup_done(surface->xdg_popup);
    if (bridge->grabbed_popup == surface)
      bridge->grabbed_popup = NULL;
  }

  struct kb_leave_foctx {
    struct wl_resource *surf;
    uint32_t serial;
  } klfo = {
      .surf = surface->resource,
      .serial = wl_display_next_serial(bridge->display),
  };
  void kbl_focout_cb(struct wl_resource * kb, void *d) {
    struct kb_leave_foctx *c = d;
    wl_keyboard_send_leave(kb, c->serial, c->surf);
  }
  bridge_for_each_keyboard(surface, kbl_focout_cb, &klfo);

  uint32_t width = 0, height = 0;
  if (surface->plexy_window) {
    plexy_window_get_size(surface->plexy_window, &width, &height);
  }
  send_xdg_toplevel_configure(surface, width, height);
}

void forward_configure(PlexyWindow *window, uint32_t width, uint32_t height,
                       uint32_t state_flags, void *user_data) {
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface)) {
    LOG_WARN("configure: missing bridge or surface");
    return;
  }

  LOG_DEBUG("configure: %ux%u flags=0x%x for %s window", width, height,
            state_flags, surface->is_x11 ? "X11" : "Wayland");

  const bool incoming_fullscreen = (state_flags & PLEXY_STATE_FULLSCREEN) != 0;
  surface->maximized = (state_flags & PLEXY_STATE_MAXIMIZED) != 0;
  surface->fullscreen = incoming_fullscreen;
  surface->activated = (state_flags & PLEXY_STATE_ACTIVATED) != 0;
  surface->resizing = (state_flags & PLEXY_STATE_RESIZING) != 0;
  surface->tiled_left = (state_flags & PLEXY_STATE_TILED_LEFT) != 0;
  surface->tiled_right = (state_flags & PLEXY_STATE_TILED_RIGHT) != 0;
  surface->suspended = (state_flags & PLEXY_STATE_SUSPENDED) != 0;

  if (width == 0 || height == 0) {
    width = surface->last_configured_width;
    height = surface->last_configured_height;
    if (width == 0 || height == 0)
      return;
  }
  surface->last_configured_width = width;
  surface->last_configured_height = height;
  surface->screen_width = width;
  surface->screen_height = height;

  if (surface->is_x11 && surface->xwm_window) {
    struct xwm_window *xwm_win = (struct xwm_window *)surface->xwm_window;

    if (xwm_win->fullscreen && !incoming_fullscreen) {
      LOG_INFO("configure: ignoring stale non-fullscreen configure %ux%u for "
               "fullscreen X11 window %u",
               width, height, xwm_win->id);
      surface->fullscreen = true;
      surface->maximized = false;
      surface->screen_x = xwm_win->x;
      surface->screen_y = xwm_win->y;
      surface->screen_width = xwm_win->width;
      surface->screen_height = xwm_win->height;
      surface->screen_pos_valid = true;
      surface->last_configured_width = xwm_win->width;
      surface->last_configured_height = xwm_win->height;
      return;
    }

    bool old_fs = xwm_win->fullscreen;
    bool old_max = (xwm_win->maximized_vert && xwm_win->maximized_horz);
    xwm_win->fullscreen = surface->fullscreen;
    xwm_win->maximized_vert = surface->maximized;
    xwm_win->maximized_horz = surface->maximized;
    if (xwm_win->fullscreen != old_fs ||
        (xwm_win->maximized_vert && xwm_win->maximized_horz) != old_max)
      xwm_window_set_net_wm_state(xwm_win);

    xwm_window_configure(xwm_win, surface->screen_x, surface->screen_y, width,
                         height);
    LOG_DEBUG("Sent ConfigureNotify to X11 window: %dx%d", width, height);
    return;
  }

  if (!surface->xdg_toplevel || !surface->xdg_surface) {
    LOG_WARN("configure: XDG surface missing resources");
    return;
  }

  send_xdg_toplevel_configure(surface, width, height);
}

void forward_close(PlexyWindow *window, void *user_data) {
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface)) {
    LOG_WARN("close: missing resources");
    return;
  }

  LOG_DEBUG("forward_close: surface=%p is_x11=%d", (void *)surface,
            surface->is_x11);

  if (surface->is_x11 && surface->xwm_window) {
    struct xwm_window *xwm_win = (struct xwm_window *)surface->xwm_window;
    LOG_INFO("Closing X11 window via WM_DELETE_WINDOW");
    xwm_window_close(xwm_win);
    return;
  }

  if (surface->xdg_popup) {
    xdg_popup_send_popup_done(surface->xdg_popup);
    return;
  }

  if (surface->xdg_toplevel) {
    xdg_toplevel_send_close(surface->xdg_toplevel);
  }
}

void forward_frame_done(PlexyWindow *window, void *user_data) {
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface)) {
    return;
  }

  surface->frame_pending = false;

  if (!wl_list_empty(&surface->frame_callbacks)) {
    uint32_t time = get_timestamp_ms();

    struct wl_resource *callback, *tmp;
    wl_resource_for_each_safe(callback, tmp, &surface->frame_callbacks) {
      wl_callback_send_done(callback, time);
      wl_resource_destroy(callback);
    }
  }

  bridge_surface_send_presentation_feedback(surface);
  bridge_fifo_on_frame_done(surface);

  if (surface->previous_buffer) {
    wl_list_remove(&surface->previous_buffer_destroy_listener.link);
    wl_list_init(&surface->previous_buffer_destroy_listener.link);

    bridge_syncobj_signal_release(surface);
    wl_buffer_send_release(surface->previous_buffer);
    surface->previous_buffer = NULL;
  }
}

void forward_enter_output(PlexyWindow *window, uint32_t output_id,
                          void *user_data) {
  (void)window;
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface)) {
    return;
  }

  track_surface_output_enter(surface, output_id);
}

void forward_leave_output(PlexyWindow *window, uint32_t output_id,
                          void *user_data) {
  (void)window;
  struct bridge_surface *surface = (struct bridge_surface *)user_data;
  if (!bridge || !surface_alive(surface)) {
    return;
  }

  track_surface_output_leave(surface, output_id);
}
