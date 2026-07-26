/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#define _POSIX_C_SOURCE 200809L
#include "wayland_bridge.h"
#include "xdg-decoration-v1-protocol.h"
#include "xdg-shell-protocol.h"
#include "xwm.h"
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-protocol.h>

struct xdg_configure {
  struct wl_list link;
  uint32_t serial;
  uint32_t width;
  uint32_t height;
  bool maximized;
  bool fullscreen;
  bool activated;
  bool resizing;
};

static bool ascii_contains_ci(const char *haystack, const char *needle) {
  if (!haystack || !needle || !*needle)
    return false;

  for (const char *h = haystack; *h; ++h) {
    const char *hp = h;
    const char *np = needle;
    while (*hp && *np &&
           tolower((unsigned char)*hp) == tolower((unsigned char)*np)) {
      ++hp;
      ++np;
    }
    if (!*np)
      return true;
  }
  return false;
}

static bool xdg_title_suggests_splash(const char *title) {
  return ascii_contains_ci(title, "splash") ||
         ascii_contains_ci(title, "startup") ||
         ascii_contains_ci(title, "starting");
}

static bool xdg_toplevel_has_fixed_small_size(struct bridge_surface *surface) {
  if (!surface)
    return false;
  if (surface->min_width <= 0 || surface->min_height <= 0)
    return false;
  if (surface->max_width != surface->min_width ||
      surface->max_height != surface->min_height)
    return false;
  return surface->min_width <= 720 && surface->min_height <= 720;
}

static bool
xdg_toplevel_fixed_size_suggests_splash(struct bridge_surface *surface) {
  if (!xdg_toplevel_has_fixed_small_size(surface))
    return false;
  if (xdg_title_suggests_splash(surface->title))
    return true;
  return ascii_contains_ci(surface->title, "gimp") ||
         ascii_contains_ci(surface->app_id, "gimp");
}

static uint32_t xdg_toplevel_protocol_type(struct bridge_surface *surface) {
  if (!surface)
    return PLEXY_WINDOW_TYPE_NORMAL;
  if (surface->is_modal || surface->toplevel_parent)
    return PLEXY_WINDOW_TYPE_DIALOG;
  if (xdg_title_suggests_splash(surface->title) ||
      xdg_toplevel_fixed_size_suggests_splash(surface))
    return PLEXY_WINDOW_TYPE_SPLASH;
  return PLEXY_WINDOW_TYPE_NORMAL;
}

static PlexyWindow *
xdg_toplevel_parent_plexy_window(struct bridge_surface *surface) {
  if (!surface || !surface->toplevel_parent)
    return NULL;
  return surface->toplevel_parent->plexy_window;
}

static void xdg_toplevel_sync_parent(struct bridge_surface *surface) {
  if (!surface || !surface->plexy_window || surface->is_x11)
    return;
  plexy_window_set_parent(surface->plexy_window,
                          xdg_toplevel_parent_plexy_window(surface));
}

static void xdg_toplevel_sync_type(struct bridge_surface *surface) {
  if (!surface || !surface->plexy_window || surface->is_x11)
    return;
  plexy_window_set_type(surface->plexy_window,
                        xdg_toplevel_protocol_type(surface));
}

static struct xdg_configure *
xdg_configure_create(struct bridge_surface *surface, uint32_t width,
                     uint32_t height) {
  struct xdg_configure *cfg = calloc(1, sizeof(*cfg));
  if (!cfg)
    return NULL;

  surface->configure_serial++;
  cfg->serial = surface->configure_serial;
  cfg->width = width;
  cfg->height = height;
  cfg->maximized = surface->maximized;
  cfg->fullscreen = surface->fullscreen;
  cfg->activated = surface->activated;
  cfg->resizing = surface->resizing;

  wl_list_insert(surface->pending_configures.prev, &cfg->link);
  return cfg;
}

struct xdg_positioner_data {
  int32_t width;
  int32_t height;
  int32_t anchor_x;
  int32_t anchor_y;
  int32_t anchor_width;
  int32_t anchor_height;
  uint32_t anchor;
  uint32_t gravity;
  uint32_t constraint_adjustment;
  int32_t offset_x;
  int32_t offset_y;
  bool reactive;
  int32_t parent_width;
  int32_t parent_height;
  uint32_t parent_configure_serial;
};

static void positioner_destroy(struct wl_client *client,
                               struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void positioner_set_size(struct wl_client *client,
                                struct wl_resource *resource, int32_t width,
                                int32_t height) {
  struct xdg_positioner_data *data = wl_resource_get_user_data(resource);
  if (data) {
    data->width = width;
    data->height = height;
  }
}

static void positioner_set_anchor_rect(struct wl_client *client,
                                       struct wl_resource *resource, int32_t x,
                                       int32_t y, int32_t width,
                                       int32_t height) {
  struct xdg_positioner_data *data = wl_resource_get_user_data(resource);
  if (data) {
    data->anchor_x = x;
    data->anchor_y = y;
    data->anchor_width = width;
    data->anchor_height = height;
  }
}

static void positioner_set_anchor(struct wl_client *client,
                                  struct wl_resource *resource,
                                  uint32_t anchor) {
  struct xdg_positioner_data *data = wl_resource_get_user_data(resource);
  if (data) {
    data->anchor = anchor;
  }
}

static void positioner_set_gravity(struct wl_client *client,
                                   struct wl_resource *resource,
                                   uint32_t gravity) {
  struct xdg_positioner_data *data = wl_resource_get_user_data(resource);
  if (data) {
    data->gravity = gravity;
  }
}

static void
positioner_set_constraint_adjustment(struct wl_client *client,
                                     struct wl_resource *resource,
                                     uint32_t constraint_adjustment) {
  struct xdg_positioner_data *data = wl_resource_get_user_data(resource);
  if (data) {
    data->constraint_adjustment = constraint_adjustment;
  }
}

static void positioner_set_offset(struct wl_client *client,
                                  struct wl_resource *resource, int32_t x,
                                  int32_t y) {
  struct xdg_positioner_data *data = wl_resource_get_user_data(resource);
  if (data) {
    data->offset_x = x;
    data->offset_y = y;
  }
}

static void positioner_set_reactive(struct wl_client *client,
                                    struct wl_resource *resource) {
  struct xdg_positioner_data *data = wl_resource_get_user_data(resource);
  if (data) {
    data->reactive = true;
  }
}

static void positioner_set_parent_size(struct wl_client *client,
                                       struct wl_resource *resource,
                                       int32_t parent_width,
                                       int32_t parent_height) {
  struct xdg_positioner_data *data = wl_resource_get_user_data(resource);
  if (data) {
    data->parent_width = parent_width;
    data->parent_height = parent_height;
  }
}

static void positioner_set_parent_configure(struct wl_client *client,
                                            struct wl_resource *resource,
                                            uint32_t serial) {
  struct xdg_positioner_data *data = wl_resource_get_user_data(resource);
  if (data) {
    data->parent_configure_serial = serial;
  }
}

static const struct xdg_positioner_interface positioner_implementation = {
    .destroy = positioner_destroy,
    .set_size = positioner_set_size,
    .set_anchor_rect = positioner_set_anchor_rect,
    .set_anchor = positioner_set_anchor,
    .set_gravity = positioner_set_gravity,
    .set_constraint_adjustment = positioner_set_constraint_adjustment,
    .set_offset = positioner_set_offset,
    .set_reactive = positioner_set_reactive,
    .set_parent_size = positioner_set_parent_size,
    .set_parent_configure = positioner_set_parent_configure,
};

static void positioner_resource_destroy(struct wl_resource *resource) {
  struct xdg_positioner_data *data = wl_resource_get_user_data(resource);
  free(data);
}

static void positioner_calculate_position(struct xdg_positioner_data *pos,
                                          int32_t *out_x, int32_t *out_y) {
  if (!pos) {
    *out_x = 0;
    *out_y = 0;
    return;
  }

  int32_t anchor_x = pos->anchor_x;
  int32_t anchor_y = pos->anchor_y;

  switch (pos->anchor) {
  case XDG_POSITIONER_ANCHOR_TOP:
    anchor_x += pos->anchor_width / 2;
    break;
  case XDG_POSITIONER_ANCHOR_BOTTOM:
    anchor_x += pos->anchor_width / 2;
    anchor_y += pos->anchor_height;
    break;
  case XDG_POSITIONER_ANCHOR_LEFT:
    anchor_y += pos->anchor_height / 2;
    break;
  case XDG_POSITIONER_ANCHOR_RIGHT:
    anchor_x += pos->anchor_width;
    anchor_y += pos->anchor_height / 2;
    break;
  case XDG_POSITIONER_ANCHOR_TOP_LEFT:
    break;
  case XDG_POSITIONER_ANCHOR_BOTTOM_LEFT:
    anchor_y += pos->anchor_height;
    break;
  case XDG_POSITIONER_ANCHOR_TOP_RIGHT:
    anchor_x += pos->anchor_width;
    break;
  case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT:
    anchor_x += pos->anchor_width;
    anchor_y += pos->anchor_height;
    break;
  case XDG_POSITIONER_ANCHOR_NONE:
  default:
    anchor_x += pos->anchor_width / 2;
    anchor_y += pos->anchor_height / 2;
    break;
  }

  int32_t popup_x = anchor_x;
  int32_t popup_y = anchor_y;

  switch (pos->gravity) {
  case XDG_POSITIONER_GRAVITY_TOP:
    popup_x -= pos->width / 2;
    popup_y -= pos->height;
    break;
  case XDG_POSITIONER_GRAVITY_BOTTOM:
    popup_x -= pos->width / 2;
    break;
  case XDG_POSITIONER_GRAVITY_LEFT:
    popup_x -= pos->width;
    popup_y -= pos->height / 2;
    break;
  case XDG_POSITIONER_GRAVITY_RIGHT:
    popup_y -= pos->height / 2;
    break;
  case XDG_POSITIONER_GRAVITY_TOP_LEFT:
    popup_x -= pos->width;
    popup_y -= pos->height;
    break;
  case XDG_POSITIONER_GRAVITY_BOTTOM_LEFT:
    popup_x -= pos->width;
    break;
  case XDG_POSITIONER_GRAVITY_TOP_RIGHT:
    popup_y -= pos->height;
    break;
  case XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT:

    break;
  case XDG_POSITIONER_GRAVITY_NONE:
  default:
    popup_x -= pos->width / 2;
    popup_y -= pos->height / 2;
    break;
  }

  *out_x = popup_x + pos->offset_x;
  *out_y = popup_y + pos->offset_y;
}

static void send_toplevel_configure(struct bridge_surface *surface) {
  if (!surface || !surface->xdg_toplevel || !surface->xdg_surface)
    return;

  uint32_t width = 0, height = 0;
  if (surface->plexy_window) {
    plexy_window_get_size(surface->plexy_window, &width, &height);
  }
  uint32_t configure_width = width;
  uint32_t configure_height = height;
  bridge_physical_to_surface_size(surface, width, height, &configure_width,
                                  &configure_height);

  struct wl_array states;
  wl_array_init(&states);

  uint32_t *state;
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

  bridge_send_xdg_toplevel_bounds(surface);

  struct xdg_configure *cfg =
      xdg_configure_create(surface, configure_width, configure_height);
  if (!cfg) {
    wl_array_release(&states);
    return;
  }

  xdg_toplevel_send_configure(surface->xdg_toplevel, configure_width,
                              configure_height, &states);

  xdg_surface_send_configure(surface->xdg_surface, cfg->serial);

  if (surface->xdg_toplevel_decoration) {
    uint32_t mode = surface->xdg_decoration_mode
                        ? surface->xdg_decoration_mode
                        : ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
    zxdg_toplevel_decoration_v1_send_configure(surface->xdg_toplevel_decoration,
                                               mode);
  }

  wl_array_release(&states);

  LOG_TRACE("xdg_toplevel: sent configure serial=%u size=%ux%u activated=%d",
            cfg->serial, width, height, surface->activated);
}

static void xdg_toplevel_destroy(struct wl_client *client,
                                 struct wl_resource *resource) {
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (surface) {
    surface->xdg_toplevel = NULL;
    surface->xdg_toplevel_decoration = NULL;
    surface->xdg_decoration_mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
  }
  wl_resource_destroy(resource);
}

static void xdg_toplevel_set_parent(struct wl_client *client,
                                    struct wl_resource *resource,
                                    struct wl_resource *parent) {
  (void)client;
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (!surface)
    return;

  if (!parent) {
    surface->toplevel_parent = NULL;
    if (surface->plexy_window) {
      xdg_toplevel_sync_parent(surface);
      if (!surface->is_modal) {
        xdg_toplevel_sync_type(surface);
      }
    }
    LOG_DEBUG("xdg_toplevel: cleared parent for surface=%p", (void *)surface);
    return;
  }

  struct bridge_surface *parent_surface = wl_resource_get_user_data(parent);
  if (!parent_surface || parent_surface == surface) {
    wl_resource_post_error(resource, XDG_TOPLEVEL_ERROR_INVALID_PARENT,
                           "invalid xdg_toplevel parent");
    return;
  }

  surface->toplevel_parent = parent_surface;
  if (surface->plexy_window) {
    xdg_toplevel_sync_parent(surface);
    xdg_toplevel_sync_type(surface);
  }
  LOG_DEBUG("xdg_toplevel: set parent surface=%p parent=%p", (void *)surface,
            (void *)parent_surface);
}

static void xdg_toplevel_set_title(struct wl_client *client,
                                   struct wl_resource *resource,
                                   const char *title) {
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (!surface)
    return;

  free(surface->title);
  surface->title = title ? strdup(title) : NULL;

  if (surface->plexy_window && title)
    plexy_window_set_title(surface->plexy_window, title);
  if (surface->plexy_window && !surface->toplevel_parent &&
      !surface->is_modal) {
    xdg_toplevel_sync_type(surface);
  }

  LOG_DEBUG("xdg_toplevel: set_title '%s'", title ? title : "(null)");
}

static void xdg_toplevel_set_app_id(struct wl_client *client,
                                    struct wl_resource *resource,
                                    const char *app_id) {
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (!surface)
    return;

  free(surface->app_id);
  surface->app_id = app_id ? strdup(app_id) : NULL;

  if (surface->plexy_window && app_id)
    plexy_window_set_app_id(surface->plexy_window, app_id);
  if (surface->plexy_window && !surface->toplevel_parent &&
      !surface->is_modal) {
    xdg_toplevel_sync_type(surface);
  }

  LOG_DEBUG("xdg_toplevel: set_app_id '%s'", app_id ? app_id : "(null)");
}

static void xdg_toplevel_show_window_menu(struct wl_client *client,
                                          struct wl_resource *resource,
                                          struct wl_resource *seat,
                                          uint32_t serial, int32_t x,
                                          int32_t y) {
  (void)client;
  (void)seat;
  (void)serial;

  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (!surface || !surface->plexy_window)
    return;

  const char *app_title =
      surface->title ? surface->title
                     : (surface->app_id ? surface->app_id : "Wayland Client");

  if (plexy_window_menu_begin(surface->plexy_window, true, app_title) < 0) {
    LOG_WARN("show_window_menu: menu_begin failed for surface=%p",
             (void *)surface);
    return;
  }

  if (plexy_window_menu_add(surface->plexy_window, 1u, "Window") < 0 ||
      plexy_window_menu_add_item(surface->plexy_window, 1u, 1002u, "Quit",
                                 true) < 0 ||
      plexy_window_menu_add(surface->plexy_window, 2u, "File") < 0 ||
      plexy_window_menu_add_item(surface->plexy_window, 2u, 1101u,
                                 "Close Window", true) < 0) {
    LOG_WARN("show_window_menu: menu population failed for surface=%p",
             (void *)surface);
  }

  if (plexy_window_menu_commit(surface->plexy_window) < 0) {
    LOG_WARN("show_window_menu: menu_commit failed for surface=%p",
             (void *)surface);
  }

  LOG_DEBUG("xdg_toplevel: show_window_menu for surface=%p at (%d,%d)",
            (void *)surface, x, y);
}

static void xdg_toplevel_move(struct wl_client *client,
                              struct wl_resource *resource,
                              struct wl_resource *seat, uint32_t serial) {
  (void)client;
  (void)seat;
  (void)serial;

  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (!surface || !surface->plexy_window)
    return;

  plexy_window_request_move(surface->plexy_window);
}

static uint32_t map_xdg_edges_to_plexy(uint32_t xdg_edges) {

  uint32_t edges = 0;
  if (xdg_edges & XDG_TOPLEVEL_RESIZE_EDGE_LEFT)
    edges |= 1u;
  if (xdg_edges & XDG_TOPLEVEL_RESIZE_EDGE_RIGHT)
    edges |= 2u;
  if (xdg_edges & XDG_TOPLEVEL_RESIZE_EDGE_TOP)
    edges |= 4u;
  if (xdg_edges & XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM)
    edges |= 8u;
  return edges;
}

static void xdg_toplevel_resize(struct wl_client *client,
                                struct wl_resource *resource,
                                struct wl_resource *seat, uint32_t serial,
                                uint32_t edges) {
  (void)client;
  (void)seat;
  (void)serial;

  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (!surface || !surface->plexy_window)
    return;

  uint32_t mapped_edges = map_xdg_edges_to_plexy(edges);
  if (mapped_edges == 0)
    return;
  plexy_window_request_resize(surface->plexy_window, mapped_edges);
}

static void xdg_toplevel_set_max_size(struct wl_client *client,
                                      struct wl_resource *resource,
                                      int32_t width, int32_t height) {
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (surface) {
    surface->max_width = width;
    surface->max_height = height;
    if (surface->plexy_window)
      plexy_window_set_geometry_hints(surface->plexy_window, surface->min_width,
                                      surface->min_height, width, height);
    if (surface->plexy_window && !surface->toplevel_parent &&
        !surface->is_modal) {
      xdg_toplevel_sync_type(surface);
    }
  }
}

static void xdg_toplevel_set_min_size(struct wl_client *client,
                                      struct wl_resource *resource,
                                      int32_t width, int32_t height) {
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (surface) {
    surface->min_width = width;
    surface->min_height = height;
    if (surface->plexy_window)
      plexy_window_set_geometry_hints(surface->plexy_window, width, height,
                                      surface->max_width, surface->max_height);
    if (surface->plexy_window && !surface->toplevel_parent &&
        !surface->is_modal) {
      xdg_toplevel_sync_type(surface);
    }
  }
}

static void xdg_toplevel_set_maximized(struct wl_client *client,
                                       struct wl_resource *resource) {
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (!surface)
    return;

  surface->maximized = true;
  if (surface->plexy_window) {
    plexy_window_request_maximize(surface->plexy_window, true);
  }
}

static void xdg_toplevel_unset_maximized(struct wl_client *client,
                                         struct wl_resource *resource) {
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (!surface)
    return;

  surface->maximized = false;
  if (surface->plexy_window) {
    plexy_window_request_maximize(surface->plexy_window, false);
  }
}

static void xdg_toplevel_set_fullscreen(struct wl_client *client,
                                        struct wl_resource *resource,
                                        struct wl_resource *output) {
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (!surface)
    return;

  surface->fullscreen = true;
  if (surface->plexy_window)
    plexy_window_request_fullscreen(surface->plexy_window, true);

  send_toplevel_configure(surface);
}

static void xdg_toplevel_unset_fullscreen(struct wl_client *client,
                                          struct wl_resource *resource) {
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (!surface)
    return;

  surface->fullscreen = false;
  if (surface->plexy_window)
    plexy_window_request_fullscreen(surface->plexy_window, false);
  send_toplevel_configure(surface);
}

static void xdg_toplevel_set_minimized(struct wl_client *client,
                                       struct wl_resource *resource) {
  (void)client;
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (!surface || !surface->plexy_window)
    return;
  plexy_window_request_minimize(surface->plexy_window);
}

static const struct xdg_toplevel_interface xdg_toplevel_implementation = {
    .destroy = xdg_toplevel_destroy,
    .set_parent = xdg_toplevel_set_parent,
    .set_title = xdg_toplevel_set_title,
    .set_app_id = xdg_toplevel_set_app_id,
    .show_window_menu = xdg_toplevel_show_window_menu,
    .move = xdg_toplevel_move,
    .resize = xdg_toplevel_resize,
    .set_max_size = xdg_toplevel_set_max_size,
    .set_min_size = xdg_toplevel_set_min_size,
    .set_maximized = xdg_toplevel_set_maximized,
    .unset_maximized = xdg_toplevel_unset_maximized,
    .set_fullscreen = xdg_toplevel_set_fullscreen,
    .unset_fullscreen = xdg_toplevel_unset_fullscreen,
    .set_minimized = xdg_toplevel_set_minimized,
};

static void xdg_surface_destroy(struct wl_client *client,
                                struct wl_resource *resource) {
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (surface) {
    surface->xdg_surface = NULL;

    struct xdg_configure *cfg, *tmp;
    wl_list_for_each_safe(cfg, tmp, &surface->pending_configures, link) {
      wl_list_remove(&cfg->link);
      free(cfg);
    }
  }
  wl_resource_destroy(resource);
}

static void xdg_surface_get_toplevel(struct wl_client *client,
                                     struct wl_resource *resource,
                                     uint32_t id) {
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (!surface)
    return;

  struct wl_resource *toplevel = wl_resource_create(
      client, &xdg_toplevel_interface, wl_resource_get_version(resource), id);

  if (!toplevel) {
    wl_resource_post_no_memory(resource);
    return;
  }

  surface->xdg_toplevel = toplevel;
  surface->activated = true;

  wl_resource_set_implementation(toplevel, &xdg_toplevel_implementation,
                                 surface, NULL);

  if (bridge_is_xwayland_client(client)) {
    LOG_DEBUG("xdg_toplevel: deferring window creation for Xwayland surface");
    return;
  }

  const char *title = surface->title ? surface->title : "Wayland Client";
  const float scale = plexy_get_ui_scale(bridge->plexy_conn);
  const uint32_t def_w = (uint32_t)(800.0f * scale);
  const uint32_t def_h = (uint32_t)(600.0f * scale);
  PlexyWindow *window =
      plexy_create_window_typed(bridge->plexy_conn, 100, 100, def_w, def_h,
                                title, xdg_toplevel_protocol_type(surface),
                                xdg_toplevel_parent_plexy_window(surface));

  if (!window) {
    wl_resource_post_error(resource, XDG_SURFACE_ERROR_NOT_CONSTRUCTED,
                           "Failed to create window");
    return;
  }

  bool server_side = surface->xdg_decoration_mode ==
                     ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
  plexy_window_set_decorations(window, server_side);

  bridge_surface_set_plexy_window(surface, window);
  if (surface->icon_name)
    plexy_window_set_icon_name(window, surface->icon_name);

  surface->screen_x = 100;
  surface->screen_y = 100;
  surface->screen_width = def_w;
  surface->screen_height = def_h;
  surface->screen_pos_valid = true;

  LOG_DEBUG("xdg_toplevel: created window id=%u", surface->plexy_window_id);

  PlexyWindowCallbacks callbacks = {
      .configure = forward_configure,
      .close = forward_close,
      .pointer_enter = forward_pointer_enter,
      .pointer_leave = forward_pointer_leave,
      .pointer_motion = forward_pointer_motion,
      .pointer_button = forward_pointer_button,
      .pointer_axis = forward_pointer_axis,
      .touch_down = forward_touch_down,
      .touch_motion = forward_touch_motion,
      .touch_up = forward_touch_up,
      .touch_cancel = forward_touch_cancel,
      .touch_frame = forward_touch_frame,
      .key = forward_key,
      .modifiers = forward_modifiers,
      .focus_in = forward_focus_in,
      .focus_out = forward_focus_out,
      .frame_done = forward_frame_done,
      .scale_changed = NULL,
      .enter_output = forward_enter_output,
      .leave_output = forward_leave_output,
  };

  plexy_window_set_callbacks(window, &callbacks, surface);
  bridge_sync_input_region_to_plexy(surface);

  uint32_t width, height;
  plexy_window_get_size(window, &width, &height);

  uint32_t toplevel_version = wl_resource_get_version(toplevel);
  if (toplevel_version >= XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION) {
    struct wl_array capabilities;
    wl_array_init(&capabilities);

    uint32_t *cap;
    cap = wl_array_add(&capabilities, sizeof(*cap));
    if (cap)
      *cap = XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE;
    cap = wl_array_add(&capabilities, sizeof(*cap));
    if (cap)
      *cap = XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN;
    cap = wl_array_add(&capabilities, sizeof(*cap));
    if (cap)
      *cap = XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE;
    cap = wl_array_add(&capabilities, sizeof(*cap));
    if (cap)
      *cap = XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU;

    xdg_toplevel_send_wm_capabilities(toplevel, &capabilities);
    wl_array_release(&capabilities);
  }

  send_toplevel_configure(surface);
}

struct xdg_popup_data {
  struct bridge_surface *surface;
  struct wl_resource *popup_resource;
  struct bridge_surface *parent_surface;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
  bool grabbed;
  struct wl_resource *grab_seat;
  uint32_t grab_serial;
};

static void popup_destroy(struct wl_client *client,
                          struct wl_resource *resource) {
  struct xdg_popup_data *data = wl_resource_get_user_data(resource);
  struct bridge_surface *restore_focus_parent = NULL;
  if (data && data->surface) {

    if (bridge && bridge->grabbed_popup == data->surface) {
      bridge->grabbed_popup = NULL;
      if (data->grabbed && data->surface->popup_parent &&
          data->surface->popup_parent->resource &&
          data->surface->popup_parent->plexy_window)
        restore_focus_parent = data->surface->popup_parent;
    }

    if (restore_focus_parent)
      forward_focus_in(restore_focus_parent->plexy_window,
                       restore_focus_parent);

    data->surface->xdg_popup = NULL;

    if (data->surface->plexy_window) {
      PlexyWindow *window = data->surface->plexy_window;
      plexy_destroy_window(window);
      bridge_surface_clear_plexy_window(data->surface);
    }
  }
  wl_resource_destroy(resource);
}

static void popup_grab(struct wl_client *client, struct wl_resource *resource,
                       struct wl_resource *seat, uint32_t serial) {
  struct xdg_popup_data *data = wl_resource_get_user_data(resource);
  if (!data || !data->surface)
    return;

  data->grabbed = true;
  data->grab_seat = seat;
  data->grab_serial = serial;

  if (bridge)
    bridge->grabbed_popup = data->surface;

  if (bridge && data->surface->resource) {

    struct bridge_surface *old_kb = bridge->keyboard_focus;
    if (old_kb && old_kb->resource) {
      struct kb_leave_grab {
        struct wl_resource *surf;
        uint32_t serial;
      } klg = {
          .surf = old_kb->resource,
          .serial = wl_display_next_serial(bridge->display),
      };
      void kbl_cb(struct wl_resource * kb, void *d) {
        struct kb_leave_grab *c = d;
        wl_keyboard_send_leave(kb, c->serial, c->surf);
      }
      bridge_for_each_keyboard(old_kb, kbl_cb, &klg);
    }

    bridge->keyboard_focus = data->surface;
    struct kb_enter_grab {
      struct wl_resource *surf;
      uint32_t serial;
      uint32_t dep, lat, loc, grp;
    } keg = {
        .surf = data->surface->resource,
        .serial = wl_display_next_serial(bridge->display),
        .dep = bridge->mods_depressed,
        .lat = bridge->mods_latched,
        .loc = bridge->mods_locked,
        .grp = bridge->mods_group,
    };
    void kbe_cb(struct wl_resource * kb, void *d) {
      struct kb_enter_grab *c = d;
      struct wl_array keys;
      wl_array_init(&keys);
      wl_keyboard_send_enter(kb, c->serial, c->surf, &keys);
      wl_array_release(&keys);
      uint32_t ms = wl_display_next_serial(
          wl_client_get_display(wl_resource_get_client(kb)));
      wl_keyboard_send_modifiers(kb, ms, c->dep, c->lat, c->loc, c->grp);
    }
    bridge_for_each_keyboard(data->surface, kbe_cb, &keg);
  }

  LOG_DEBUG("xdg_popup: grab requested serial=%u — keyboard redirected",
            serial);
}

static void popup_reposition(struct wl_client *client,
                             struct wl_resource *resource,
                             struct wl_resource *positioner, uint32_t token) {
  struct xdg_popup_data *data = wl_resource_get_user_data(resource);
  if (!data)
    return;

  struct xdg_positioner_data *pos_data = wl_resource_get_user_data(positioner);
  if (pos_data) {
    positioner_calculate_position(pos_data, &data->x, &data->y);
    data->width = pos_data->width > 0 ? pos_data->width : data->width;
    data->height = pos_data->height > 0 ? pos_data->height : data->height;
  }

  if (wl_resource_get_version(resource) >=
      XDG_POPUP_REPOSITIONED_SINCE_VERSION) {
    xdg_popup_send_repositioned(resource, token);
  }

  xdg_popup_send_configure(resource, data->x, data->y, data->width,
                           data->height);

  if (data->surface && data->surface->xdg_surface) {
    data->surface->configure_serial++;
    xdg_surface_send_configure(data->surface->xdg_surface,
                               data->surface->configure_serial);
  }
}

static const struct xdg_popup_interface popup_implementation = {
    .destroy = popup_destroy,
    .grab = popup_grab,
    .reposition = popup_reposition,
};

static void popup_resource_destroy(struct wl_resource *resource) {
  struct xdg_popup_data *data = wl_resource_get_user_data(resource);
  if (data && data->surface) {
    data->surface->xdg_popup = NULL;

    if (data->surface->plexy_window) {
      PlexyWindow *window = data->surface->plexy_window;
      plexy_destroy_window(window);
      bridge_surface_clear_plexy_window(data->surface);
    }
  }
  free(data);
}

static void xdg_surface_get_popup(struct wl_client *client,
                                  struct wl_resource *resource, uint32_t id,
                                  struct wl_resource *parent,
                                  struct wl_resource *positioner) {
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (!surface)
    return;

  struct wl_resource *popup = wl_resource_create(
      client, &xdg_popup_interface, wl_resource_get_version(resource), id);
  if (!popup) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct xdg_popup_data *data = calloc(1, sizeof(*data));
  if (!data) {
    wl_resource_destroy(popup);
    wl_resource_post_no_memory(resource);
    return;
  }

  data->surface = surface;
  data->popup_resource = popup;
  surface->xdg_popup = popup;

  if (surface->plexy_window) {
    PlexyWindow *window = surface->plexy_window;
    plexy_destroy_window(window);
    bridge_surface_clear_plexy_window(surface);
  }

  struct bridge_surface *parent_surface = NULL;
  if (parent) {
    parent_surface = wl_resource_get_user_data(parent);
    data->parent_surface = parent_surface;
    surface->popup_parent = parent_surface;
  }

  struct xdg_positioner_data *pos_data = wl_resource_get_user_data(positioner);
  if (pos_data) {
    positioner_calculate_position(pos_data, &data->x, &data->y);
    data->width = pos_data->width > 0 ? pos_data->width : 200;
    data->height = pos_data->height > 0 ? pos_data->height : 200;

    LOG_DEBUG("xdg_popup positioner: anchor_rect=(%d,%d %dx%d) anchor=%u "
              "gravity=%u offset=(%d,%d) -> pos=(%d,%d) size=%dx%d",
              pos_data->anchor_x, pos_data->anchor_y, pos_data->anchor_width,
              pos_data->anchor_height, pos_data->anchor, pos_data->gravity,
              pos_data->offset_x, pos_data->offset_y, data->x, data->y,
              data->width, data->height);
  } else {
    data->x = 0;
    data->y = 0;
    data->width = 200;
    data->height = 200;
  }

  wl_resource_set_implementation(popup, &popup_implementation, data,
                                 popup_resource_destroy);

  int32_t popup_x = data->x;
  int32_t popup_y = data->y;

  PlexyWindow *parent_plexy_window =
      parent_surface ? parent_surface->plexy_window : NULL;

  PlexyWindow *popup_window = plexy_create_popup(
      bridge->plexy_conn, parent_plexy_window, popup_x, popup_y, data->width,
      data->height, PLEXY_POPUP_FLAG_NONE);
  if (popup_window) {
    bridge_surface_set_plexy_window(surface, popup_window);

    PlexyWindowCallbacks callbacks = {
        .configure = forward_configure,
        .close = forward_close,
        .pointer_enter = forward_pointer_enter,
        .pointer_leave = forward_pointer_leave,
        .pointer_motion = forward_pointer_motion,
        .pointer_button = forward_pointer_button,
        .pointer_axis = forward_pointer_axis,
        .touch_down = forward_touch_down,
        .touch_motion = forward_touch_motion,
        .touch_up = forward_touch_up,
        .touch_cancel = forward_touch_cancel,
        .touch_frame = forward_touch_frame,
        .key = forward_key,
        .modifiers = forward_modifiers,
        .focus_in = forward_focus_in,
        .focus_out = forward_focus_out,
        .frame_done = forward_frame_done,
    };
    plexy_window_set_callbacks(popup_window, &callbacks, surface);
    bridge_sync_input_region_to_plexy(surface);

    LOG_DEBUG("xdg_popup: created popup id=%u parent=%u at (%d,%d) size=%dx%d",
              surface->plexy_window_id,
              parent_plexy_window ? plexy_window_get_id(parent_plexy_window)
                                  : 0,
              popup_x, popup_y, data->width, data->height);
  }

  xdg_popup_send_configure(popup, data->x, data->y, data->width, data->height);

  surface->configure_serial++;
  xdg_surface_send_configure(resource, surface->configure_serial);
}

static void xdg_surface_set_window_geometry(struct wl_client *client,
                                            struct wl_resource *resource,
                                            int32_t x, int32_t y, int32_t width,
                                            int32_t height) {
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (!surface)
    return;

  surface->window_geometry.x = x;
  surface->window_geometry.y = y;
  surface->window_geometry.width = width;
  surface->window_geometry.height = height;
  surface->geometry_set = true;

  LOG_TRACE("xdg_surface: set_window_geometry x=%d y=%d w=%d h=%d", x, y, width,
            height);
}

static void xdg_surface_ack_configure(struct wl_client *client,
                                      struct wl_resource *resource,
                                      uint32_t serial) {
  struct bridge_surface *surface = wl_resource_get_user_data(resource);
  if (!surface)
    return;

  struct xdg_configure *cfg, *tmp;
  bool found = false;

  wl_list_for_each_safe(cfg, tmp, &surface->pending_configures, link) {
    if (cfg->serial == serial) {
      found = true;
    }
    if (cfg->serial <= serial) {
      wl_list_remove(&cfg->link);
      free(cfg);
    }
  }

  if (!found && serial != 0) {
    LOG_TRACE("xdg_surface: ack_configure for serial %u (not in pending list)",
              serial);
  }

  surface->last_acked_serial = serial;
  surface->configured = true;
  surface->resizing = false;

  LOG_TRACE("xdg_surface: ack_configure serial=%u", serial);
}

static const struct xdg_surface_interface xdg_surface_implementation = {
    .destroy = xdg_surface_destroy,
    .get_toplevel = xdg_surface_get_toplevel,
    .get_popup = xdg_surface_get_popup,
    .set_window_geometry = xdg_surface_set_window_geometry,
    .ack_configure = xdg_surface_ack_configure,
};

struct xdg_client {
  struct wl_resource *resource;
  struct wl_list link;
  uint32_t ping_serial;
  bool ping_pending;
};

static struct wl_list xdg_clients;
static bool xdg_clients_initialized = false;

static void xdg_wm_base_destroy(struct wl_client *client,
                                struct wl_resource *resource) {

  struct xdg_client *xc, *tmp;
  wl_list_for_each_safe(xc, tmp, &xdg_clients, link) {
    if (xc->resource == resource) {
      wl_list_remove(&xc->link);
      free(xc);
      break;
    }
  }
  wl_resource_destroy(resource);
}

static void xdg_wm_base_create_positioner(struct wl_client *client,
                                          struct wl_resource *resource,
                                          uint32_t id) {
  struct wl_resource *positioner = wl_resource_create(
      client, &xdg_positioner_interface, wl_resource_get_version(resource), id);

  if (!positioner) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct xdg_positioner_data *data = calloc(1, sizeof(*data));
  if (!data) {
    wl_resource_destroy(positioner);
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource_set_implementation(positioner, &positioner_implementation, data,
                                 positioner_resource_destroy);
}

static void xdg_wm_base_get_xdg_surface(struct wl_client *client,
                                        struct wl_resource *resource,
                                        uint32_t id,
                                        struct wl_resource *surface_resource) {
  struct bridge_surface *surface =
      bridge_surface_from_resource(surface_resource);
  if (!surface) {
    wl_resource_post_error(resource, XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE,
                           "surface is null");
    return;
  }

  if (surface->xdg_surface) {
    wl_resource_post_error(resource, XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE,
                           "surface already has an xdg_surface");
    return;
  }

  struct wl_resource *xdg_surface = wl_resource_create(
      client, &xdg_surface_interface, wl_resource_get_version(resource), id);

  if (!xdg_surface) {
    wl_resource_post_no_memory(resource);
    return;
  }

  surface->xdg_surface = xdg_surface;
  wl_list_init(&surface->pending_configures);

  wl_resource_set_implementation(xdg_surface, &xdg_surface_implementation,
                                 surface, NULL);

  LOG_DEBUG("xdg_surface: created for surface=%p", (void *)surface);
}

static void xdg_wm_base_pong(struct wl_client *client,
                             struct wl_resource *resource, uint32_t serial) {

  struct xdg_client *xc;
  wl_list_for_each(xc, &xdg_clients, link) {
    if (xc->resource == resource) {
      if (xc->ping_serial == serial) {
        xc->ping_pending = false;
        LOG_TRACE("xdg_wm_base: pong received serial=%u", serial);
      }
      break;
    }
  }
}

static const struct xdg_wm_base_interface xdg_wm_base_implementation = {
    .destroy = xdg_wm_base_destroy,
    .create_positioner = xdg_wm_base_create_positioner,
    .get_xdg_surface = xdg_wm_base_get_xdg_surface,
    .pong = xdg_wm_base_pong,
};

static void xdg_wm_base_resource_destroy(struct wl_resource *resource) {
  struct xdg_client *xc, *tmp;
  wl_list_for_each_safe(xc, tmp, &xdg_clients, link) {
    if (xc->resource == resource) {
      wl_list_remove(&xc->link);
      free(xc);
      break;
    }
  }
}

void bind_xdg_wm_base(struct wl_client *client, void *data, uint32_t version,
                      uint32_t id) {
  if (!xdg_clients_initialized) {
    wl_list_init(&xdg_clients);
    xdg_clients_initialized = true;
  }

  struct wl_resource *resource =
      wl_resource_create(client, &xdg_wm_base_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  struct xdg_client *xc = calloc(1, sizeof(*xc));
  if (xc) {
    xc->resource = resource;
    wl_list_insert(&xdg_clients, &xc->link);
  }

  wl_resource_set_implementation(resource, &xdg_wm_base_implementation, NULL,
                                 xdg_wm_base_resource_destroy);

  LOG_DEBUG("xdg_wm_base: client bound version=%u", version);
}

void xdg_wm_base_send_pings(void) {
  if (!xdg_clients_initialized)
    return;

  static uint32_t ping_serial = 1;

  struct xdg_client *xc;
  wl_list_for_each(xc, &xdg_clients, link) {
    if (!xc->ping_pending) {
      xc->ping_serial = ping_serial++;
      xc->ping_pending = true;
      xdg_wm_base_send_ping(xc->resource, xc->ping_serial);
    }
  }
}

bool bridge_create_x11_window(struct bridge_surface *surface, const char *title,
                              int32_t x, int32_t y, int32_t width,
                              int32_t height, uint32_t window_type) {
  if (!surface || !bridge) {
    LOG_ERROR("bridge_create_x11_window: invalid surface or bridge");
    return false;
  }

  if (surface->plexy_window) {
    LOG_WARN("bridge_create_x11_window: surface already has a window");
    return true;
  }

  surface->is_x11 = true;

  const char *window_title = title ? title : "X11 Window";

  int fallback_size = 0;
  if (width <= 0) {
    width = 800;
    fallback_size = 1;
  }
  if (height <= 0) {
    height = 600;
    fallback_size = 1;
  }

  float ui_scale = plexy_get_ui_scale(bridge->plexy_conn);
  int32_t scaled_w = fallback_size ? (int32_t)((float)width * ui_scale) : width;
  int32_t scaled_h =
      fallback_size ? (int32_t)((float)height * ui_scale) : height;

  PlexyOutputInfo output_info;
  int32_t out_x = 0, out_y = 0;
  int32_t out_w = INT32_MAX, out_h = INT32_MAX;
  if (bridge_get_default_output_info(&output_info) &&
      output_info.pixel_width > 0 && output_info.pixel_height > 0) {
    out_x = output_info.x;
    out_y = output_info.y;
    out_w = output_info.pixel_width;
    out_h = output_info.pixel_height;
    if (scaled_w > out_w)
      scaled_w = out_w;
    if (scaled_h > out_h)
      scaled_h = out_h;
  }

  struct xwm_window *xwin =
      surface->xwm_window ? (struct xwm_window *)surface->xwm_window : NULL;
  PlexyWindow *transient_parent = NULL;
  if (xwin && xwin->transient_for && xwin->transient_for->surface) {
    transient_parent = xwin->transient_for->surface->plexy_window;
  }

  if (window_type == PLEXY_WINDOW_TYPE_DIALOG && xwin) {
    if (xwin->transient_for && xwin->transient_for->surface &&
        xwin->transient_for->surface->screen_pos_valid) {
      struct bridge_surface *ps = xwin->transient_for->surface;
      x = ps->screen_x + (ps->screen_width - scaled_w) / 2;
      y = ps->screen_y + (ps->screen_height - scaled_h) / 2;
      if (x < 0)
        x = 0;
      if (y < 0)
        y = 0;
      LOG_INFO("Dialog centered on parent: (%d,%d)", x, y);
    }
  } else if (window_type == PLEXY_WINDOW_TYPE_SPLASH && bridge->plexy_conn) {
    if (out_w < INT32_MAX && out_h < INT32_MAX) {
      x = out_x + (out_w - scaled_w) / 2;
      y = out_y + (out_h - scaled_h) / 2;
      if (x < out_x)
        x = out_x;
      if (y < out_y)
        y = out_y;
    }
  } else if (x <= 0 && y <= 0 && bridge->plexy_conn) {
    if (out_w < INT32_MAX && out_h < INT32_MAX) {
      x = out_x + (out_w - scaled_w) / 2;
      y = out_y + (out_h - scaled_h) / 2;
      if (x < out_x)
        x = out_x;
      if (y < out_y)
        y = out_y;
    }
  } else {
    if (x < 0)
      x = 100;
    if (y < 0)
      y = 100;
  }

  if (out_w < INT32_MAX && x + scaled_w > out_x + out_w)
    x = out_x + out_w - scaled_w;
  if (out_h < INT32_MAX && y + scaled_h > out_y + out_h)
    y = out_y + out_h - scaled_h;
  if (out_w < INT32_MAX && x < out_x)
    x = out_x;
  if (out_h < INT32_MAX && y < out_y)
    y = out_y;

  LOG_INFO("Creating X11 window: '%s' at (%d,%d) %dx%d", window_title, x, y,
           scaled_w, scaled_h);

  PlexyWindow *window =
      plexy_create_window_typed(bridge->plexy_conn, x, y, scaled_w, scaled_h,
                                window_title, window_type, transient_parent);
  if (!window) {
    LOG_ERROR("Failed to create PlexyWindow for X11 surface");
    return false;
  }

  bridge_surface_set_plexy_window(surface, window);
  surface->activated = true;
  surface->configured = true;

  if (xwin && xwin->class && xwin->class[0]) {
    plexy_window_set_app_id(window, xwin->class);
  }

  bool want_decorations = true;
  if (window_type == PLEXY_WINDOW_TYPE_SPLASH ||
      window_type == PLEXY_WINDOW_TYPE_DOCK ||
      window_type == PLEXY_WINDOW_TYPE_DESKTOP ||
      window_type == PLEXY_WINDOW_TYPE_TOOLTIP ||
      window_type == PLEXY_WINDOW_TYPE_NOTIFICATION ||
      window_type == PLEXY_WINDOW_TYPE_DND ||
      window_type == PLEXY_WINDOW_TYPE_POPUP_MENU ||
      window_type == PLEXY_WINDOW_TYPE_DROPDOWN_MENU ||
      window_type == PLEXY_WINDOW_TYPE_MENU ||
      window_type == PLEXY_WINDOW_TYPE_COMBO) {
    want_decorations = false;
  } else if (window_type != PLEXY_WINDOW_TYPE_NORMAL &&
             window_type != PLEXY_WINDOW_TYPE_DIALOG && surface->xwm_window) {

    struct xwm_window *xwin = (struct xwm_window *)surface->xwm_window;
    if (!xwin->decorate)
      want_decorations = false;
  }
  if (!want_decorations) {
    plexy_window_set_decorations(window, false);
  }

  int32_t title_bar_offset = 0;
  if (want_decorations && bridge->plexy_conn) {
    float ui_scale = plexy_get_ui_scale(bridge->plexy_conn);
    title_bar_offset = (int32_t)(32.0f * ui_scale);
  }

  surface->screen_x = x;
  surface->screen_y = y + title_bar_offset;
  surface->screen_width = scaled_w;
  surface->screen_height = scaled_h;
  surface->screen_pos_valid = true;

  if (surface->xwm_window) {
    extern void xwm_window_configure(struct xwm_window * window, int32_t x,
                                     int32_t y, int32_t width, int32_t height);
    xwm_window_configure((struct xwm_window *)surface->xwm_window, x,
                         y + title_bar_offset, scaled_w, scaled_h);
  }

  LOG_INFO("Created X11 window: id=%u for surface", surface->plexy_window_id);

  if (surface->xwm_window) {
    struct xwm_window *xwin = (struct xwm_window *)surface->xwm_window;
    struct xwm_size_hints *sh = &xwin->size_hints;
    int32_t min_w = 0, min_h = 0, max_w = 0, max_h = 0;
    if (sh->flags & XWM_P_MIN_SIZE) {
      min_w = sh->min_width;
      min_h = sh->min_height;
    }
    if (sh->flags & XWM_P_MAX_SIZE) {
      max_w = sh->max_width;
      max_h = sh->max_height;
    }
    if (min_w > 0 || min_h > 0 || max_w > 0 || max_h > 0) {
      plexy_window_set_geometry_hints(window, min_w, min_h, max_w, max_h);
    }
  }

  if (surface->xdg_toplevel && surface->xdg_surface) {
    struct wl_array states;
    wl_array_init(&states);
    xdg_toplevel_send_configure(surface->xdg_toplevel, scaled_w, scaled_h,
                                &states);
    surface->configure_serial++;
    xdg_surface_send_configure(surface->xdg_surface, surface->configure_serial);
    wl_array_release(&states);
    LOG_DEBUG("Sent initial xdg configure to X11 surface: %dx%d", scaled_w,
              scaled_h);
  }

  PlexyWindowCallbacks callbacks = {
      .configure = forward_configure,
      .close = forward_close,
      .pointer_enter = forward_pointer_enter,
      .pointer_leave = forward_pointer_leave,
      .pointer_motion = forward_pointer_motion,
      .pointer_button = forward_pointer_button,
      .pointer_axis = forward_pointer_axis,
      .touch_down = forward_touch_down,
      .touch_motion = forward_touch_motion,
      .touch_up = forward_touch_up,
      .touch_cancel = forward_touch_cancel,
      .touch_frame = forward_touch_frame,
      .key = forward_key,
      .modifiers = forward_modifiers,
      .focus_in = forward_focus_in,
      .focus_out = forward_focus_out,
      .frame_done = forward_frame_done,
      .scale_changed = NULL,
      .enter_output = forward_enter_output,
      .leave_output = forward_leave_output,
  };

  plexy_window_set_callbacks(window, &callbacks, surface);
  bridge_sync_input_region_to_plexy(surface);

  return true;
}

bool bridge_create_x11_popup(struct bridge_surface *surface, const char *title,
                             PlexyWindow *parent_window, int32_t x, int32_t y,
                             int32_t width, int32_t height,
                             uint32_t window_type) {
  if (!surface || !bridge) {
    LOG_ERROR("bridge_create_x11_popup: invalid surface or bridge");
    return false;
  }

  if (surface->plexy_window) {
    LOG_WARN("bridge_create_x11_popup: surface already has a window");
    return true;
  }

  surface->is_x11 = true;

  if (width <= 0)
    width = 200;
  if (height <= 0)
    height = 200;

  LOG_INFO("Creating X11 popup: '%s' at (%d,%d) %dx%d parent=%p",
           title ? title : "X11 Popup", x, y, width, height,
           (void *)parent_window);

  PlexyWindow *window =
      plexy_create_popup(bridge->plexy_conn, parent_window, x, y, width, height,
                         PLEXY_POPUP_FLAG_NONE);
  if (!window) {
    LOG_ERROR("Failed to create popup PlexyWindow for X11 surface");
    return false;
  }

  bridge_surface_set_plexy_window(surface, window);
  surface->activated = true;
  surface->configured = true;
  surface->screen_x = x;
  surface->screen_y = y;
  surface->screen_width = width;
  surface->screen_height = height;
  surface->screen_pos_valid = true;

  LOG_INFO("Created X11 popup: id=%u parent=%u at (%d,%d) %dx%d",
           surface->plexy_window_id,
           parent_window ? plexy_window_get_id(parent_window) : 0, x, y, width,
           height);

  if (window_type != PLEXY_WINDOW_TYPE_NORMAL) {
    plexy_window_set_type(window, window_type);
  }

  PlexyWindowCallbacks callbacks = {
      .configure = forward_configure,
      .close = forward_close,
      .pointer_enter = forward_pointer_enter,
      .pointer_leave = forward_pointer_leave,
      .pointer_motion = forward_pointer_motion,
      .pointer_button = forward_pointer_button,
      .pointer_axis = forward_pointer_axis,
      .touch_down = forward_touch_down,
      .touch_motion = forward_touch_motion,
      .touch_up = forward_touch_up,
      .touch_cancel = forward_touch_cancel,
      .touch_frame = forward_touch_frame,
      .key = forward_key,
      .modifiers = forward_modifiers,
      .focus_in = forward_focus_in,
      .focus_out = forward_focus_out,
      .frame_done = forward_frame_done,
      .scale_changed = NULL,
      .enter_output = forward_enter_output,
      .leave_output = forward_leave_output,
  };

  plexy_window_set_callbacks(window, &callbacks, surface);
  bridge_sync_input_region_to_plexy(surface);

  return true;
}

bool bridge_update_x11_popup(struct bridge_surface *surface, int32_t x,
                             int32_t y, int32_t width, int32_t height,
                             uint32_t window_type) {
  if (!surface || !bridge || !surface->plexy_window) {
    LOG_ERROR("bridge_update_x11_popup: invalid surface or popup window");
    return false;
  }

  if (width <= 0)
    width = surface->screen_width > 0 ? surface->screen_width : 200;
  if (height <= 0)
    height = surface->screen_height > 0 ? surface->screen_height : 200;

  if (plexy_popup_update_geometry(surface->plexy_window, x, y, width, height,
                                  PLEXY_POPUP_FLAG_NONE) < 0) {
    LOG_ERROR("bridge_update_x11_popup: failed to update popup geometry");
    return false;
  }

  surface->screen_x = x;
  surface->screen_y = y;
  surface->screen_width = width;
  surface->screen_height = height;
  surface->screen_pos_valid = true;

  if (window_type != PLEXY_WINDOW_TYPE_NORMAL) {
    plexy_window_set_type(surface->plexy_window, window_type);
  }

  bridge_sync_input_region_to_plexy(surface);
  LOG_INFO("Updated X11 popup: id=%u at (%d,%d) %dx%d",
           surface->plexy_window_id, x, y, width, height);
  return true;
}
