/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#define _GNU_SOURCE
#include "text-input-v3-protocol.h"
#include "wayland_bridge.h"
#include "xx-input-method-v2-protocol.h"
#include <linux/input-event-codes.h>
#include <stdlib.h>
#include <string.h>

struct wl_resource *bridge_ime_get_focused_text_input(void);
void bridge_ime_send_text_input_done(struct wl_resource *ti_resource);
extern struct xwm *xwm_get_instance(void);
extern void xwm_inject_text(struct xwm *xwm, const char *text);
extern void xwm_send_key_event(struct xwm *xwm, uint32_t keycode, bool pressed,
                               uint32_t modifiers);

static void positioner_destroy(struct wl_client *client,
                               struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}
static void positioner_set_size(struct wl_client *c, struct wl_resource *r,
                                uint32_t w, uint32_t h) {
  (void)c;
  (void)r;
  (void)w;
  (void)h;
}
static void positioner_set_anchor(struct wl_client *c, struct wl_resource *r,
                                  uint32_t anchor) {
  (void)c;
  (void)r;
  (void)anchor;
}
static void positioner_set_gravity(struct wl_client *c, struct wl_resource *r,
                                   uint32_t g) {
  (void)c;
  (void)r;
  (void)g;
}
static void positioner_set_constraint(struct wl_client *c,
                                      struct wl_resource *r, uint32_t a) {
  (void)c;
  (void)r;
  (void)a;
}
static void positioner_set_offset(struct wl_client *c, struct wl_resource *r,
                                  int32_t x, int32_t y) {
  (void)c;
  (void)r;
  (void)x;
  (void)y;
}
static void positioner_set_reactive(struct wl_client *c,
                                    struct wl_resource *r) {
  (void)c;
  (void)r;
}

static const struct xx_input_popup_positioner_v1_interface positioner_impl = {
    .destroy = positioner_destroy,
    .set_size = positioner_set_size,
    .set_anchor = positioner_set_anchor,
    .set_gravity = positioner_set_gravity,
    .set_constraint_adjustment = positioner_set_constraint,
    .set_offset = positioner_set_offset,
    .set_reactive = positioner_set_reactive,
};

static void positioner_resource_destroy(struct wl_resource *resource) {
  (void)resource;
}

static void popup_destroy(struct wl_client *client,
                          struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}
static void popup_ack_configure(struct wl_client *c, struct wl_resource *r,
                                uint32_t serial) {
  (void)c;
  (void)r;
  (void)serial;
}
static void popup_reposition(struct wl_client *c, struct wl_resource *r,
                             struct wl_resource *positioner, uint32_t token) {
  (void)c;
  (void)r;
  (void)positioner;
  (void)token;
}

static const struct xx_input_popup_surface_v2_interface popup_impl = {
    .destroy = popup_destroy,
    .ack_configure = popup_ack_configure,
    .reposition = popup_reposition,
};

static void popup_resource_destroy(struct wl_resource *resource) {
  (void)resource;
}

static void im_commit_string(struct wl_client *c, struct wl_resource *r,
                             const char *text) {
  (void)c;
  (void)r;
  if (!bridge || !text)
    return;

  struct wl_resource *ti = bridge_ime_get_focused_text_input();
  if (ti) {
    zwp_text_input_v3_send_commit_string(ti, text);
  } else if (bridge->keyboard_focus && bridge->keyboard_focus->is_x11) {
    struct xwm *xwm = xwm_get_instance();
    if (xwm)
      xwm_inject_text(xwm, text);
  }
}

static void im_set_preedit_string(struct wl_client *c, struct wl_resource *r,
                                  const char *text, int32_t begin,
                                  int32_t end) {
  (void)c;
  (void)r;
  if (!bridge || !text)
    return;

  struct wl_resource *ti = bridge_ime_get_focused_text_input();
  if (ti) {
    zwp_text_input_v3_send_preedit_string(ti, text, begin, end);
  }
}

static void im_delete_surrounding(struct wl_client *c, struct wl_resource *r,
                                  uint32_t before, uint32_t after) {
  (void)c;
  (void)r;
  if (!bridge)
    return;

  struct wl_resource *ti = bridge_ime_get_focused_text_input();
  if (ti) {
    zwp_text_input_v3_send_delete_surrounding_text(ti, before, after);
  } else if (bridge->keyboard_focus && bridge->keyboard_focus->is_x11) {
    struct xwm *xwm = xwm_get_instance();
    if (xwm) {
      for (uint32_t i = 0; i < before; i++) {
        xwm_send_key_event(xwm, KEY_BACKSPACE, true, 0);
        xwm_send_key_event(xwm, KEY_BACKSPACE, false, 0);
      }
      for (uint32_t i = 0; i < after; i++) {
        xwm_send_key_event(xwm, KEY_DELETE, true, 0);
        xwm_send_key_event(xwm, KEY_DELETE, false, 0);
      }
    }
  }
}

static void im_commit(struct wl_client *c, struct wl_resource *r,
                      uint32_t serial) {
  (void)c;
  (void)r;
  (void)serial;
  if (!bridge)
    return;

  struct wl_resource *ti = bridge_ime_get_focused_text_input();
  if (ti) {

    bridge_ime_send_text_input_done(ti);
  }
}

static void im_get_input_popup_surface(struct wl_client *client,
                                       struct wl_resource *resource,
                                       uint32_t id, struct wl_resource *surface,
                                       struct wl_resource *positioner) {
  (void)surface;
  (void)positioner;
  struct wl_resource *popup =
      wl_resource_create(client, &xx_input_popup_surface_v2_interface, 1, id);
  if (!popup) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(popup, &popup_impl, NULL,
                                 popup_resource_destroy);
  (void)resource;
}

static void im_destroy(struct wl_client *client, struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void im_move_cursor(struct wl_client *c, struct wl_resource *r,
                           int32_t cursor, int32_t anchor) {
  (void)c;
  (void)r;
  (void)cursor;
  (void)anchor;
}

static void im_perform_action(struct wl_client *c, struct wl_resource *r,
                              uint32_t action) {
  (void)c;
  (void)r;
  (void)action;
}

static const struct xx_input_method_v1_interface input_method_impl = {
    .commit_string = im_commit_string,
    .set_preedit_string = im_set_preedit_string,
    .delete_surrounding_text = im_delete_surrounding,
    .commit = im_commit,
    .get_input_popup_surface = im_get_input_popup_surface,
    .destroy = im_destroy,
    .move_cursor = im_move_cursor,
    .perform_action = im_perform_action,
};

static void input_method_resource_destroy(struct wl_resource *resource) {
  if (bridge && bridge->active_input_method == resource)
    bridge->active_input_method = NULL;
}

static void manager_get_input_method(struct wl_client *client,
                                     struct wl_resource *resource,
                                     struct wl_resource *seat, uint32_t id) {
  (void)seat;
  uint32_t version = wl_resource_get_version(resource);
  struct wl_resource *im = wl_resource_create(
      client, &xx_input_method_v1_interface, version < 4 ? version : 4, id);
  if (!im) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(im, &input_method_impl, NULL,
                                 input_method_resource_destroy);

  if (bridge)
    bridge->active_input_method = im;
}

static void manager_get_positioner(struct wl_client *client,
                                   struct wl_resource *resource, uint32_t id) {
  (void)resource;
  struct wl_resource *pos = wl_resource_create(
      client, &xx_input_popup_positioner_v1_interface, 1, id);
  if (!pos) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(pos, &positioner_impl, NULL,
                                 positioner_resource_destroy);
}

static void manager_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct xx_input_method_manager_v2_interface manager_impl = {
    .get_input_method = manager_get_input_method,
    .get_positioner = manager_get_positioner,
    .destroy = manager_destroy,
};

static void manager_resource_destroy(struct wl_resource *resource) {
  (void)resource;
}

void bind_input_method_manager(struct wl_client *client, void *data,
                               uint32_t version, uint32_t id) {
  (void)data;
  struct wl_resource *resource = wl_resource_create(
      client, &xx_input_method_manager_v2_interface, version, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &manager_impl, NULL,
                                 manager_resource_destroy);
}

void bridge_ime_forward_text_input_state(struct wl_resource *im);

void bridge_ime_notify_text_input_state(bool enabled) {
  if (!bridge || !bridge->active_input_method)
    return;

  struct wl_resource *im = bridge->active_input_method;

  if (enabled) {
    xx_input_method_v1_send_activate(im);
    bridge_ime_forward_text_input_state(im);
    xx_input_method_v1_send_done(im);
  } else {
    xx_input_method_v1_send_deactivate(im);
    xx_input_method_v1_send_done(im);
  }
}
