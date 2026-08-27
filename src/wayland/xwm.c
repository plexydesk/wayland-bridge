/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#define _POSIX_C_SOURCE 200809L
#include "xwm.h"
#include "wayland_bridge.h"
#include "xwayland-shell-v1-protocol.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static inline uint64_t u64_from_u32s(uint32_t hi, uint32_t lo) {
  return ((uint64_t)hi << 32) | lo;
}

#define XWM_SEND_EVENT_MASK 0x80
#define XWM_EVENT_TYPE(event) ((event)->response_type & ~XWM_SEND_EVENT_MASK)

#define XWM_HASH_SIZE 256

struct xwm_hash_entry {
  uint32_t key;
  void *value;
  struct xwm_hash_entry *next;
};

struct xwm_hash_table {
  struct xwm_hash_entry *buckets[XWM_HASH_SIZE];
};

struct xwm_hash_table *xwm_hash_table_create(void) {
  struct xwm_hash_table *table = calloc(1, sizeof(*table));
  return table;
}

void xwm_hash_table_destroy(struct xwm_hash_table *table) {
  if (!table)
    return;

  for (int i = 0; i < XWM_HASH_SIZE; i++) {
    struct xwm_hash_entry *entry = table->buckets[i];
    while (entry) {
      struct xwm_hash_entry *next = entry->next;
      free(entry);
      entry = next;
    }
  }
  free(table);
}

static uint32_t hash_key(uint32_t key) {

  return (key * 2654435761u) >> (32 - 8);
}

void xwm_hash_table_insert(struct xwm_hash_table *table, uint32_t key,
                           void *value) {
  uint32_t bucket = hash_key(key);

  struct xwm_hash_entry *entry = table->buckets[bucket];
  while (entry) {
    if (entry->key == key) {
      entry->value = value;
      return;
    }
    entry = entry->next;
  }

  entry = malloc(sizeof(*entry));
  if (!entry)
    return;

  entry->key = key;
  entry->value = value;
  entry->next = table->buckets[bucket];
  table->buckets[bucket] = entry;
}

void *xwm_hash_table_lookup(struct xwm_hash_table *table, uint32_t key) {
  uint32_t bucket = hash_key(key);
  struct xwm_hash_entry *entry = table->buckets[bucket];

  while (entry) {
    if (entry->key == key)
      return entry->value;
    entry = entry->next;
  }
  return NULL;
}

void xwm_hash_table_remove(struct xwm_hash_table *table, uint32_t key) {
  uint32_t bucket = hash_key(key);
  struct xwm_hash_entry **pp = &table->buckets[bucket];

  while (*pp) {
    if ((*pp)->key == key) {
      struct xwm_hash_entry *entry = *pp;
      *pp = entry->next;
      free(entry);
      return;
    }
    pp = &(*pp)->next;
  }
}

char *xwm_get_atom_name(struct xwm *xwm, xcb_atom_t atom) {
  xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name(xwm->conn, atom);
  xcb_get_atom_name_reply_t *reply =
      xcb_get_atom_name_reply(xwm->conn, cookie, NULL);

  if (!reply) {
    char fallback[32];
    snprintf(fallback, sizeof(fallback), "atom_%u", atom);
    return strdup(fallback);
  }

  int len = xcb_get_atom_name_name_length(reply);
  char *name = strndup(xcb_get_atom_name_name(reply), len);
  free(reply);
  return name;
}

static const struct {
  const char *name;
  size_t offset;
} atom_info[] = {

    {"WM_PROTOCOLS", offsetof(struct xwm_atoms, wm_protocols)},
    {"WM_NORMAL_HINTS", offsetof(struct xwm_atoms, wm_normal_hints)},
    {"WM_TAKE_FOCUS", offsetof(struct xwm_atoms, wm_take_focus)},
    {"WM_DELETE_WINDOW", offsetof(struct xwm_atoms, wm_delete_window)},
    {"WM_STATE", offsetof(struct xwm_atoms, wm_state)},
    {"WM_S0", offsetof(struct xwm_atoms, wm_s0)},
    {"WM_CLIENT_MACHINE", offsetof(struct xwm_atoms, wm_client_machine)},
    {"WM_CHANGE_STATE", offsetof(struct xwm_atoms, wm_change_state)},

    {"_NET_FRAME_EXTENTS", offsetof(struct xwm_atoms, net_frame_extents)},
    {"_NET_WM_CM_S0", offsetof(struct xwm_atoms, net_wm_cm_s0)},
    {"_NET_WM_NAME", offsetof(struct xwm_atoms, net_wm_name)},
    {"_NET_WM_PID", offsetof(struct xwm_atoms, net_wm_pid)},
    {"_NET_WM_ICON", offsetof(struct xwm_atoms, net_wm_icon)},
    {"_NET_WM_STATE", offsetof(struct xwm_atoms, net_wm_state)},
    {"_NET_WM_STATE_MAXIMIZED_VERT",
     offsetof(struct xwm_atoms, net_wm_state_maximized_vert)},
    {"_NET_WM_STATE_MAXIMIZED_HORZ",
     offsetof(struct xwm_atoms, net_wm_state_maximized_horz)},
    {"_NET_WM_STATE_FULLSCREEN",
     offsetof(struct xwm_atoms, net_wm_state_fullscreen)},
    {"_NET_WM_USER_TIME", offsetof(struct xwm_atoms, net_wm_user_time)},
    {"_NET_WM_ICON_NAME", offsetof(struct xwm_atoms, net_wm_icon_name)},
    {"_NET_WM_DESKTOP", offsetof(struct xwm_atoms, net_wm_desktop)},
    {"_NET_WM_WINDOW_TYPE", offsetof(struct xwm_atoms, net_wm_window_type)},
    {"_NET_WM_WINDOW_TYPE_DESKTOP",
     offsetof(struct xwm_atoms, net_wm_window_type_desktop)},
    {"_NET_WM_WINDOW_TYPE_DOCK",
     offsetof(struct xwm_atoms, net_wm_window_type_dock)},
    {"_NET_WM_WINDOW_TYPE_TOOLBAR",
     offsetof(struct xwm_atoms, net_wm_window_type_toolbar)},
    {"_NET_WM_WINDOW_TYPE_MENU",
     offsetof(struct xwm_atoms, net_wm_window_type_menu)},
    {"_NET_WM_WINDOW_TYPE_UTILITY",
     offsetof(struct xwm_atoms, net_wm_window_type_utility)},
    {"_NET_WM_WINDOW_TYPE_SPLASH",
     offsetof(struct xwm_atoms, net_wm_window_type_splash)},
    {"_NET_WM_WINDOW_TYPE_DIALOG",
     offsetof(struct xwm_atoms, net_wm_window_type_dialog)},
    {"_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
     offsetof(struct xwm_atoms, net_wm_window_type_dropdown)},
    {"_NET_WM_WINDOW_TYPE_POPUP_MENU",
     offsetof(struct xwm_atoms, net_wm_window_type_popup)},
    {"_NET_WM_WINDOW_TYPE_TOOLTIP",
     offsetof(struct xwm_atoms, net_wm_window_type_tooltip)},
    {"_NET_WM_WINDOW_TYPE_NOTIFICATION",
     offsetof(struct xwm_atoms, net_wm_window_type_notification)},
    {"_NET_WM_WINDOW_TYPE_COMBO",
     offsetof(struct xwm_atoms, net_wm_window_type_combo)},
    {"_NET_WM_WINDOW_TYPE_DND",
     offsetof(struct xwm_atoms, net_wm_window_type_dnd)},
    {"_NET_WM_WINDOW_TYPE_NORMAL",
     offsetof(struct xwm_atoms, net_wm_window_type_normal)},
    {"_NET_WM_MOVERESIZE", offsetof(struct xwm_atoms, net_wm_moveresize)},
    {"_NET_SUPPORTING_WM_CHECK",
     offsetof(struct xwm_atoms, net_supporting_wm_check)},
    {"_NET_SUPPORTED", offsetof(struct xwm_atoms, net_supported)},
    {"_NET_ACTIVE_WINDOW", offsetof(struct xwm_atoms, net_active_window)},
    {"_NET_CLIENT_LIST", offsetof(struct xwm_atoms, net_client_list)},
    {"_NET_WM_PING", offsetof(struct xwm_atoms, net_wm_ping)},

    {"WM_HINTS", offsetof(struct xwm_atoms, wm_hints)},
    {"_MOTIF_WM_HINTS", offsetof(struct xwm_atoms, motif_wm_hints)},

    {"CLIPBOARD", offsetof(struct xwm_atoms, clipboard)},
    {"CLIPBOARD_MANAGER", offsetof(struct xwm_atoms, clipboard_manager)},
    {"TARGETS", offsetof(struct xwm_atoms, targets)},
    {"UTF8_STRING", offsetof(struct xwm_atoms, utf8_string)},
    {"_WL_SELECTION", offsetof(struct xwm_atoms, wl_selection)},
    {"INCR", offsetof(struct xwm_atoms, incr)},
    {"TIMESTAMP", offsetof(struct xwm_atoms, timestamp)},
    {"MULTIPLE", offsetof(struct xwm_atoms, multiple)},
    {"COMPOUND_TEXT", offsetof(struct xwm_atoms, compound_text)},
    {"TEXT", offsetof(struct xwm_atoms, text)},
    {"STRING", offsetof(struct xwm_atoms, string)},
    {"WINDOW", offsetof(struct xwm_atoms, window)},
    {"text/plain;charset=utf-8", offsetof(struct xwm_atoms, text_plain_utf8)},
    {"text/plain", offsetof(struct xwm_atoms, text_plain)},
    {"PRIMARY", offsetof(struct xwm_atoms, primary)},
    {"_WL_PRIMARY_SELECTION",
     offsetof(struct xwm_atoms, _wl_primary_selection)},
    {"_NET_WM_STATE_ABOVE", offsetof(struct xwm_atoms, net_wm_state_above)},
    {"_NET_WM_STATE_BELOW", offsetof(struct xwm_atoms, net_wm_state_below)},
    {"_NET_WM_STATE_MODAL", offsetof(struct xwm_atoms, net_wm_state_modal)},
    {"_NET_WM_STATE_HIDDEN", offsetof(struct xwm_atoms, net_wm_state_hidden)},

    {"WL_SURFACE_ID", offsetof(struct xwm_atoms, wl_surface_id)},
    {"WL_SURFACE_SERIAL", offsetof(struct xwm_atoms, wl_surface_serial)},
    {"_XWAYLAND_ALLOW_COMMITS", offsetof(struct xwm_atoms, allow_commits)},
    {"_XQUADRO_WINDOW_ID", offsetof(struct xwm_atoms, xquadro_window_id)},
};

#define ATOM_COUNT (sizeof(atom_info) / sizeof(atom_info[0]))

void xwm_init_atoms(struct xwm *xwm) {
  xcb_connection_t *c = xwm->conn;
  xcb_intern_atom_cookie_t cookies[ATOM_COUNT];

  for (size_t i = 0; i < ATOM_COUNT; i++) {
    cookies[i] =
        xcb_intern_atom(c, 0, strlen(atom_info[i].name), atom_info[i].name);
  }

  xcb_flush(c);

  for (size_t i = 0; i < ATOM_COUNT; i++) {
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(c, cookies[i], NULL);
    xcb_atom_t *atom_ptr =
        (xcb_atom_t *)((char *)&xwm->atoms + atom_info[i].offset);

    if (reply) {
      *atom_ptr = reply->atom;
      free(reply);
    } else {
      *atom_ptr = XCB_ATOM_NONE;
      LOG_WARN("XWM: failed to intern atom '%s'", atom_info[i].name);
    }
  }

  LOG_INFO("XWM atoms initialized");
}

static bool our_resource(struct xwm *xwm, uint32_t id) {
  const xcb_setup_t *setup = xcb_get_setup(xwm->conn);
  return (id & ~setup->resource_id_mask) == setup->resource_id_base;
}

static bool xwm_window_is_popup(struct xwm_window *window) {
  if (!window)
    return false;
  struct xwm *xwm = window->xwm;

  if (window->override_redirect)
    return true;

  if (window->type == xwm->atoms.net_wm_window_type_popup ||
      window->type == xwm->atoms.net_wm_window_type_dropdown ||
      window->type == xwm->atoms.net_wm_window_type_menu ||
      window->type == xwm->atoms.net_wm_window_type_tooltip ||
      window->type == xwm->atoms.net_wm_window_type_combo ||
      window->type == xwm->atoms.net_wm_window_type_notification) {
    return true;
  }

  return false;
}

static PlexyWindow *
xwm_window_resolve_and_cache_parent(struct xwm_window *window) {
  if (!window)
    return NULL;

  if (window->popup_parent_plexy)
    return window->popup_parent_plexy;

  PlexyWindow *parent = NULL;

  if (window->transient_for && window->transient_for->surface &&
      window->transient_for->surface->plexy_window) {
    parent = window->transient_for->surface->plexy_window;
    window->popup_parent_window = window->transient_for;
  }

  if (!parent && window->pid != 0) {
    struct xwm *xwm = window->xwm;
    if (xwm && xwm->focus_window && xwm->focus_window->pid == window->pid &&
        xwm->focus_window->surface &&
        xwm->focus_window->surface->plexy_window) {
      parent = xwm->focus_window->surface->plexy_window;
      window->popup_parent_window = xwm->focus_window;
      LOG_INFO(
          "XWM: window %u using same-PID focus fallback (pid=%u) for parent",
          window->id, window->pid);
    }
  }

  if (!parent)
    LOG_INFO("XWM: window %u has no WM_TRANSIENT_FOR and no same-PID focus, "
             "creating as unparented popup",
             window->id);

  window->popup_parent_plexy = parent;
  return parent;
}

static void xwm_popup_rel_coords(struct xwm_window *window, int32_t *out_x,
                                 int32_t *out_y) {
  *out_x = window->x;
  *out_y = window->y;
  if (window->popup_parent_window) {
    struct bridge_surface *parent_bs = window->popup_parent_window->surface;
    if (parent_bs && parent_bs->screen_pos_valid) {
      *out_x -= parent_bs->screen_x;
      *out_y -= parent_bs->screen_y;
    } else {
      *out_x -= window->popup_parent_window->x;
      *out_y -= window->popup_parent_window->y;
    }
  }
}

static uint32_t xwm_window_get_plexy_type(struct xwm_window *window) {
  if (!window)
    return PLEXY_WINDOW_TYPE_NORMAL;
  struct xwm *xwm = window->xwm;

  if (window->type == xwm->atoms.net_wm_window_type_popup)
    return PLEXY_WINDOW_TYPE_POPUP_MENU;
  if (window->type == xwm->atoms.net_wm_window_type_dropdown)
    return PLEXY_WINDOW_TYPE_DROPDOWN_MENU;
  if (window->type == xwm->atoms.net_wm_window_type_menu)
    return PLEXY_WINDOW_TYPE_MENU;
  if (window->type == xwm->atoms.net_wm_window_type_tooltip)
    return PLEXY_WINDOW_TYPE_TOOLTIP;
  if (window->type == xwm->atoms.net_wm_window_type_combo)
    return PLEXY_WINDOW_TYPE_COMBO;
  if (window->type == xwm->atoms.net_wm_window_type_notification)
    return PLEXY_WINDOW_TYPE_NOTIFICATION;
  if (window->type == xwm->atoms.net_wm_window_type_splash)
    return PLEXY_WINDOW_TYPE_SPLASH;
  if (window->type == xwm->atoms.net_wm_window_type_dialog)
    return PLEXY_WINDOW_TYPE_DIALOG;
  if (window->type == xwm->atoms.net_wm_window_type_toolbar)
    return PLEXY_WINDOW_TYPE_TOOLBAR;
  if (window->type == xwm->atoms.net_wm_window_type_utility)
    return PLEXY_WINDOW_TYPE_UTILITY;
  if (window->type == xwm->atoms.net_wm_window_type_dock)
    return PLEXY_WINDOW_TYPE_DOCK;
  if (window->type == xwm->atoms.net_wm_window_type_desktop)
    return PLEXY_WINDOW_TYPE_DESKTOP;
  if (window->type == xwm->atoms.net_wm_window_type_dnd)
    return PLEXY_WINDOW_TYPE_DND;

  if (window->transient_for && !window->override_redirect)
    return PLEXY_WINDOW_TYPE_DIALOG;

  if (window->override_redirect)
    return PLEXY_WINDOW_TYPE_POPUP_MENU;

  return PLEXY_WINDOW_TYPE_NORMAL;
}

static bool xwm_translate_to_root(struct xwm *xwm, xcb_window_t win_id,
                                  int32_t *out_x, int32_t *out_y) {
  xcb_translate_coordinates_cookie_t tc =
      xcb_translate_coordinates(xwm->conn, win_id, xwm->screen->root, 0, 0);
  xcb_translate_coordinates_reply_t *tr =
      xcb_translate_coordinates_reply(xwm->conn, tc, NULL);
  if (!tr)
    return false;
  *out_x = tr->dst_x;
  *out_y = tr->dst_y;
  free(tr);
  return true;
}

static void xwm_window_ensure_real_geometry(struct xwm_window *window) {
  if (!window->override_redirect)
    return;

  xcb_get_geometry_cookie_t gc =
      xcb_get_geometry(window->xwm->conn, window->id);
  xcb_get_geometry_reply_t *gr =
      xcb_get_geometry_reply(window->xwm->conn, gc, NULL);
  if (gr) {

    int32_t root_x = gr->x, root_y = gr->y;
    xwm_translate_to_root(window->xwm, window->id, &root_x, &root_y);

    LOG_INFO("XWM: window %u queried geometry: %dx%d at (%d,%d) [cached: %dx%d "
             "at (%d,%d)]",
             window->id, gr->width, gr->height, root_x, root_y, window->width,
             window->height, window->x, window->y);

    window->x = root_x;
    window->y = root_y;
    if (gr->width >= 4 && gr->height >= 4) {
      window->width = gr->width;
      window->height = gr->height;
    }
    free(gr);
  }

  if ((window->width < 4 || window->height < 4) && window->create_width >= 4 &&
      window->create_height >= 4) {
    LOG_INFO("XWM: window %u using CreateNotify dims %dx%d (current %dx%d)",
             window->id, window->create_width, window->create_height,
             window->width, window->height);
    window->width = window->create_width;
    window->height = window->create_height;
  }
}

static struct xwm_window *xwm_window_create(struct xwm *xwm, xcb_window_t id,
                                            int width, int height, int x, int y,
                                            bool override_redirect) {
  struct xwm_window *window = calloc(1, sizeof(*window));
  if (!window)
    return NULL;

  window->xwm = xwm;
  window->id = id;
  window->width = width;
  window->height = height;
  window->x = x;
  window->y = y;
  window->override_redirect = override_redirect;
  window->properties_dirty = true;
  window->saved_width = 512;
  window->saved_height = 512;
  window->create_width = width;
  window->create_height = height;
  window->decorate = true;
  window->wants_input = true;

  xcb_get_geometry_cookie_t geom_cookie = xcb_get_geometry(xwm->conn, id);
  xcb_get_geometry_reply_t *geom =
      xcb_get_geometry_reply(xwm->conn, geom_cookie, NULL);
  if (geom) {
    window->has_alpha = (geom->depth == 32);
    free(geom);
  }

  uint32_t event_mask =
      XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE;
  if (override_redirect)
    event_mask |= XCB_EVENT_MASK_STRUCTURE_NOTIFY;
  uint32_t values[1] = {event_mask};
  xcb_change_window_attributes(xwm->conn, id, XCB_CW_EVENT_MASK, values);

  xwm_hash_table_insert(xwm->window_hash, id, window);

  wl_list_insert(&xwm->window_list, &window->link);

  LOG_DEBUG("XWM: created window %u (%dx%d @ %d,%d%s)", id, width, height, x, y,
            override_redirect ? ", override-redirect" : "");

  return window;
}

static void xwm_window_destroy(struct xwm_window *window) {
  if (!window)
    return;

  struct xwm *xwm = window->xwm;

  LOG_DEBUG("XWM: destroying window %u", window->id);

  xwm_hash_table_remove(xwm->window_hash, window->id);

  wl_list_remove(&window->link);

  if (xwm->focus_window == window)
    xwm_set_focus(xwm, NULL);

  if (xwm->pending_focus_window == window)
    xwm->pending_focus_window = NULL;

  if (window->surface && window->surface->xwm_window == window)
    window->surface->xwm_window = NULL;

  free(window->name);
  free(window->class);
  free(window->machine);
  free(window);
}

static bool xwm_bind_surface_window(struct xwm_window *window,
                                    struct bridge_surface *surface,
                                    const char *path) {
  if (!window || !surface)
    return false;

  struct xwm_window *owner = (struct xwm_window *)surface->xwm_window;
  if (owner && owner != window) {
    if (owner->mapped) {

      LOG_ERROR(
          "XWM: %s: illegal rebind of surface=%p from mapped window=%u to "
          "window=%u — refusing",
          path ? path : "bind", (void *)surface, owner->id, window->id);
      return false;
    }

    LOG_WARN(
        "XWM: %s: rebinding surface=%p from unmapped window=%u to window=%u",
        path ? path : "bind", (void *)surface, owner->id, window->id);
  }

  window->surface = surface;
  surface->xwm_window = window;
  return true;
}

struct xwm_window *xwm_get_window(struct xwm *xwm, xcb_window_t id) {
  return xwm_hash_table_lookup(xwm->window_hash, id);
}

void xwm_window_read_properties(struct xwm_window *window) {
  struct xwm *xwm = window->xwm;

  xcb_get_property_cookie_t c_net_name =
      xcb_get_property(xwm->conn, 0, window->id, xwm->atoms.net_wm_name,
                       xwm->atoms.utf8_string, 0, 2048);
  xcb_get_property_cookie_t c_wm_name = xcb_get_property(
      xwm->conn, 0, window->id, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 2048);
  xcb_get_property_cookie_t c_class = xcb_get_property(
      xwm->conn, 0, window->id, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 2048);
  xcb_get_property_cookie_t c_pid = xcb_get_property(
      xwm->conn, 0, window->id, xwm->atoms.net_wm_pid, XCB_ATOM_CARDINAL, 0, 1);
  xcb_get_property_cookie_t c_protocols = xcb_get_property(
      xwm->conn, 0, window->id, xwm->atoms.wm_protocols, XCB_ATOM_ATOM, 0, 100);
  xcb_get_property_cookie_t c_hints = xcb_get_property(
      xwm->conn, 0, window->id, xwm->atoms.wm_hints, XCB_ATOM_ANY, 0, 9);
  xcb_get_property_cookie_t c_state = xcb_get_property(
      xwm->conn, 0, window->id, xwm->atoms.net_wm_state, XCB_ATOM_ATOM, 0, 100);
  xcb_get_property_cookie_t c_motif = xcb_get_property(
      xwm->conn, 0, window->id, xwm->atoms.motif_wm_hints, XCB_ATOM_ANY, 0, 5);
  xcb_get_property_cookie_t c_type =
      xcb_get_property(xwm->conn, 0, window->id, xwm->atoms.net_wm_window_type,
                       XCB_ATOM_ATOM, 0, 1);
  xcb_get_property_cookie_t c_transient =
      xcb_get_property(xwm->conn, 0, window->id, XCB_ATOM_WM_TRANSIENT_FOR,
                       XCB_ATOM_WINDOW, 0, 1);
  xcb_get_property_cookie_t c_normal =
      xcb_get_property(xwm->conn, 0, window->id, xwm->atoms.wm_normal_hints,
                       XCB_ATOM_ANY, 0, 18);

  xcb_get_property_reply_t *r_net_name =
      xcb_get_property_reply(xwm->conn, c_net_name, NULL);
  xcb_get_property_reply_t *r_wm_name =
      xcb_get_property_reply(xwm->conn, c_wm_name, NULL);
  xcb_get_property_reply_t *r_class =
      xcb_get_property_reply(xwm->conn, c_class, NULL);
  xcb_get_property_reply_t *r_pid =
      xcb_get_property_reply(xwm->conn, c_pid, NULL);
  xcb_get_property_reply_t *r_proto =
      xcb_get_property_reply(xwm->conn, c_protocols, NULL);
  xcb_get_property_reply_t *r_hints =
      xcb_get_property_reply(xwm->conn, c_hints, NULL);
  xcb_get_property_reply_t *r_state =
      xcb_get_property_reply(xwm->conn, c_state, NULL);
  xcb_get_property_reply_t *r_motif =
      xcb_get_property_reply(xwm->conn, c_motif, NULL);
  xcb_get_property_reply_t *r_type =
      xcb_get_property_reply(xwm->conn, c_type, NULL);
  xcb_get_property_reply_t *r_transient =
      xcb_get_property_reply(xwm->conn, c_transient, NULL);
  xcb_get_property_reply_t *r_normal =
      xcb_get_property_reply(xwm->conn, c_normal, NULL);

  xcb_get_property_reply_t *r_name =
      (r_net_name && xcb_get_property_value_length(r_net_name) > 0) ? r_net_name
                                                                    : r_wm_name;
  if (r_name && xcb_get_property_value_length(r_name) > 0) {
    int len = xcb_get_property_value_length(r_name);
    free(window->name);
    window->name = malloc(len + 1);
    if (window->name) {
      memcpy(window->name, xcb_get_property_value(r_name), len);
      window->name[len] = '\0';
    }
  }
  free(r_net_name);
  free(r_wm_name);

  if (r_class && xcb_get_property_value_length(r_class) > 0) {
    char *str = xcb_get_property_value(r_class);
    int len = xcb_get_property_value_length(r_class);
    int first = strnlen(str, len);
    if (first < len - 1) {
      char *second = str + first + 1;
      int slen = strnlen(second, len - first - 1);
      free(window->class);
      window->class = malloc(slen + 1);
      if (window->class) {
        memcpy(window->class, second, slen);
        window->class[slen] = '\0';
      }
    }
  }
  free(r_class);

  if (r_pid && xcb_get_property_value_length(r_pid) >= 4)
    window->pid = *(uint32_t *)xcb_get_property_value(r_pid);
  free(r_pid);

  window->delete_window = false;
  window->take_focus = false;
  if (r_proto) {
    xcb_atom_t *atoms = xcb_get_property_value(r_proto);
    int n_atoms = xcb_get_property_value_length(r_proto) / sizeof(xcb_atom_t);
    for (int i = 0; i < n_atoms; i++) {
      if (atoms[i] == xwm->atoms.wm_delete_window)
        window->delete_window = true;
      else if (atoms[i] == xwm->atoms.wm_take_focus)
        window->take_focus = true;
    }
  }
  free(r_proto);

  window->wants_input = true;
  if (r_hints &&
      xcb_get_property_value_length(r_hints) >= (int)(4 * sizeof(uint32_t))) {
    uint32_t *h = xcb_get_property_value(r_hints);
    uint32_t flags = h[0];

    if (flags & (1u << 0))
      window->wants_input = (bool)h[1];
  }
  free(r_hints);

  window->fullscreen = false;
  window->maximized_vert = false;
  window->maximized_horz = false;
  window->above = false;
  window->below = false;
  window->modal = false;
  window->hidden = false;
  if (r_state) {
    xcb_atom_t *atoms = xcb_get_property_value(r_state);
    int n_atoms = xcb_get_property_value_length(r_state) / sizeof(xcb_atom_t);
    for (int i = 0; i < n_atoms; i++) {
      if (atoms[i] == xwm->atoms.net_wm_state_fullscreen)
        window->fullscreen = true;
      else if (atoms[i] == xwm->atoms.net_wm_state_maximized_vert)
        window->maximized_vert = true;
      else if (atoms[i] == xwm->atoms.net_wm_state_maximized_horz)
        window->maximized_horz = true;
      else if (atoms[i] == xwm->atoms.net_wm_state_above)
        window->above = true;
      else if (atoms[i] == xwm->atoms.net_wm_state_below)
        window->below = true;
      else if (atoms[i] == xwm->atoms.net_wm_state_modal)
        window->modal = true;
      else if (atoms[i] == xwm->atoms.net_wm_state_hidden)
        window->hidden = true;
    }
  }
  free(r_state);

  if (r_motif &&
      xcb_get_property_value_length(r_motif) >= (int)(5 * sizeof(uint32_t))) {
    uint32_t *m = xcb_get_property_value(r_motif);
    memcpy(&window->motif_hints, m, sizeof(struct xwm_motif_hints));

    if (window->motif_hints.flags & XWM_MWM_HINTS_DECORATIONS) {
      uint32_t d = window->motif_hints.decorations;
      if (d & XWM_MWM_DECOR_ALL) {

        window->decorate = true;
      } else {

        window->decorate =
            (d & (XWM_MWM_DECOR_TITLE | XWM_MWM_DECOR_BORDER)) != 0;
      }
    }
  }
  free(r_motif);

  if (r_type &&
      xcb_get_property_value_length(r_type) >= (int)sizeof(xcb_atom_t))
    window->type = *(xcb_atom_t *)xcb_get_property_value(r_type);
  free(r_type);

  if (r_transient &&
      xcb_get_property_value_length(r_transient) >= (int)sizeof(xcb_window_t)) {
    xcb_window_t parent_id =
        *(xcb_window_t *)xcb_get_property_value(r_transient);
    window->transient_for = xwm_get_window(xwm, parent_id);
  }
  free(r_transient);

  memset(&window->size_hints, 0, sizeof(window->size_hints));
  if (r_normal && xcb_get_property_value_length(r_normal) >=
                      (int)sizeof(struct xwm_size_hints)) {
    memcpy(&window->size_hints, xcb_get_property_value(r_normal),
           sizeof(struct xwm_size_hints));
  }
  free(r_normal);

  window->properties_dirty = false;

  LOG_DEBUG("XWM: window %u props: name='%s' class='%s' pid=%d type=%u "
            "input=%d decorate=%d",
            window->id, window->name ? window->name : "(null)",
            window->class ? window->class : "(null)", window->pid, window->type,
            window->wants_input, window->decorate);
}

void xwm_window_set_wm_state(struct xwm_window *window, int32_t state) {
  struct xwm *xwm = window->xwm;
  uint32_t property[2] = {state, XCB_WINDOW_NONE};

  xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, window->id,
                      xwm->atoms.wm_state, xwm->atoms.wm_state, 32, 2,
                      property);
}

static void xwm_window_set_allow_commits(struct xwm_window *window,
                                         bool allow) {
  struct xwm *xwm = window->xwm;
  uint32_t val = allow ? 1 : 0;
  xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, window->id,
                      xwm->atoms.allow_commits, XCB_ATOM_CARDINAL, 32, 1, &val);
}

void xwm_window_set_net_wm_state(struct xwm_window *window) {
  struct xwm *xwm = window->xwm;
  xcb_atom_t states[7];
  int n = 0;

  if (window->fullscreen)
    states[n++] = xwm->atoms.net_wm_state_fullscreen;
  if (window->maximized_vert)
    states[n++] = xwm->atoms.net_wm_state_maximized_vert;
  if (window->maximized_horz)
    states[n++] = xwm->atoms.net_wm_state_maximized_horz;
  if (window->above)
    states[n++] = xwm->atoms.net_wm_state_above;
  if (window->below)
    states[n++] = xwm->atoms.net_wm_state_below;
  if (window->modal)
    states[n++] = xwm->atoms.net_wm_state_modal;
  if (window->hidden)
    states[n++] = xwm->atoms.net_wm_state_hidden;

  xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, window->id,
                      xwm->atoms.net_wm_state, XCB_ATOM_ATOM, 32, n, states);
}

static void xwm_window_send_configure_notify(struct xwm_window *window) {
  struct xwm *xwm = window->xwm;

  xcb_configure_notify_event_t event = {
      .response_type = XCB_CONFIGURE_NOTIFY,
      .event = window->id,
      .window = window->id,
      .above_sibling = XCB_WINDOW_NONE,
      .x = window->x,
      .y = window->y,
      .width = window->width,
      .height = window->height,
      .border_width = 0,
      .override_redirect = window->override_redirect,
  };

  xcb_send_event(xwm->conn, 0, window->id, XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                 (char *)&event);
}

static void xwm_handle_xcb_event(struct xwm *xwm, xcb_generic_event_t *event);

void xwm_window_close(struct xwm_window *window) {
  struct xwm *xwm = window->xwm;

  if (window->delete_window) {

    xcb_client_message_event_t event = {
        .response_type = XCB_CLIENT_MESSAGE,
        .format = 32,
        .window = window->id,
        .type = xwm->atoms.wm_protocols,
        .data.data32 = {xwm->atoms.wm_delete_window, XCB_TIME_CURRENT_TIME, 0,
                        0, 0}};

    xcb_send_event(xwm->conn, 0, window->id, XCB_EVENT_MASK_NO_EVENT,
                   (char *)&event);
  } else {

    xcb_kill_client(xwm->conn, window->id);
  }

  xcb_flush(xwm->conn);
}

void xwm_window_configure(struct xwm_window *window, int32_t x, int32_t y,
                          int32_t width, int32_t height) {
  struct xwm *xwm = window->xwm;

  if (window->x == x && window->y == y && window->width == width &&
      window->height == height) {
    if (window->surface) {
      window->surface->screen_x = x;
      window->surface->screen_y = y;
      window->surface->screen_width = width;
      window->surface->screen_height = height;
      window->surface->screen_pos_valid = true;
    }
    LOG_TRACE("XWM: skipped unchanged configure window=%u (%d,%d) %dx%d",
              window->id, x, y, width, height);
    return;
  }

  LOG_DEBUG("XWM: xwm_window_configure window=%u (%d,%d) %dx%d", window->id, x,
            y, width, height);

  window->x = x;
  window->y = y;
  window->width = width;
  window->height = height;

  if (window->surface) {
    window->surface->screen_x = x;
    window->surface->screen_y = y;
    window->surface->screen_width = width;
    window->surface->screen_height = height;
    window->surface->screen_pos_valid = true;
  }

  uint32_t values[4] = {x, y, width, height};
  uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                  XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;

  xcb_configure_window(xwm->conn, window->id, mask, values);
  xwm_window_send_configure_notify(window);
  xcb_flush(xwm->conn);
}

bool xwm_window_is_focus_inactive(struct xwm_window *window) {
  if (!window)
    return false;
  if (window->override_redirect)
    return true;
  struct xwm *xwm = window->xwm;
  if (!xwm)
    return false;
  return window->type == xwm->atoms.net_wm_window_type_tooltip ||
         window->type == xwm->atoms.net_wm_window_type_notification ||
         window->type == xwm->atoms.net_wm_window_type_combo ||
         window->type == xwm->atoms.net_wm_window_type_dnd ||
         window->type == xwm->atoms.net_wm_window_type_popup ||
         window->type == xwm->atoms.net_wm_window_type_dropdown;
}

void xwm_set_focus(struct xwm *xwm, struct xwm_window *window) {
  if (window) {
    if (window->override_redirect)
      return;

    if (window->take_focus) {
      char buf[32] = {0};
      xcb_client_message_event_t *ev = (void *)buf;
      ev->response_type = XCB_CLIENT_MESSAGE;
      ev->format = 32;
      ev->window = window->id;
      ev->type = xwm->atoms.wm_protocols;
      ev->data.data32[0] = xwm->atoms.wm_take_focus;
      ev->data.data32[1] = XCB_TIME_CURRENT_TIME;
      xcb_send_event(xwm->conn, 0, window->id, XCB_EVENT_MASK_NO_EVENT, buf);
    }

    if (window->wants_input) {

      xwm->focus_window = window;
      xwm->pending_focus_window = window;
      xcb_void_cookie_t ck =
          xcb_set_input_focus(xwm->conn, XCB_INPUT_FOCUS_POINTER_ROOT,
                              window->id, XCB_TIME_CURRENT_TIME);
      xwm->last_focus_seq = ck.sequence;
    } else {

      xwm->focus_window = window;
    }

    xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, xwm->screen->root,
                        xwm->atoms.net_active_window, xwm->atoms.window, 32, 1,
                        &window->id);

    uint32_t stack_above = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(xwm->conn, window->id, XCB_CONFIG_WINDOW_STACK_MODE,
                         &stack_above);
  } else {
    xwm->pending_focus_window = NULL;

    xcb_window_t sink = xwm->no_focus_window ? xwm->no_focus_window : XCB_NONE;
    xcb_void_cookie_t ck = xcb_set_input_focus(
        xwm->conn, XCB_INPUT_FOCUS_POINTER_ROOT, sink, XCB_TIME_CURRENT_TIME);
    xwm->last_focus_seq = ck.sequence;

    xcb_window_t none =
        xwm->no_focus_window ? xwm->no_focus_window : XCB_WINDOW_NONE;
    xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, xwm->screen->root,
                        xwm->atoms.net_active_window, xwm->atoms.window, 32, 1,
                        &none);
    xwm->focus_window = NULL;
  }

  xcb_flush(xwm->conn);
}

static void handle_create_notify(struct xwm *xwm,
                                 xcb_create_notify_event_t *event) {
  if (our_resource(xwm, event->window))
    return;

  LOG_INFO("XWM: CREATE_NOTIFY window=%u at (%d,%d) %dx%d%s", event->window,
           event->x, event->y, event->width, event->height,
           event->override_redirect ? " override-redirect" : "");

  xwm_window_create(xwm, event->window, event->width, event->height, event->x,
                    event->y, event->override_redirect);
}

static void handle_destroy_notify(struct xwm *xwm,
                                  xcb_destroy_notify_event_t *event) {
  if (our_resource(xwm, event->window))
    return;

  LOG_DEBUG("XWM: XCB_DESTROY_NOTIFY window=%u", event->window);

  struct xwm_window *window = xwm_get_window(xwm, event->window);
  if (window)
    xwm_window_destroy(window);
}

void xwm_window_try_pair_surface(struct xwm_window *window) {
  if (!window->surface_id || window->surface)
    return;

  struct bridge_surface *surface;
  wl_list_for_each(surface, &window->xwm->bridge->surfaces, link) {
    if (wl_resource_get_id(surface->resource) == window->surface_id) {
      if (!xwm_bind_surface_window(window, surface, "TRY_PAIR"))
        break;

      LOG_INFO("XWM: paired window %u '%s' with surface %u", window->id,
               window->name ? window->name : "(null)", window->surface_id);

      LOG_TRACE("XWM: TRY_PAIR set surface->xwm_window=%p for window=%u",
                (void *)window, window->id);

      if (window->name && surface->xdg_toplevel) {
      }

      wl_list_remove(&window->link);
      wl_list_insert(&window->xwm->window_list, &window->link);

      return;
    }
  }

  LOG_DEBUG("XWM: window %u waiting for surface %u", window->id,
            window->surface_id);
  wl_list_remove(&window->link);
  wl_list_insert(&window->xwm->unpaired_list, &window->link);
}

void xwm_surface_created(struct xwm *xwm, struct bridge_surface *surface,
                         uint32_t surface_id) {
  if (!xwm)
    return;

  LOG_DEBUG("XWM: checking surface %u against unpaired windows", surface_id);

  struct xwm_window *window, *tmp;
  wl_list_for_each_safe(window, tmp, &xwm->unpaired_list, link) {
    if (window->surface_id == surface_id) {
      if (!xwm_bind_surface_window(window, surface, "DEFERRED2"))
        continue;

      LOG_INFO("XWM: paired window %u '%s' with surface %u (deferred)",
               window->id, window->name ? window->name : "(null)", surface_id);

      LOG_TRACE("XWM: DEFERRED2 set surface->xwm_window=%p for window=%u",
                (void *)window, window->id);

      wl_list_remove(&window->link);
      wl_list_insert(&xwm->window_list, &window->link);
      return;
    }
  }

  wl_list_for_each(window, &xwm->window_list, link) {
    if (window->surface_id == surface_id && !window->surface) {
      if (!xwm_bind_surface_window(window, surface, "NORMAL"))
        continue;

      LOG_INFO("XWM: paired window %u '%s' with surface %u", window->id,
               window->name ? window->name : "(null)", surface_id);

      LOG_TRACE("XWM: NORMAL set surface->xwm_window=%p for window=%u",
                (void *)window, window->id);

      return;
    }
  }
}

static void xwm_update_client_list(struct xwm *xwm) {
  size_t count = 0;
  struct xwm_window *w;
  wl_list_for_each(w, &xwm->window_list, link) if (w->mapped) count++;

  xcb_window_t *wins = count ? malloc(count * sizeof(*wins)) : NULL;
  size_t i = 0;
  wl_list_for_each(w, &xwm->window_list, link) if (w->mapped) wins[i++] = w->id;

  xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, xwm->screen->root,
                      xwm->atoms.net_client_list, XCB_ATOM_WINDOW, 32, count,
                      wins ? wins : (void *)"");
  free(wins);
}

static void handle_map_request(struct xwm *xwm,
                               xcb_map_request_event_t *event) {
  if (our_resource(xwm, event->window))
    return;

  struct xwm_window *window = xwm_get_window(xwm, event->window);
  if (!window)
    return;

  xwm_window_read_properties(window);

  LOG_INFO("XWM: MAP_REQUEST window=%u name='%s' class='%s' %dx%d",
           event->window, window->name ? window->name : "(null)",
           window->class ? window->class : "(null)", window->width,
           window->height);

  xwm_window_set_allow_commits(window, false);

  xwm_window_set_wm_state(window, XWM_ICCCM_NORMAL_STATE);
  xwm_window_set_net_wm_state(window);

  xcb_map_window(xwm->conn, event->window);

  xwm_window_set_allow_commits(window, true);

  window->mapped = true;
  xwm_update_client_list(xwm);
}

static void xwm_clamp_popup_pos(struct xwm_window *window) {
  if (window->x < 0)
    window->x = 0;
  if (window->y < 0)
    window->y = 0;
}

static void xwm_reactivate_popup_parent(struct xwm *xwm,
                                        struct xwm_window *window) {
  if (!window || window->override_redirect)
    return;
  struct xwm_window *parent = window->popup_parent_window;
  if (!parent || !parent->surface || !parent->surface->plexy_window_id)
    return;
  if (!xwm->bridge || !xwm->bridge->plexy_conn)
    return;

  if (xwm->focus_window && xwm->focus_window != parent) {
    LOG_INFO("XWM: skipping re-activation of popup parent window=%u "
             "(popup=%u): current focus is window=%u — not raising background "
             "window",
             parent->id, window->id, xwm->focus_window->id);
    return;
  }
  LOG_INFO("XWM: re-activating popup parent window=%u after popup=%u creation "
           "to restore z_order",
           parent->id, window->id);
  plexy_activate_window(xwm->bridge->plexy_conn,
                        parent->surface->plexy_window_id);
}

static void handle_map_notify(struct xwm *xwm, xcb_map_notify_event_t *event) {
  if (our_resource(xwm, event->window))
    return;

  LOG_DEBUG("XWM: XCB_MAP_NOTIFY window=%u%s", event->window,
            event->override_redirect ? " override-redirect" : "");

  if (event->override_redirect) {

    if (event->event != event->window)
      return;

    struct xwm_window *window = xwm_get_window(xwm, event->window);
    if (!window)
      return;

    window->override_redirect = true;
    xwm_window_read_properties(window);
    window->mapped = true;

    uint16_t pre_w = window->width, pre_h = window->height;

    xwm_window_ensure_real_geometry(window);

    bool was_tiny =
        (pre_w < 4 || pre_h < 4) && (window->width >= 4 && window->height >= 4);
    if (was_tiny && window->x == 0 && window->y == 0) {
      xcb_query_pointer_cookie_t pc =
          xcb_query_pointer(xwm->conn, xwm->screen->root);
      xcb_query_pointer_reply_t *pr =
          xcb_query_pointer_reply(xwm->conn, pc, NULL);
      if (pr) {
        window->x = pr->root_x;
        window->y = pr->root_y;
        free(pr);
      }
      uint32_t vals[] = {window->x, window->y, window->width, window->height};
      xcb_configure_window(xwm->conn, window->id,
                           XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                               XCB_CONFIG_WINDOW_WIDTH |
                               XCB_CONFIG_WINDOW_HEIGHT,
                           vals);
      xwm_window_send_configure_notify(window);
      xcb_flush(xwm->conn);
      LOG_INFO("XWM: GTK3 popup fix — configured OR window %u "
               "from 1x1@(0,0) to %dx%d@(%d,%d)",
               window->id, window->width, window->height, window->x, window->y);
    }

    LOG_INFO("XWM: MAP_NOTIFY override-redirect window=%u name='%s' class='%s' "
             "type=%u %dx%d",
             window->id, window->name ? window->name : "(null)",
             window->class ? window->class : "(null)", window->type,
             window->width, window->height);

    if (window->surface && !window->surface->plexy_window &&
        window->width >= 4 && window->height >= 4) {
      uint32_t plexy_type = xwm_window_get_plexy_type(window);
      PlexyWindow *parent = xwm_window_resolve_and_cache_parent(window);
      xwm_clamp_popup_pos(window);
      int32_t rel_x, rel_y;
      xwm_popup_rel_coords(window, &rel_x, &rel_y);
      LOG_INFO("XWM: creating popup for override-redirect window %u "
               "(plexy_type=%u) parent=%p abs=(%d,%d) rel=(%d,%d)",
               window->id, plexy_type, (void *)parent, window->x, window->y,
               rel_x, rel_y);
      bridge_create_x11_popup(window->surface, window->name, parent, rel_x,
                              rel_y, window->width, window->height, plexy_type);
      xwm_reactivate_popup_parent(xwm, window);
    }
  }
}

static void handle_unmap_notify(struct xwm *xwm,
                                xcb_unmap_notify_event_t *event) {
  if (our_resource(xwm, event->window))
    return;

  if (event->response_type & XWM_SEND_EVENT_MASK)
    return;

  LOG_DEBUG("XWM: XCB_UNMAP_NOTIFY window=%u", event->window);

  struct xwm_window *window = xwm_get_window(xwm, event->window);
  if (!window)
    return;

  window->mapped = false;

  if (window->surface && window->surface->plexy_window &&
      window->override_redirect) {
    LOG_INFO("XWM: unmapping override-redirect window %u, destroying popup",
             window->id);
    PlexyWindow *plexy_window = window->surface->plexy_window;
    plexy_destroy_window(plexy_window);
    bridge_surface_clear_plexy_window(window->surface);
  }

  if (window->surface && window->surface->xwm_window == window)
    window->surface->xwm_window = NULL;
  window->surface = NULL;
  window->surface_id = 0;
  window->surface_serial = 0;

  xwm_window_set_wm_state(window, XWM_ICCCM_WITHDRAWN_STATE);
  xwm_update_client_list(xwm);
}

static void handle_configure_request(struct xwm *xwm,
                                     xcb_configure_request_event_t *event) {
  struct xwm_window *window = xwm_get_window(xwm, event->window);
  if (!window)
    return;

  LOG_INFO("XWM: CONFIGURE_REQUEST window=%u mask=0x%x req=(%d,%d) %dx%d "
           "cur=(%d,%d) %dx%d or=%d",
           event->window, event->value_mask, event->x, event->y, event->width,
           event->height, window->x, window->y, window->width, window->height,
           window->override_redirect);

  uint16_t mask = event->value_mask;
  if (!window->override_redirect) {
    mask &= ~(XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y);
  }

  if (mask & XCB_CONFIG_WINDOW_X)
    window->x = event->x;
  if (mask & XCB_CONFIG_WINDOW_Y)
    window->y = event->y;
  if (mask & XCB_CONFIG_WINDOW_WIDTH)
    window->width = event->width;
  if (mask & XCB_CONFIG_WINDOW_HEIGHT)
    window->height = event->height;

  uint32_t values[7];
  int i = 0;

  if (mask & XCB_CONFIG_WINDOW_X)
    values[i++] = event->x;
  if (mask & XCB_CONFIG_WINDOW_Y)
    values[i++] = event->y;
  if (mask & XCB_CONFIG_WINDOW_WIDTH)
    values[i++] = event->width;
  if (mask & XCB_CONFIG_WINDOW_HEIGHT)
    values[i++] = event->height;
  if (mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
    values[i++] = event->border_width;
  if (mask & XCB_CONFIG_WINDOW_SIBLING)
    values[i++] = event->sibling;
  if (mask & XCB_CONFIG_WINDOW_STACK_MODE)
    values[i++] = event->stack_mode;

  if (mask)
    xcb_configure_window(xwm->conn, event->window, mask, values);

  if ((event->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) &&
      event->stack_mode == XCB_STACK_MODE_ABOVE && window->surface &&
      window->surface->plexy_window_id && xwm->bridge &&
      xwm->bridge->plexy_conn) {

    plexy_activate_window(xwm->bridge->plexy_conn,
                          window->surface->plexy_window_id);
  }

  xwm_window_send_configure_notify(window);
}

static void handle_configure_notify(struct xwm *xwm,
                                    xcb_configure_notify_event_t *event) {
  struct xwm_window *window = xwm_get_window(xwm, event->window);
  if (!window)
    return;

  int16_t old_x = window->x, old_y = window->y;
  uint16_t old_w = window->width, old_h = window->height;

  if (!window->override_redirect && window->fullscreen && window->surface &&
      window->surface->plexy_window &&
      (event->x != window->x || event->y != window->y ||
       event->width != window->width || event->height != window->height)) {
    LOG_INFO("XWM: CONFIGURE_NOTIFY fullscreen window=%u event=(%d,%d) "
             "%dx%d differs from WM geometry=(%d,%d) %dx%d, keeping WM "
             "geometry",
             event->window, event->x, event->y, event->width, event->height,
             window->x, window->y, window->width, window->height);

    window->surface->screen_x = window->x;
    window->surface->screen_y = window->y;
    window->surface->screen_width = window->width;
    window->surface->screen_height = window->height;
    window->surface->screen_pos_valid = true;
    goto check_override;
  }

  if (!window->override_redirect && window->surface &&
      window->surface->plexy_window) {
    if (event->x != window->x || event->y != window->y) {
      LOG_INFO("XWM: CONFIGURE_NOTIFY managed window=%u event pos=(%d,%d) "
               "differs from WM pos=(%d,%d), keeping WM pos",
               event->window, event->x, event->y, window->x, window->y);

      window->width = event->width;
      window->height = event->height;

      window->surface->screen_x = window->x;
      window->surface->screen_y = window->y;
      window->surface->screen_width = window->width;
      window->surface->screen_height = window->height;
      window->surface->screen_pos_valid = true;
      goto check_override;
    }
  }

  window->x = event->x;
  window->y = event->y;
  window->width = event->width;
  window->height = event->height;

check_override:
  if (window->override_redirect) {

    int32_t root_x = window->x, root_y = window->y;
    xwm_translate_to_root(xwm, window->id, &root_x, &root_y);
    window->x = root_x;
    window->y = root_y;

    LOG_INFO("XWM: CONFIGURE_NOTIFY OR window=%u at (%d,%d) %dx%d [old (%d,%d) "
             "%dx%d] mapped=%d surface=%p plexy=%p",
             event->window, root_x, root_y, event->width, event->height, old_x,
             old_y, old_w, old_h, window->mapped, (void *)window->surface,
             window->surface ? (void *)window->surface->plexy_window : NULL);
  }

  if (event->width > window->create_width)
    window->create_width = event->width;
  if (event->height > window->create_height)
    window->create_height = event->height;

  if (!window->override_redirect || !window->mapped || !window->surface)
    return;

  bool size_changed = (window->width != old_w || window->height != old_h);
  bool pos_changed = (window->x != old_x || window->y != old_y);

  if (!window->surface->plexy_window) {

    if (window->width < 4 || window->height < 4)
      return;
    if (window->properties_dirty)
      xwm_window_read_properties(window);
    uint32_t plexy_type = xwm_window_get_plexy_type(window);

    PlexyWindow *parent = xwm_window_resolve_and_cache_parent(window);
    xwm_clamp_popup_pos(window);
    int32_t rel_x, rel_y;
    xwm_popup_rel_coords(window, &rel_x, &rel_y);
    LOG_INFO("XWM: deferred popup creation for window %u now %dx%d at (%d,%d) "
             "rel=(%d,%d) type=%u",
             window->id, window->width, window->height, window->x, window->y,
             rel_x, rel_y, plexy_type);
    bridge_create_x11_popup(window->surface, window->name, parent, rel_x, rel_y,
                            window->width, window->height, plexy_type);
    xwm_reactivate_popup_parent(xwm, window);
  } else if (size_changed || pos_changed) {
    if (window->width < 4 || window->height < 4)
      return;
    uint32_t plexy_type = xwm_window_get_plexy_type(window);
    xwm_clamp_popup_pos(window);
    int32_t rel_x2, rel_y2;
    xwm_popup_rel_coords(window, &rel_x2, &rel_y2);
    LOG_INFO("XWM: override-redirect window %u reconfigured %dx%d at (%d,%d) "
             "rel=(%d,%d), updating popup in place",
             window->id, window->width, window->height, window->x, window->y,
             rel_x2, rel_y2);
    if (bridge_update_x11_popup(window->surface, rel_x2, rel_y2, window->width,
                                window->height, plexy_type)) {
      window->surface->screen_x = window->x;
      window->surface->screen_y = window->y;
      window->surface->screen_width = window->width;
      window->surface->screen_height = window->height;
      window->surface->screen_pos_valid = true;
    }
  }
}

static void handle_property_notify(struct xwm *xwm,
                                   xcb_property_notify_event_t *event) {

  if (xwm->incr_active && event->window == xwm->wm_window &&
      event->atom == xwm->incr_property &&
      event->state == XCB_PROPERTY_NEW_VALUE) {

    xcb_get_property_reply_t *reply = xcb_get_property_reply(
        xwm->conn,
        xcb_get_property(xwm->conn, 1, xwm->wm_window, xwm->incr_property,
                         XCB_ATOM_ANY, 0, 0x1fffffff),
        NULL);
    if (!reply)
      return;

    int chunk_len = xcb_get_property_value_length(reply);

    if (chunk_len == 0) {

      if (xwm->incr_buffer && xwm->incr_buffer_len > 0) {
        char *text = malloc(xwm->incr_buffer_len + 1);
        if (text) {
          memcpy(text, xwm->incr_buffer, xwm->incr_buffer_len);
          text[xwm->incr_buffer_len] = '\0';
          LOG_INFO("XWM: INCR transfer complete: %zu bytes",
                   xwm->incr_buffer_len);
          bridge_clipboard_set_text(text);
          free(text);
        }
      }
      free(xwm->incr_buffer);
      xwm->incr_buffer = NULL;
      xwm->incr_buffer_len = 0;
      xwm->incr_buffer_cap = 0;
      xwm->incr_active = false;
      free(reply);
      return;
    }

    size_t needed = xwm->incr_buffer_len + (size_t)chunk_len;
    if (needed > xwm->incr_buffer_cap) {
      size_t new_cap = needed * 2;
      char *new_buf = realloc(xwm->incr_buffer, new_cap);
      if (!new_buf) {
        LOG_ERROR("XWM: INCR realloc failed, aborting transfer");
        free(xwm->incr_buffer);
        xwm->incr_buffer = NULL;
        xwm->incr_buffer_len = 0;
        xwm->incr_buffer_cap = 0;
        xwm->incr_active = false;
        free(reply);
        return;
      }
      xwm->incr_buffer = new_buf;
      xwm->incr_buffer_cap = new_cap;
    }

    memcpy(xwm->incr_buffer + xwm->incr_buffer_len,
           xcb_get_property_value(reply), (size_t)chunk_len);
    xwm->incr_buffer_len += (size_t)chunk_len;

    LOG_DEBUG("XWM: INCR chunk %d bytes (total %zu)", chunk_len,
              xwm->incr_buffer_len);
    free(reply);
    return;
  }

  struct xwm_window *window = xwm_get_window(xwm, event->window);
  if (!window)
    return;

  if (event->atom == xwm->atoms.wl_surface_id) {
    if (event->state == XCB_PROPERTY_DELETE)
      return;

    xcb_get_property_cookie_t cookie =
        xcb_get_property(xwm->conn, 0, window->id, xwm->atoms.wl_surface_id,
                         XCB_ATOM_CARDINAL, 0, 1);
    xcb_get_property_reply_t *reply =
        xcb_get_property_reply(xwm->conn, cookie, NULL);
    if (reply && xcb_get_property_value_length(reply) >= 4) {
      window->surface_id = *(uint32_t *)xcb_get_property_value(reply);
      LOG_INFO("XWM: window %u has WL_SURFACE_ID=%u", window->id,
               window->surface_id);
      xwm_window_try_pair_surface(window);
    }
    free(reply);
    return;
  }

  if (event->atom == xwm->atoms.xquadro_window_id) {
    if (event->state == XCB_PROPERTY_DELETE)
      return;

    xcb_get_property_cookie_t cookie =
        xcb_get_property(xwm->conn, 0, window->id, xwm->atoms.xquadro_window_id,
                         XCB_ATOM_CARDINAL, 0, 1);
    xcb_get_property_reply_t *reply =
        xcb_get_property_reply(xwm->conn, cookie, NULL);
    if (reply && xcb_get_property_value_length(reply) >= 4) {
      window->xquadro_window_id = *(uint32_t *)xcb_get_property_value(reply);
      LOG_INFO("XWM: window %u has _XQUADRO_WINDOW_ID=%u", window->id,
               window->xquadro_window_id);

      struct bridge_surface *surface =
          bridge_surface_from_window_id(window->xquadro_window_id);
      if (surface && surface->resource) {
        window->surface_id = wl_resource_get_id(surface->resource);
        LOG_INFO("XWM: Xquadro paired window %u with bridge_surface id=%u",
                 window->id, window->surface_id);
        xwm_window_try_pair_surface(window);
      }
    }
    free(reply);
    return;
  }

  xcb_atom_t atom = event->atom;
  if (event->state == XCB_PROPERTY_DELETE)
    return;

  xcb_get_property_cookie_t cookie =
      xcb_get_property(xwm->conn, 0, window->id, atom, XCB_ATOM_ANY, 0, 2048);
  xcb_get_property_reply_t *reply =
      xcb_get_property_reply(xwm->conn, cookie, NULL);
  if (!reply)
    return;

  if (atom == xwm->atoms.net_wm_name || atom == XCB_ATOM_WM_NAME) {
    int len = xcb_get_property_value_length(reply);
    if (len > 0) {
      free(window->name);
      window->name = malloc(len + 1);
      if (window->name) {
        memcpy(window->name, xcb_get_property_value(reply), len);
        window->name[len] = '\0';
      }
    }
    if (window->surface && window->surface->plexy_window && window->name)
      plexy_window_set_title(window->surface->plexy_window, window->name);
  } else if (atom == XCB_ATOM_WM_CLASS) {
    int len = xcb_get_property_value_length(reply);
    if (len > 0) {
      char *str = xcb_get_property_value(reply);
      int first = strnlen(str, len);
      if (first < len - 1) {
        char *second = str + first + 1;
        int slen = strnlen(second, len - first - 1);
        free(window->class);
        window->class = malloc(slen + 1);
        if (window->class) {
          memcpy(window->class, second, slen);
          window->class[slen] = '\0';
        }
      }
      if (window->surface && window->surface->plexy_window && window->class)
        plexy_window_set_app_id(window->surface->plexy_window, window->class);
    }
  } else if (atom == xwm->atoms.motif_wm_hints) {
    if (xcb_get_property_value_length(reply) >= (int)(5 * sizeof(uint32_t))) {
      uint32_t *m = xcb_get_property_value(reply);
      memcpy(&window->motif_hints, m, sizeof(struct xwm_motif_hints));
      if (window->motif_hints.flags & XWM_MWM_HINTS_DECORATIONS) {
        uint32_t d = window->motif_hints.decorations;
        window->decorate = (d & XWM_MWM_DECOR_ALL) ||
                           (d & (XWM_MWM_DECOR_TITLE | XWM_MWM_DECOR_BORDER));
      }
    }
  } else if (atom == xwm->atoms.net_wm_window_type) {
    if (xcb_get_property_value_length(reply) >= (int)sizeof(xcb_atom_t))
      window->type = *(xcb_atom_t *)xcb_get_property_value(reply);
  } else if (atom == xwm->atoms.wm_protocols) {
    xcb_atom_t *atoms = xcb_get_property_value(reply);
    int n_atoms = xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);
    window->delete_window = false;
    window->take_focus = false;
    for (int i = 0; i < n_atoms; i++) {
      if (atoms[i] == xwm->atoms.wm_delete_window)
        window->delete_window = true;
      else if (atoms[i] == xwm->atoms.wm_take_focus)
        window->take_focus = true;
    }
  } else if (atom == xwm->atoms.wm_hints) {
    if (xcb_get_property_value_length(reply) >= (int)(4 * sizeof(uint32_t))) {
      uint32_t *h = xcb_get_property_value(reply);
      if (h[0] & (1u << 0))
        window->wants_input = (bool)h[1];
    }
  } else if (atom == XCB_ATOM_WM_TRANSIENT_FOR) {
    if (xcb_get_property_value_length(reply) >= (int)sizeof(xcb_window_t)) {
      xcb_window_t pid = *(xcb_window_t *)xcb_get_property_value(reply);
      window->transient_for = xwm_get_window(xwm, pid);
    }
  }

  free(reply);
  window->properties_dirty = false;
}

static void xwm_try_pair_window_with_surface(struct xwm *xwm,
                                             struct xwm_window *window);

static void handle_client_message(struct xwm *xwm,
                                  xcb_client_message_event_t *event) {
  struct xwm_window *window = xwm_get_window(xwm, event->window);

  if (event->type == xwm->atoms.net_wm_state) {

    if (!window)
      return;

    uint32_t action = event->data.data32[0];
    xcb_atom_t prop1 = event->data.data32[1];
    xcb_atom_t prop2 = event->data.data32[2];

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2

    for (int i = 0; i < 2; i++) {
      xcb_atom_t prop = (i == 0) ? prop1 : prop2;
      if (!prop)
        continue;

      bool *flag = NULL;
      if (prop == xwm->atoms.net_wm_state_fullscreen)
        flag = &window->fullscreen;
      else if (prop == xwm->atoms.net_wm_state_maximized_vert)
        flag = &window->maximized_vert;
      else if (prop == xwm->atoms.net_wm_state_maximized_horz)
        flag = &window->maximized_horz;
      else if (prop == xwm->atoms.net_wm_state_above)
        flag = &window->above;
      else if (prop == xwm->atoms.net_wm_state_below)
        flag = &window->below;
      else if (prop == xwm->atoms.net_wm_state_modal)
        flag = &window->modal;
      else if (prop == xwm->atoms.net_wm_state_hidden)
        flag = &window->hidden;

      if (flag) {
        if (action == _NET_WM_STATE_TOGGLE)
          *flag = !*flag;
        else
          *flag = (action == _NET_WM_STATE_ADD);
      }
    }

    xwm_window_set_net_wm_state(window);

    if (window->surface && window->surface->plexy_window) {
      bool maximized = window->maximized_vert && window->maximized_horz;

      if (window->fullscreen) {
        uint32_t sw = xwm->screen->width_in_pixels;
        uint32_t sh = xwm->screen->height_in_pixels;
        xwm_window_configure(window, 0, 0, sw, sh);
        plexy_window_request_fullscreen(window->surface->plexy_window, true);
        window->surface->fullscreen = true;
        window->surface->maximized = false;
      } else if (maximized) {
        plexy_window_request_fullscreen(window->surface->plexy_window, false);
        plexy_window_request_maximize(window->surface->plexy_window, true);
        window->surface->maximized = true;
        window->surface->fullscreen = false;
      } else {

        plexy_window_request_fullscreen(window->surface->plexy_window, false);
        plexy_window_request_maximize(window->surface->plexy_window, false);
        window->surface->maximized = false;
        window->surface->fullscreen = false;
      }

      if (window->above && xwm->bridge && xwm->bridge->plexy_conn)
        plexy_activate_window(xwm->bridge->plexy_conn,
                              window->surface->plexy_window_id);
    }
  } else if (event->type == xwm->atoms.net_active_window) {

    if (window && !xwm_window_is_focus_inactive(window) && window->surface &&
        window->surface->plexy_window && xwm->bridge &&
        xwm->bridge->plexy_conn) {
      LOG_DEBUG("XWM: _NET_ACTIVE_WINDOW for %u → routing through compositor",
                window->id);
      plexy_activate_window(xwm->bridge->plexy_conn,
                            window->surface->plexy_window_id);
    }
  } else if (event->type == xwm->atoms.wm_protocols) {

    if (!window)
      return;
    if (event->data.data32[0] == xwm->atoms.net_wm_ping &&
        event->data.data32[2] == window->id) {
      LOG_DEBUG("XWM: window %u sent pong", window->id);
    }
  } else if (event->type == xwm->atoms.net_wm_moveresize) {

    if (!window || !window->surface || !window->surface->plexy_window)
      return;
    uint32_t direction = event->data.data32[2];
    LOG_DEBUG("XWM: window %u requests move/resize direction=%u", window->id,
              direction);

    if (direction == 8 || direction == 10) {

      plexy_window_request_move(window->surface->plexy_window);
    } else {
      static const uint32_t dir_to_edges[11] = {
          4 | 1, 4, 4 | 2, 2, 8 | 2, 8, 8 | 1, 1, 0, 8 | 2, 0,
      };
      uint32_t edges = (direction < 11) ? dir_to_edges[direction] : 0;
      if (edges)
        plexy_window_request_resize(window->surface->plexy_window, edges);
    }
  } else if (event->type == xwm->atoms.wm_change_state) {
    if (!window)
      return;
    if (event->data.data32[0] == XWM_ICCCM_ICONIC_STATE) {

      xwm_window_set_wm_state(window, XWM_ICCCM_ICONIC_STATE);
      xcb_unmap_window(xwm->conn, window->id);
    }
  } else if (event->type == xwm->atoms.wl_surface_id && !xwm->shell_bound) {
    if (!window)
      return;
    window->surface_id = event->data.data32[0];
    LOG_INFO("XWM: window %u received WL_SURFACE_ID=%u (legacy)", window->id,
             window->surface_id);
    xwm_window_try_pair_surface(window);
  } else if (event->type == xwm->atoms.wl_surface_serial) {
    if (!window)
      return;
    uint64_t serial =
        u64_from_u32s(event->data.data32[1], event->data.data32[0]);
    window->surface_serial = serial;
    LOG_INFO("XWM: window %u received WL_SURFACE_SERIAL=%lu", window->id,
             (unsigned long)serial);
    wl_list_remove(&window->link);
    wl_list_init(&window->link);
    xwm_try_pair_window_with_surface(xwm, window);
  }
}

static void handle_selection_request(struct xwm *xwm,
                                     xcb_selection_request_event_t *ev) {

  xcb_selection_notify_event_t notify = {
      .response_type = XCB_SELECTION_NOTIFY,
      .time = ev->time,
      .requestor = ev->requestor,
      .selection = ev->selection,
      .target = ev->target,
      .property = XCB_ATOM_NONE,
  };

  bool is_clipboard =
      (ev->selection == xwm->atoms.clipboard && xwm->clipboard_owner);
  bool is_primary = (ev->selection == xwm->atoms.primary && xwm->primary_owner);
  if (!is_clipboard && !is_primary) {
    LOG_DEBUG("XWM: SelectionRequest for non-owned selection %u (requestor=%u)",
              ev->selection, ev->requestor);
    goto send;
  }

  xcb_atom_t prop = ev->property != XCB_ATOM_NONE ? ev->property : ev->target;

  if (ev->target == xwm->atoms.targets) {

    xcb_atom_t supported[] = {
        xwm->atoms.targets,
        xwm->atoms.timestamp,
        xwm->atoms.utf8_string,
        XCB_ATOM_STRING,
    };
    xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, ev->requestor, prop,
                        XCB_ATOM_ATOM, 32,
                        sizeof(supported) / sizeof(supported[0]), supported);
    notify.property = prop;

  } else if (ev->target == xwm->atoms.utf8_string ||
             ev->target == XCB_ATOM_STRING) {
    char *text = calloc(1, PLEXY_CLIPBOARD_MAX);
    bool got =
        text &&
        (is_primary ? bridge_primary_get_text(text, PLEXY_CLIPBOARD_MAX)
                    : bridge_clipboard_get_text(text, PLEXY_CLIPBOARD_MAX));
    if (got && text[0]) {
      xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, ev->requestor, prop,
                          ev->target, 8, strlen(text), text);
      notify.property = prop;
      LOG_INFO("XWM: Sent clipboard text (%zu bytes) to requestor=%u",
               strlen(text), ev->requestor);
    } else {
      LOG_INFO("XWM: SelectionRequest for %s text but clipboard is empty "
               "(active=%d)",
               is_clipboard ? "CLIPBOARD" : "PRIMARY", got);
    }
    free(text);

  } else if (ev->target == xwm->atoms.timestamp) {
    uint32_t ts = 0;
    xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, ev->requestor, prop,
                        XCB_ATOM_INTEGER, 32, 1, &ts);
    notify.property = prop;
  }

send:
  xcb_send_event(xwm->conn, 0, ev->requestor, XCB_EVENT_MASK_NO_EVENT,
                 (char *)&notify);
  xcb_flush(xwm->conn);
}

static void handle_selection_notify(struct xwm *xwm,
                                    xcb_selection_notify_event_t *ev) {
  if (ev->requestor != xwm->wm_window)
    return;
  bool is_clipboard = (ev->selection == xwm->atoms.clipboard);
  bool is_primary = (ev->selection == xwm->atoms.primary);
  if (!is_clipboard && !is_primary)
    return;
  if (ev->property == XCB_ATOM_NONE) {
    LOG_INFO("XWM: SelectionNotify: selection owner refused to convert %s",
             is_clipboard ? "CLIPBOARD" : "PRIMARY");
    return;
  }

  xcb_get_property_reply_t *reply = xcb_get_property_reply(
      xwm->conn,
      xcb_get_property(xwm->conn, 1, xwm->wm_window, ev->property, XCB_ATOM_ANY,
                       0, 0x1fffffff),
      NULL);
  if (!reply)
    return;

  if (reply->type == xwm->atoms.incr) {
    xwm->incr_active = true;
    xwm->incr_property = ev->property;
    xwm->incr_buffer_len = 0;

    uint32_t size_hint = 0;
    if (xcb_get_property_value_length(reply) >= 4)
      size_hint = *(uint32_t *)xcb_get_property_value(reply);
    if (size_hint < 4096)
      size_hint = 4096;

    free(xwm->incr_buffer);
    xwm->incr_buffer = malloc(size_hint);
    xwm->incr_buffer_cap = xwm->incr_buffer ? size_hint : 0;

    xcb_delete_property(xwm->conn, xwm->wm_window, ev->property);
    xcb_flush(xwm->conn);

    LOG_DEBUG("XWM: INCR transfer started, size_hint=%u", size_hint);
    free(reply);
    return;
  }

  int len = xcb_get_property_value_length(reply);
  if (len > 0) {
    char *text = malloc((size_t)len + 1);
    if (text) {
      memcpy(text, xcb_get_property_value(reply), (size_t)len);
      text[len] = '\0';
      LOG_INFO("XWM: X11→Wayland %s: %d bytes",
               is_primary ? "primary" : "clipboard", len);
      if (is_primary)
        bridge_primary_set_text(text);
      else
        bridge_clipboard_set_text(text);
      free(text);
    }
  }
  free(reply);
}

static void
handle_xfixes_selection_notify(struct xwm *xwm,
                               xcb_xfixes_selection_notify_event_t *ev) {
  bool is_clipboard = (ev->selection == xwm->atoms.clipboard);
  bool is_primary = (ev->selection == xwm->atoms.primary);
  if (!is_clipboard && !is_primary)
    return;

  if (ev->owner == xwm->wm_window) {
    return;
  }

  if (ev->owner == XCB_WINDOW_NONE) {
    if (is_clipboard)
      xwm->clipboard_owner = false;
    if (is_primary)
      xwm->primary_owner = false;
    return;
  }

  if (is_clipboard) {
    xwm->clipboard_owner = false;
  } else {
    xwm->primary_owner = false;
  }
  xcb_convert_selection(
      xwm->conn, xwm->wm_window, ev->selection, xwm->atoms.utf8_string,
      is_primary ? xwm->atoms._wl_primary_selection : xwm->atoms.wl_selection,
      ev->timestamp);
  xcb_flush(xwm->conn);
  LOG_INFO("XWM: X11 app took %s (owner=%u), requesting UTF8_STRING",
           is_primary ? "PRIMARY" : "CLIPBOARD", ev->owner);
}

static void xwm_on_wayland_clipboard_change(void *data) {
  struct xwm *xwm = data;
  if (!xwm || !xwm->conn || !xwm->screen)
    return;
  if (!bridge_clipboard_has_text())
    return;

  xcb_set_selection_owner(xwm->conn, xwm->wm_window, xwm->atoms.clipboard,
                          XCB_CURRENT_TIME);
  xwm->clipboard_owner = true;
  xcb_flush(xwm->conn);
  LOG_INFO("XWM: Wayland clipboard changed — claimed X11 CLIPBOARD selection");
}

static void xwm_on_wayland_primary_change(void *data) {
  struct xwm *xwm = data;
  if (!xwm || !xwm->conn || !xwm->screen)
    return;
  if (!bridge_primary_has_text())
    return;

  xcb_set_selection_owner(xwm->conn, xwm->wm_window, xwm->atoms.primary,
                          XCB_CURRENT_TIME);
  xwm->primary_owner = true;
  xcb_flush(xwm->conn);
  LOG_DEBUG(
      "XWM: Wayland primary selection changed — claimed X11 PRIMARY selection");
}

static void handle_focus_in(struct xwm *xwm, xcb_focus_in_event_t *event) {
  LOG_DEBUG("XWM: FocusIn window=%u detail=%d mode=%d seq=%u focus_window=%u "
            "pending=%u",
            event->event, event->detail, event->mode, event->sequence,
            xwm->focus_window ? xwm->focus_window->id : 0,
            xwm->pending_focus_window ? xwm->pending_focus_window->id : 0);

  if (event->detail == XCB_NOTIFY_DETAIL_POINTER) {
    LOG_DEBUG("XWM: FocusIn filtered: detail=Pointer");
    return;
  }

  if (event->mode == XCB_NOTIFY_MODE_GRAB ||
      event->mode == XCB_NOTIFY_MODE_UNGRAB) {
    LOG_DEBUG("XWM: FocusIn filtered: mode=Grab/Ungrab");
    return;
  }

  struct xwm_window *window = xwm_get_window(xwm, event->event);

  if (window && xwm->pending_focus_window &&
      window == xwm->pending_focus_window) {
    LOG_DEBUG("XWM: FocusIn confirmed pending focus for window=%u",
              event->event);
    xwm->focus_window = window;
    xwm->pending_focus_window = NULL;
    return;
  }

  if (window == xwm->focus_window) {
    LOG_DEBUG("XWM: FocusIn matches compositor focus window=%u", event->event);
    return;
  }

  if (xwm->focus_window) {
    LOG_DEBUG("XWM: Ignoring unsolicited client focus-steal to %u (compositor "
              "focus=%u)",
              event->event, xwm->focus_window->id);
  } else if (xwm->pending_focus_window) {

    LOG_DEBUG(
        "XWM: FocusIn for %u ignored, waiting for pending focus on window=%u",
        event->event, xwm->pending_focus_window->id);
  } else {
    LOG_DEBUG("XWM: FocusIn mismatch but no compositor focus set");
  }
}

static void xwm_handle_xcb_event(struct xwm *xwm, xcb_generic_event_t *event) {
  uint8_t event_type = XWM_EVENT_TYPE(event);
  LOG_DEBUG("XWM: received event type %d", event_type);

  switch (event_type) {
  case XCB_CREATE_NOTIFY:
    handle_create_notify(xwm, (xcb_create_notify_event_t *)event);
    break;
  case XCB_DESTROY_NOTIFY:
    handle_destroy_notify(xwm, (xcb_destroy_notify_event_t *)event);
    break;
  case XCB_MAP_REQUEST:
    handle_map_request(xwm, (xcb_map_request_event_t *)event);
    break;
  case XCB_MAP_NOTIFY:
    handle_map_notify(xwm, (xcb_map_notify_event_t *)event);
    break;
  case XCB_UNMAP_NOTIFY:
    handle_unmap_notify(xwm, (xcb_unmap_notify_event_t *)event);
    break;
  case XCB_CONFIGURE_REQUEST:
    handle_configure_request(xwm, (xcb_configure_request_event_t *)event);
    break;
  case XCB_CONFIGURE_NOTIFY:
    handle_configure_notify(xwm, (xcb_configure_notify_event_t *)event);
    break;
  case XCB_PROPERTY_NOTIFY:
    handle_property_notify(xwm, (xcb_property_notify_event_t *)event);
    break;
  case XCB_CLIENT_MESSAGE:
    handle_client_message(xwm, (xcb_client_message_event_t *)event);
    break;
  case XCB_MAPPING_NOTIFY:
    break;
  case XCB_FOCUS_IN:
    handle_focus_in(xwm, (xcb_focus_in_event_t *)event);
    break;
  case XCB_FOCUS_OUT: {
    xcb_focus_out_event_t *fo = (xcb_focus_out_event_t *)event;
    LOG_DEBUG(
        "XWM: FocusOut window=%u detail=%d mode=%d seq=%u focus_window=%u",
        fo->event, fo->detail, fo->mode, fo->sequence,
        xwm->focus_window ? xwm->focus_window->id : 0);
    break;
  }
  case XCB_SELECTION_REQUEST:
    handle_selection_request(xwm, (xcb_selection_request_event_t *)event);
    break;
  case XCB_SELECTION_NOTIFY:
    handle_selection_notify(xwm, (xcb_selection_notify_event_t *)event);
    break;
  default:

    if (xwm->xfixes &&
        event_type == xwm->xfixes->first_event + XCB_XFIXES_SELECTION_NOTIFY) {
      handle_xfixes_selection_notify(
          xwm, (xcb_xfixes_selection_notify_event_t *)event);
    } else {
      LOG_DEBUG("XWM: unhandled event type %d", event_type);
    }
    break;
  }
}

static int xwm_handle_event(int fd, uint32_t mask, void *data) {
  (void)fd;

  struct xwm *xwm = data;
  xcb_generic_event_t *event;
  int count = 0;

  if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
    LOG_ERROR("XWM: xcb connection hangup/error (mask=0x%x)", mask);
    wl_event_source_remove(xwm->source);
    xwm->source = NULL;
    return 0;
  }

  while ((event = xcb_poll_for_event(xwm->conn)) != NULL) {
    xwm_handle_xcb_event(xwm, event);

    free(event);
    count++;
  }

  if (count > 0) {
    LOG_DEBUG("XWM: processed %d events", count);
    xcb_flush(xwm->conn);
  }

  return count;
}

static void
xwm_try_pair_surface_with_window(struct xwm *xwm,
                                 struct xwm_xwayland_surface *xsurf);

static void xwm_xwayland_surface_destroy(struct wl_resource *resource) {
  struct xwm_xwayland_surface *xsurf = wl_resource_get_user_data(resource);
  if (!xsurf)
    return;

  if (xsurf->surface_commit_listener.link.prev)
    wl_list_remove(&xsurf->surface_commit_listener.link);
  if (xsurf->surface_destroy_listener.link.prev)
    wl_list_remove(&xsurf->surface_destroy_listener.link);
  if (xsurf->link.prev)
    wl_list_remove(&xsurf->link);
  free(xsurf);
}

static void xwm_xwayland_surface_committed(struct wl_listener *listener,
                                           void *data) {
  (void)data;
  struct xwm_xwayland_surface *xsurf =
      wl_container_of(listener, xsurf, surface_commit_listener);

  if (xsurf->serial == 0)
    return;

  LOG_DEBUG("XWM: xwayland_surface committed with serial %lu",
            (unsigned long)xsurf->serial);

  wl_list_remove(&xsurf->surface_commit_listener.link);
  wl_list_init(&xsurf->surface_commit_listener.link);

  xwm_try_pair_surface_with_window(xsurf->xwm, xsurf);
}

static void xwm_xwayland_surface_destroyed(struct wl_listener *listener,
                                           void *data) {
  (void)data;
  struct xwm_xwayland_surface *xsurf =
      wl_container_of(listener, xsurf, surface_destroy_listener);
  wl_list_remove(&xsurf->surface_commit_listener.link);
  wl_list_init(&xsurf->surface_commit_listener.link);
  wl_list_remove(&xsurf->surface_destroy_listener.link);
  wl_list_init(&xsurf->surface_destroy_listener.link);
  wl_list_remove(&xsurf->link);
  wl_list_init(&xsurf->link);
  xsurf->surface = NULL;

  xsurf->resource = NULL;
}

static void xwl_surface_set_serial(struct wl_client *client,
                                   struct wl_resource *resource,
                                   uint32_t serial_lo, uint32_t serial_hi) {
  (void)client;
  struct xwm_xwayland_surface *xsurf = wl_resource_get_user_data(resource);
  uint64_t serial = u64_from_u32s(serial_hi, serial_lo);

  if (serial == 0) {
    wl_resource_post_error(resource, XWAYLAND_SURFACE_V1_ERROR_INVALID_SERIAL,
                           "Invalid serial for xwayland surface");
    return;
  }

  if (xsurf->serial != 0) {
    wl_resource_post_error(resource,
                           XWAYLAND_SURFACE_V1_ERROR_ALREADY_ASSOCIATED,
                           "Surface already has a serial");
    return;
  }

  xsurf->serial = serial;
  LOG_INFO("XWM: xwayland_surface set serial=%lu", (unsigned long)serial);

  xwm_try_pair_surface_with_window(xsurf->xwm, xsurf);
}

static void xwl_surface_destroy(struct wl_client *client,
                                struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct xwayland_surface_v1_interface xwl_surface_interface = {
    .set_serial = xwl_surface_set_serial,
    .destroy = xwl_surface_destroy,
};

static void
xwl_shell_get_xwayland_surface(struct wl_client *client,
                               struct wl_resource *resource, uint32_t id,
                               struct wl_resource *surface_resource) {
  struct xwm *xwm = wl_resource_get_user_data(resource);
  struct bridge_surface *surf = wl_resource_get_user_data(surface_resource);

  if (!surf) {
    wl_resource_post_error(resource, XWAYLAND_SHELL_V1_ERROR_ROLE,
                           "wl_surface does not have a valid bridge_surface");
    return;
  }

  struct xwm_xwayland_surface *xsurf = calloc(1, sizeof(*xsurf));
  if (!xsurf) {
    wl_client_post_no_memory(client);
    return;
  }

  xsurf->resource =
      wl_resource_create(client, &xwayland_surface_v1_interface, 1, id);
  if (!xsurf->resource) {
    free(xsurf);
    wl_client_post_no_memory(client);
    return;
  }

  xsurf->xwm = xwm;
  xsurf->surface = surf;
  xsurf->serial = 0;
  wl_list_init(&xsurf->link);

  LOG_INFO("XWM: xwayland_surface created: bridge_surface=%p "
           "wl_surface_resource=%p (id=%u)",
           (void *)surf, (void *)surf->resource,
           wl_resource_get_id(surf->resource));

  wl_resource_set_implementation(xsurf->resource, &xwl_surface_interface, xsurf,
                                 xwm_xwayland_surface_destroy);

  xsurf->surface_commit_listener.notify = xwm_xwayland_surface_committed;

  wl_list_init(&xsurf->surface_commit_listener.link);
  wl_signal_add(&surf->commit_signal, &xsurf->surface_commit_listener);

  xsurf->surface_destroy_listener.notify = xwm_xwayland_surface_destroyed;
  wl_list_init(&xsurf->surface_destroy_listener.link);
  wl_resource_add_destroy_listener(surf->resource,
                                   &xsurf->surface_destroy_listener);

  LOG_INFO("XWM: created xwayland_surface for wl_surface %u",
           wl_resource_get_id(surface_resource));

  wl_list_insert(&xwm->unpaired_surface_list, &xsurf->link);
}

static void xwl_shell_destroy(struct wl_client *client,
                              struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct xwayland_shell_v1_interface xwayland_shell_implementation =
    {
        .get_xwayland_surface = xwl_shell_get_xwayland_surface,
        .destroy = xwl_shell_destroy,
};

static void bind_xwayland_shell(struct wl_client *client, void *data,
                                uint32_t version, uint32_t id) {
  struct xwm *xwm = data;

  struct wl_client *xwayland_client = bridge_get_xwayland_client();
  if (client != xwayland_client) {
    LOG_WARN("XWM: non-Xwayland client tried to bind xwayland_shell_v1");

    struct wl_resource *resource =
        wl_resource_create(client, &xwayland_shell_v1_interface, version, id);
    wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                           "permission to bind xwayland_shell denied");
    return;
  }

  struct wl_resource *resource =
      wl_resource_create(client, &xwayland_shell_v1_interface, version, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  xwm->shell_bound = true;
  wl_resource_set_implementation(resource, &xwayland_shell_implementation, xwm,
                                 NULL);

  LOG_INFO("XWM: Xwayland bound xwayland_shell_v1 - using new protocol for "
           "window pairing");
}

static void xwm_try_pair_window_with_surface(struct xwm *xwm,
                                             struct xwm_window *window) {
  if (window->surface_serial == 0 || window->surface)
    return;

  struct xwm_xwayland_surface *xsurf, *tmp;
  wl_list_for_each_safe(xsurf, tmp, &xwm->unpaired_surface_list, link) {
    if (xsurf->serial == window->surface_serial) {
      if (!xwm_bind_surface_window(window, xsurf->surface,
                                   "SERIAL_PAIR_WIN_TO_SURF"))
        continue;

      wl_list_remove(&xsurf->link);
      wl_list_init(&xsurf->link);
      wl_list_remove(&window->link);
      wl_list_init(&window->link);

      LOG_INFO("XWM: paired window %u '%s' with bridge_surface=%p (serial=%lu, "
               "resource=%p id=%u)",
               window->id, window->name ? window->name : "",
               (void *)window->surface, (unsigned long)window->surface_serial,
               (void *)window->surface->resource,
               wl_resource_get_id(window->surface->resource));

      LOG_TRACE("XWM: set surface->xwm_window=%p for window=%u", (void *)window,
                window->id);

      if (window->surface && !window->surface->plexy_window) {

        if (window->properties_dirty)
          xwm_window_read_properties(window);
        xwm_window_ensure_real_geometry(window);
        uint32_t plexy_type = xwm_window_get_plexy_type(window);
        if (xwm_window_is_popup(window)) {

          if (window->width < 4 || window->height < 4) {
            LOG_INFO(
                "XWM: deferring popup creation for window %u (%dx%d too small)",
                window->id, window->width, window->height);
          } else {
            PlexyWindow *parent = xwm_window_resolve_and_cache_parent(window);
            xwm_clamp_popup_pos(window);
            int32_t rel_x, rel_y;
            xwm_popup_rel_coords(window, &rel_x, &rel_y);
            LOG_INFO("XWM: window %u is popup (type=%u override_redirect=%d "
                     "plexy_type=%u), parent=%p abs=(%d,%d) rel=(%d,%d)",
                     window->id, window->type, window->override_redirect,
                     plexy_type, (void *)parent, window->x, window->y, rel_x,
                     rel_y);
            bridge_create_x11_popup(window->surface, window->name, parent,
                                    rel_x, rel_y, window->width, window->height,
                                    plexy_type);
            xwm_reactivate_popup_parent(xwm, window);
          }
        } else {
          LOG_INFO("XWM: window %u is toplevel (type=%u plexy_type=%u)",
                   window->id, window->type, plexy_type);
          bridge_create_x11_window(window->surface, window->name, window->x,
                                   window->y, window->width, window->height,
                                   plexy_type);
        }
      }
      return;
    }
  }

  LOG_DEBUG("XWM: window %u waiting for surface with serial=%lu", window->id,
            (unsigned long)window->surface_serial);
  wl_list_remove(&window->link);
  wl_list_insert(&xwm->unpaired_list, &window->link);
}

static void
xwm_try_pair_surface_with_window(struct xwm *xwm,
                                 struct xwm_xwayland_surface *xsurf) {
  if (xsurf->serial == 0)
    return;

  struct xwm_window *window, *tmp;
  wl_list_for_each_safe(window, tmp, &xwm->unpaired_list, link) {
    if (window->surface_serial == xsurf->serial) {
      if (!xwm_bind_surface_window(window, xsurf->surface,
                                   "SERIAL_PAIR_SURF_TO_WIN"))
        continue;

      wl_list_remove(&xsurf->link);
      wl_list_init(&xsurf->link);
      wl_list_remove(&window->link);
      wl_list_init(&window->link);

      LOG_INFO("XWM: paired window %u '%s' with surface (serial=%lu, deferred)",
               window->id, window->name ? window->name : "",
               (unsigned long)xsurf->serial);

      LOG_TRACE("XWM: DEFERRED set surface->xwm_window=%p for window=%u",
                (void *)window, window->id);

      if (window->surface && !window->surface->plexy_window) {
        if (window->properties_dirty)
          xwm_window_read_properties(window);
        xwm_window_ensure_real_geometry(window);
        uint32_t plexy_type = xwm_window_get_plexy_type(window);
        if (xwm_window_is_popup(window)) {
          if (window->width < 4 || window->height < 4) {
            LOG_INFO(
                "XWM: deferring deferred popup for window %u (%dx%d too small)",
                window->id, window->width, window->height);
          } else {
            PlexyWindow *parent = xwm_window_resolve_and_cache_parent(window);
            xwm_clamp_popup_pos(window);
            int32_t rel_x, rel_y;
            xwm_popup_rel_coords(window, &rel_x, &rel_y);
            LOG_INFO("XWM: deferred window %u is popup (type=%u "
                     "override_redirect=%d plexy_type=%u), parent=%p "
                     "abs=(%d,%d) rel=(%d,%d)",
                     window->id, window->type, window->override_redirect,
                     plexy_type, (void *)parent, window->x, window->y, rel_x,
                     rel_y);
            bridge_create_x11_popup(window->surface, window->name, parent,
                                    rel_x, rel_y, window->width, window->height,
                                    plexy_type);
            xwm_reactivate_popup_parent(xwm, window);
          }
        } else {
          LOG_INFO(
              "XWM: deferred window %u is toplevel (type=%u plexy_type=%u)",
              window->id, window->type, plexy_type);
          bridge_create_x11_window(window->surface, window->name, window->x,
                                   window->y, window->width, window->height,
                                   plexy_type);
        }
      }
      return;
    }
  }

  LOG_DEBUG("XWM: surface with serial=%lu waiting for X11 window",
            (unsigned long)xsurf->serial);
}

static void xwm_create_wm_window(struct xwm *xwm) {
  xwm->wm_window = xcb_generate_id(xwm->conn);
  xcb_create_window(xwm->conn, XCB_COPY_FROM_PARENT, xwm->wm_window,
                    xwm->screen->root, 0, 0, 10, 10, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, xwm->screen->root_visual, 0,
                    NULL);
  uint32_t wm_values[1] = {XCB_EVENT_MASK_PROPERTY_CHANGE};
  xcb_change_window_attributes(xwm->conn, xwm->wm_window, XCB_CW_EVENT_MASK,
                               wm_values);

  static const char wm_name[] = "PlexyShell";

  xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, xwm->wm_window,
                      xwm->atoms.net_supporting_wm_check, XCB_ATOM_WINDOW, 32,
                      1, &xwm->wm_window);

  xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, xwm->wm_window,
                      xwm->atoms.net_wm_name, xwm->atoms.utf8_string, 8,
                      strlen(wm_name), wm_name);

  xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, xwm->screen->root,
                      xwm->atoms.net_supporting_wm_check, XCB_ATOM_WINDOW, 32,
                      1, &xwm->wm_window);

  xcb_set_selection_owner(xwm->conn, xwm->wm_window, xwm->atoms.wm_s0,
                          XCB_TIME_CURRENT_TIME);

  xcb_set_selection_owner(xwm->conn, xwm->wm_window, xwm->atoms.net_wm_cm_s0,
                          XCB_TIME_CURRENT_TIME);

  xwm->no_focus_window = xcb_generate_id(xwm->conn);
  uint32_t nfw_vals[2] = {1, XCB_EVENT_MASK_KEY_PRESS |
                                 XCB_EVENT_MASK_KEY_RELEASE};
  xcb_create_window(xwm->conn, XCB_COPY_FROM_PARENT, xwm->no_focus_window,
                    xwm->screen->root, -100, -100, 1, 1, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, xwm->screen->root_visual,
                    XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK, nfw_vals);
  xcb_map_window(xwm->conn, xwm->no_focus_window);
}

static bool xwm_finish_init(struct xwm *xwm) {
  LOG_DEBUG("XWM: xwm_finish_init called, conn=%p", (void *)xwm->conn);

  const xcb_setup_t *setup = xcb_get_setup(xwm->conn);
  if (!setup) {
    LOG_ERROR("XWM: xcb_get_setup returned NULL");
    return false;
  }

  xcb_screen_iterator_t s = xcb_setup_roots_iterator(setup);
  if (!s.data) {
    LOG_ERROR("XWM: no screens available");
    return false;
  }
  xwm->screen = s.data;

  LOG_INFO("XWM: connected to X display :%d (root=%u)", xwm->display_number,
           xwm->screen->root);
  xwm->state = XWM_CONNECTED;

  xcb_prefetch_extension_data(xwm->conn, &xcb_xfixes_id);
  xcb_prefetch_extension_data(xwm->conn, &xcb_composite_id);
  xwm_init_atoms(xwm);

  {
    xcb_query_extension_cookie_t xtest_cookie =
        xcb_query_extension(xwm->conn, 5, "XTEST");
    xcb_query_extension_reply_t *xtest_reply =
        xcb_query_extension_reply(xwm->conn, xtest_cookie, NULL);
    xwm->xtest_present = xtest_reply && xtest_reply->present;
    free(xtest_reply);
    if (!xwm->xtest_present)
      LOG_WARN("XWM: XTEST extension not available — button routing will "
               "bypass grabs");
  }

  xwm->xfixes = xcb_get_extension_data(xwm->conn, &xcb_xfixes_id);
  if (!xwm->xfixes || !xwm->xfixes->present) {
    LOG_WARN("XWM: xfixes extension not available");
  } else {
    xcb_xfixes_query_version_reply_t *xfixes_reply =
        xcb_xfixes_query_version_reply(
            xwm->conn, xcb_xfixes_query_version(xwm->conn, 5, 0), NULL);
    if (xfixes_reply) {
      LOG_INFO("XWM: XFixes v%u.%u available (event_base=%u), subscribing to "
               "CLIPBOARD+PRIMARY",
               xfixes_reply->major_version, xfixes_reply->minor_version,
               xwm->xfixes->first_event);
      free(xfixes_reply);
    } else {
      LOG_WARN("XWM: XFixes query_version failed");
    }
    xcb_xfixes_select_selection_input(
        xwm->conn, xwm->screen->root, xwm->atoms.clipboard,
        XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY);
    xcb_xfixes_select_selection_input(
        xwm->conn, xwm->screen->root, xwm->atoms.primary,
        XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY);
  }

  bridge_clipboard_register_change_listener(xwm_on_wayland_clipboard_change,
                                            xwm);

  bridge_primary_register_change_listener(xwm_on_wayland_primary_change, xwm);

  uint32_t values[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                        XCB_EVENT_MASK_PROPERTY_CHANGE};

  xcb_void_cookie_t attr_cookie = xcb_change_window_attributes_checked(
      xwm->conn, xwm->screen->root, XCB_CW_EVENT_MASK, values);
  xcb_generic_error_t *attr_err = xcb_request_check(xwm->conn, attr_cookie);
  if (attr_err) {
    LOG_ERROR("XWM: SubstructureRedirect failed (error %d) - another WM?",
              attr_err->error_code);
    free(attr_err);
    return false;
  }
  LOG_INFO("XWM: SubstructureRedirect claimed on root");

  xcb_composite_redirect_subwindows(xwm->conn, xwm->screen->root,
                                    XCB_COMPOSITE_REDIRECT_MANUAL);

  xcb_atom_t supported[] = {
      xwm->atoms.net_wm_moveresize,
      xwm->atoms.net_wm_state,
      xwm->atoms.net_wm_state_fullscreen,
      xwm->atoms.net_wm_state_maximized_vert,
      xwm->atoms.net_wm_state_maximized_horz,
      xwm->atoms.net_wm_state_above,
      xwm->atoms.net_wm_state_below,
      xwm->atoms.net_wm_state_modal,
      xwm->atoms.net_wm_state_hidden,
      xwm->atoms.net_active_window,
      xwm->atoms.net_frame_extents,
      xwm->atoms.net_client_list,
      xwm->atoms.net_wm_name,
      xwm->atoms.net_wm_pid,
      xwm->atoms.net_wm_ping,
      xwm->atoms.net_wm_window_type,
      xwm->atoms.net_wm_desktop,
      xwm->atoms.net_wm_icon_name,
      xwm->atoms.net_wm_user_time,
  };

  xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, xwm->screen->root,
                      xwm->atoms.net_supported, XCB_ATOM_ATOM, 32,
                      sizeof(supported) / sizeof(supported[0]), supported);

  xcb_window_t none = XCB_WINDOW_NONE;
  xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, xwm->screen->root,
                      xwm->atoms.net_active_window, xwm->atoms.window, 32, 1,
                      &none);

  xwm_create_wm_window(xwm);

  if (!xwm_init_randr(xwm)) {
    LOG_WARN("XWM: RandR not available - games may not detect display");
  }

  xcb_flush(xwm->conn);

  if (xcb_connection_has_error(xwm->conn)) {
    LOG_ERROR("XWM: connection died during init (after flush)");
    return false;
  }

  xwm->loop = wl_display_get_event_loop(xwm->bridge->display);
  int xcb_fd = xcb_get_file_descriptor(xwm->conn);
  xwm->source = wl_event_loop_add_fd(xwm->loop, xcb_fd, WL_EVENT_READABLE,
                                     xwm_handle_event, xwm);
  wl_event_source_check(xwm->source);

  LOG_INFO("XWM: initialized successfully");
  xwm->state = XWM_READY;

  return true;
}

static int xwm_connection_timer(void *data) {
  struct xwm *xwm = data;

  LOG_DEBUG("XWM: timer callback fired (state=%d, attempts=%d)", xwm->state,
            xwm->connection_attempts);

  xwm->connection_attempts++;

  int wm_fd = bridge_get_xwayland_wm_fd();
  if (wm_fd < 0) {
    if (xwm->connection_attempts >= 20) {
      LOG_ERROR("XWM: no WM fd available after %d attempts - giving up",
                xwm->connection_attempts);
      xwm->state = XWM_FAILED;
      if (xwm->retry_timer) {
        wl_event_source_remove(xwm->retry_timer);
        xwm->retry_timer = NULL;
      }
      return 0;
    }
    int delay_ms = 500 * (1 + (xwm->connection_attempts / 5));
    LOG_INFO("XWM: WM fd not ready, retrying in %dms", delay_ms);
    wl_event_source_timer_update(xwm->retry_timer, delay_ms);
    return 0;
  }

  LOG_INFO("XWM: connecting via -wm fd %d (attempt %d)", wm_fd,
           xwm->connection_attempts);

  xwm->conn = xcb_connect_to_fd(wm_fd, NULL);

  int conn_err = xcb_connection_has_error(xwm->conn);

  if (conn_err) {
    LOG_ERROR("XWM: xcb_connect_to_fd failed (error %d)", conn_err);
    xcb_disconnect(xwm->conn);
    xwm->conn = NULL;

    if (xwm->connection_attempts >= 20) {
      LOG_ERROR("XWM: failed to connect after %d attempts - giving up",
                xwm->connection_attempts);
      xwm->state = XWM_FAILED;
      if (xwm->retry_timer) {
        wl_event_source_remove(xwm->retry_timer);
        xwm->retry_timer = NULL;
      }
      return 0;
    }

    int delay_ms = 500 * (1 + (xwm->connection_attempts / 5));
    LOG_INFO("XWM: connection failed (error %d), retrying in %dms", conn_err,
             delay_ms);
    wl_event_source_timer_update(xwm->retry_timer, delay_ms);
    return 0;
  }

  LOG_INFO("XWM: connection established after %d attempt(s)",
           xwm->connection_attempts);

  if (xwm->retry_timer) {
    wl_event_source_remove(xwm->retry_timer);
    xwm->retry_timer = NULL;
  }

  if (!xwm_finish_init(xwm)) {
    LOG_ERROR("XWM: initialization failed after successful connection");
    xwm->state = XWM_FAILED;
    if (xwm->conn) {
      xcb_disconnect(xwm->conn);
      xwm->conn = NULL;
    }
    return 0;
  }

  return 0;
}

bool xwm_is_ready(struct xwm *xwm) { return xwm && xwm->state == XWM_READY; }

struct xwm *xwm_create_async(struct wayland_bridge *bridge,
                             int display_number) {
  struct xwm *xwm = calloc(1, sizeof(*xwm));
  if (!xwm)
    return NULL;

  xwm->bridge = bridge;
  xwm->display_number = display_number;
  xwm->state = XWM_CONNECTING;
  xwm->connection_attempts = 0;
  wl_list_init(&xwm->window_list);
  wl_list_init(&xwm->unpaired_list);
  wl_list_init(&xwm->unpaired_surface_list);

  xwm->window_hash = xwm_hash_table_create();
  if (!xwm->window_hash) {
    free(xwm);
    return NULL;
  }

  xwm->loop = wl_display_get_event_loop(bridge->display);
  xwm->retry_timer =
      wl_event_loop_add_timer(xwm->loop, xwm_connection_timer, xwm);
  if (!xwm->retry_timer) {
    LOG_ERROR("XWM: failed to create retry timer");
    xwm_hash_table_destroy(xwm->window_hash);
    free(xwm);
    return NULL;
  }

  wl_event_source_timer_update(xwm->retry_timer, 500);

  xwm->xwayland_shell_global =
      wl_global_create(bridge->display, &xwayland_shell_v1_interface, 1, xwm,
                       bind_xwayland_shell);
  if (!xwm->xwayland_shell_global) {
    LOG_WARN("XWM: failed to create xwayland_shell_v1 global (early)");
  } else {
    LOG_INFO("XWM: registered xwayland_shell_v1 global (early, before XWayland "
             "connects)");
  }

  LOG_INFO("XWM: async initialization started for display :%d", display_number);

  return xwm;
}

struct xwm *xwm_create(struct wayland_bridge *bridge, int display_number) {
  struct xwm *xwm = calloc(1, sizeof(*xwm));
  if (!xwm)
    return NULL;

  xwm->bridge = bridge;
  xwm->display_number = display_number;
  wl_list_init(&xwm->window_list);
  wl_list_init(&xwm->unpaired_list);
  wl_list_init(&xwm->unpaired_surface_list);

  xwm->window_hash = xwm_hash_table_create();
  if (!xwm->window_hash) {
    free(xwm);
    return NULL;
  }

  int wm_fd = bridge_get_xwayland_wm_fd();
  if (wm_fd < 0) {
    LOG_ERROR("XWM: no WM fd available");
    xwm_hash_table_destroy(xwm->window_hash);
    free(xwm);
    return NULL;
  }

  xwm->conn = xcb_connect_to_fd(wm_fd, NULL);
  if (xcb_connection_has_error(xwm->conn)) {
    LOG_ERROR("XWM: failed to connect via WM fd %d", wm_fd);
    xcb_disconnect(xwm->conn);
    xwm_hash_table_destroy(xwm->window_hash);
    free(xwm);
    return NULL;
  }

  xcb_screen_iterator_t s = xcb_setup_roots_iterator(xcb_get_setup(xwm->conn));
  xwm->screen = s.data;

  LOG_INFO("XWM: connected via WM fd (root=%u)", xwm->screen->root);

  xcb_prefetch_extension_data(xwm->conn, &xcb_xfixes_id);
  xcb_prefetch_extension_data(xwm->conn, &xcb_composite_id);

  xwm_init_atoms(xwm);

  xwm->xfixes = xcb_get_extension_data(xwm->conn, &xcb_xfixes_id);
  if (!xwm->xfixes || !xwm->xfixes->present) {
    LOG_WARN("XWM: xfixes extension not available");
  } else {
    xcb_xfixes_query_version_reply_t *xfixes_reply =
        xcb_xfixes_query_version_reply(
            xwm->conn, xcb_xfixes_query_version(xwm->conn, 5, 0), NULL);
    if (xfixes_reply) {
      LOG_INFO("XWM: XFixes v%u.%u available (event_base=%u), subscribing to "
               "CLIPBOARD+PRIMARY",
               xfixes_reply->major_version, xfixes_reply->minor_version,
               xwm->xfixes->first_event);
      free(xfixes_reply);
    } else {
      LOG_WARN("XWM: XFixes query_version failed");
    }
    xcb_xfixes_select_selection_input(
        xwm->conn, xwm->screen->root, xwm->atoms.clipboard,
        XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY);
    xcb_xfixes_select_selection_input(
        xwm->conn, xwm->screen->root, xwm->atoms.primary,
        XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY);
  }
  bridge_clipboard_register_change_listener(xwm_on_wayland_clipboard_change,
                                            xwm);
  bridge_primary_register_change_listener(xwm_on_wayland_primary_change, xwm);

  uint32_t values[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                        XCB_EVENT_MASK_PROPERTY_CHANGE};

  xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(
      xwm->conn, xwm->screen->root, XCB_CW_EVENT_MASK, values);
  xcb_generic_error_t *error = xcb_request_check(xwm->conn, cookie);
  if (error) {
    LOG_ERROR("XWM: failed to set root window event mask (error %d) - another "
              "WM running?",
              error->error_code);
    free(error);
    xcb_disconnect(xwm->conn);
    xwm_hash_table_destroy(xwm->window_hash);
    free(xwm);
    return NULL;
  }

  xcb_composite_redirect_subwindows(xwm->conn, xwm->screen->root,
                                    XCB_COMPOSITE_REDIRECT_MANUAL);

  xcb_atom_t supported[] = {
      xwm->atoms.net_wm_moveresize,
      xwm->atoms.net_wm_state,
      xwm->atoms.net_wm_state_fullscreen,
      xwm->atoms.net_wm_state_maximized_vert,
      xwm->atoms.net_wm_state_maximized_horz,
      xwm->atoms.net_wm_state_above,
      xwm->atoms.net_wm_state_below,
      xwm->atoms.net_wm_state_modal,
      xwm->atoms.net_wm_state_hidden,
      xwm->atoms.net_active_window,
      xwm->atoms.net_frame_extents,
      xwm->atoms.net_client_list,
      xwm->atoms.net_wm_name,
      xwm->atoms.net_wm_pid,
      xwm->atoms.net_wm_ping,
      xwm->atoms.net_wm_window_type,
      xwm->atoms.net_wm_desktop,
      xwm->atoms.net_wm_icon_name,
      xwm->atoms.net_wm_user_time,
  };

  xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, xwm->screen->root,
                      xwm->atoms.net_supported, XCB_ATOM_ATOM, 32,
                      sizeof(supported) / sizeof(supported[0]), supported);

  xcb_window_t none = XCB_WINDOW_NONE;
  xcb_change_property(xwm->conn, XCB_PROP_MODE_REPLACE, xwm->screen->root,
                      xwm->atoms.net_active_window, xwm->atoms.window, 32, 1,
                      &none);

  xwm_create_wm_window(xwm);

  if (!xwm_init_randr(xwm)) {
    LOG_WARN("XWM: RandR not available - games may not detect display");
  }

  xcb_flush(xwm->conn);

  xwm->loop = wl_display_get_event_loop(bridge->display);
  int xcb_fd = xcb_get_file_descriptor(xwm->conn);
  xwm->source = wl_event_loop_add_fd(xwm->loop, xcb_fd, WL_EVENT_READABLE,
                                     xwm_handle_event, xwm);
  wl_event_source_check(xwm->source);

  LOG_INFO("XWM: initialized successfully");

  return xwm;
}

void xwm_destroy(struct xwm *xwm) {
  if (!xwm)
    return;

  LOG_INFO("XWM: shutting down (state=%d)", xwm->state);

  if (xwm->retry_timer) {
    wl_event_source_remove(xwm->retry_timer);
    xwm->retry_timer = NULL;
  }

  if (xwm->source)
    wl_event_source_remove(xwm->source);

  if (xwm->xwayland_shell_global)
    wl_global_destroy(xwm->xwayland_shell_global);

  struct xwm_window *window, *tmp;
  wl_list_for_each_safe(window, tmp, &xwm->window_list, link) {
    xwm_window_destroy(window);
  }
  wl_list_for_each_safe(window, tmp, &xwm->unpaired_list, link) {
    xwm_window_destroy(window);
  }

  struct xwm_xwayland_surface *xsurf, *xtmp;
  wl_list_for_each_safe(xsurf, xtmp, &xwm->unpaired_surface_list, link) {
    wl_list_remove(&xsurf->link);
    wl_list_init(&xsurf->link);

    if (xsurf->resource)
      wl_resource_destroy(xsurf->resource);
    else
      free(xsurf);
  }

  if (xwm->window_hash) {
    xwm_hash_table_destroy(xwm->window_hash);
    xwm->window_hash = NULL;
  }

  if (xwm->conn)
    xcb_disconnect(xwm->conn);

  free(xwm);
}

void xwm_send_focus_in_event(struct xwm_window *window) {
  if (!window || !window->xwm || !window->xwm->conn)
    return;

  if (!window->mapped)
    return;

  xcb_focus_in_event_t event = {.response_type = XCB_FOCUS_IN,
                                .detail = XCB_NOTIFY_DETAIL_NONLINEAR,
                                .event = window->id,
                                .mode = XCB_NOTIFY_MODE_NORMAL};

  xcb_send_event(window->xwm->conn, 0, window->id, XCB_EVENT_MASK_FOCUS_CHANGE,
                 (char *)&event);
  xcb_flush(window->xwm->conn);

  LOG_TRACE("XWM: Sent X11 FocusIn to window=%u", window->id);
}

void xwm_send_focus_out_event(struct xwm_window *window) {
  if (!window || !window->xwm || !window->xwm->conn)
    return;

  if (!window->mapped)
    return;

  xcb_focus_out_event_t event = {.response_type = XCB_FOCUS_OUT,
                                 .detail = XCB_NOTIFY_DETAIL_NONLINEAR,
                                 .event = window->id,
                                 .mode = XCB_NOTIFY_MODE_NORMAL};

  xcb_send_event(window->xwm->conn, 0, window->id, XCB_EVENT_MASK_FOCUS_CHANGE,
                 (char *)&event);
  xcb_flush(window->xwm->conn);

  LOG_TRACE("XWM: Sent X11 FocusOut to window=%u", window->id);
}

static inline int16_t clamp_i16(int32_t v) {
  if (v < -32768)
    return (int16_t)-32768;
  if (v > 32767)
    return (int16_t)32767;
  return (int16_t)v;
}

void xwm_send_enter_event(struct xwm_window *window, int32_t x, int32_t y) {
  if (!window || !window->xwm || !window->xwm->conn)
    return;

  if (!window->mapped)
    return;

  int16_t root_x = clamp_i16(window->x + x);
  int16_t root_y = clamp_i16(window->y + y);

  xcb_enter_notify_event_t event = {.response_type = XCB_ENTER_NOTIFY,
                                    .detail = XCB_NOTIFY_DETAIL_NONLINEAR,
                                    .time = XCB_CURRENT_TIME,
                                    .root = window->xwm->screen->root,
                                    .event = window->id,
                                    .child = XCB_WINDOW_NONE,
                                    .root_x = root_x,
                                    .root_y = root_y,
                                    .event_x = clamp_i16(x),
                                    .event_y = clamp_i16(y),
                                    .state = 0,
                                    .mode = XCB_NOTIFY_MODE_NORMAL,
                                    .same_screen_focus = 1};

  xcb_send_event(window->xwm->conn, 0, window->id, XCB_EVENT_MASK_ENTER_WINDOW,
                 (char *)&event);
  xcb_flush(window->xwm->conn);

  LOG_TRACE("XWM: Sent X11 EnterNotify to window=%u at %d,%d", window->id, x,
            y);
}

void xwm_send_leave_event(struct xwm_window *window, int32_t x, int32_t y) {
  if (!window || !window->xwm || !window->xwm->conn)
    return;

  if (!window->mapped)
    return;

  int16_t root_x = clamp_i16(window->x + x);
  int16_t root_y = clamp_i16(window->y + y);

  xcb_leave_notify_event_t event = {.response_type = XCB_LEAVE_NOTIFY,
                                    .detail = XCB_NOTIFY_DETAIL_NONLINEAR,
                                    .time = XCB_CURRENT_TIME,
                                    .root = window->xwm->screen->root,
                                    .event = window->id,
                                    .child = XCB_WINDOW_NONE,
                                    .root_x = root_x,
                                    .root_y = root_y,
                                    .event_x = clamp_i16(x),
                                    .event_y = clamp_i16(y),
                                    .state = 0,
                                    .mode = XCB_NOTIFY_MODE_NORMAL,
                                    .same_screen_focus = 1};

  xcb_send_event(window->xwm->conn, 0, window->id, XCB_EVENT_MASK_LEAVE_WINDOW,
                 (char *)&event);
  xcb_flush(window->xwm->conn);

  LOG_TRACE("XWM: Sent X11 LeaveNotify to window=%u at %d,%d", window->id, x,
            y);
}

void xwm_send_motion_event(struct xwm_window *window, int32_t x, int32_t y) {
  if (!window || !window->xwm || !window->xwm->conn)
    return;

  if (!window->mapped)
    return;

  int16_t root_x = clamp_i16(window->x + x);
  int16_t root_y = clamp_i16(window->y + y);

  xcb_motion_notify_event_t event = {.response_type = XCB_MOTION_NOTIFY,
                                     .detail = XCB_MOTION_NORMAL,
                                     .time = XCB_CURRENT_TIME,
                                     .root = window->xwm->screen->root,
                                     .event = window->id,
                                     .child = XCB_WINDOW_NONE,
                                     .root_x = root_x,
                                     .root_y = root_y,
                                     .event_x = clamp_i16(x),
                                     .event_y = clamp_i16(y),
                                     .state = window->xwm->button_state,
                                     .same_screen = 1};
  xcb_send_event(window->xwm->conn, 0, window->id,
                 XCB_EVENT_MASK_POINTER_MOTION, (char *)&event);
  xcb_flush(window->xwm->conn);

  LOG_TRACE("XWM: Sent X11 MotionNotify to window=%u at (%d,%d) root=(%d,%d)",
            window->id, x, y, root_x, root_y);
}

void xwm_send_button_event(struct xwm_window *window, uint32_t button,
                           bool pressed, int32_t x, int32_t y) {
  if (!window || !window->xwm || !window->xwm->conn)
    return;

  if (!window->mapped)
    return;

  uint8_t x11_button;
  uint16_t button_mask;
  switch (button) {
  case 0x110:
    x11_button = 1;
    button_mask = XCB_BUTTON_MASK_1;
    break;
  case 0x111:
    x11_button = 3;
    button_mask = XCB_BUTTON_MASK_3;
    break;
  case 0x112:
    x11_button = 2;
    button_mask = XCB_BUTTON_MASK_2;
    break;
  case 0x113:
    x11_button = 8;
    button_mask = 0;
    break;
  case 0x114:
    x11_button = 9;
    button_mask = 0;
    break;
  default:
    x11_button = (uint8_t)(button - 0x110 + 1);
    button_mask = 0;
    break;
  }

  int16_t root_x = clamp_i16(window->x + x);
  int16_t root_y = clamp_i16(window->y + y);

  xcb_button_press_event_t event = {
      .response_type = pressed ? XCB_BUTTON_PRESS : XCB_BUTTON_RELEASE,
      .detail = x11_button,
      .time = XCB_CURRENT_TIME,
      .root = window->xwm->screen->root,
      .event = window->id,
      .child = XCB_WINDOW_NONE,
      .root_x = root_x,
      .root_y = root_y,
      .event_x = clamp_i16(x),
      .event_y = clamp_i16(y),
      .state = window->xwm->button_state,
      .same_screen = 1};
  xcb_send_event(window->xwm->conn, 0, window->id,
                 pressed ? XCB_EVENT_MASK_BUTTON_PRESS
                         : XCB_EVENT_MASK_BUTTON_RELEASE,
                 (char *)&event);

  xcb_flush(window->xwm->conn);

  if (pressed && button_mask)
    window->xwm->button_state |= button_mask;
  else if (!pressed && button_mask)
    window->xwm->button_state &= ~button_mask;

  LOG_TRACE("XWM: Sent X11 Button%s btn=%u (x11=%u) root=%d,%d xtest=%d",
            pressed ? "Press" : "Release", button, x11_button, root_x, root_y,
            window->xwm->xtest_present);
}

void xwm_send_expose_event(struct xwm_window *window) {
  if (!window || !window->xwm || !window->xwm->conn)
    return;

  if (!window->mapped)
    return;

  xcb_expose_event_t event = {.response_type = XCB_EXPOSE,
                              .window = window->id,
                              .x = 0,
                              .y = 0,
                              .width = window->width,
                              .height = window->height,
                              .count = 0};

  xcb_send_event(window->xwm->conn, 0, window->id, XCB_EVENT_MASK_EXPOSURE,
                 (char *)&event);
  xcb_flush(window->xwm->conn);
}

void xwm_send_key_event(struct xwm *xwm, uint32_t keycode, bool pressed,
                        uint32_t modifiers) {
  struct xwm_window *window = xwm ? xwm->focus_window : NULL;
  if (!window || !xwm->conn)
    return;

  if (!window->mapped)
    return;

  uint8_t x11_keycode = keycode + 8;

  uint16_t state = 0;
  if (modifiers & 0x01)
    state |= XCB_MOD_MASK_SHIFT;
  if (modifiers & 0x02)
    state |= XCB_MOD_MASK_CONTROL;
  if (modifiers & 0x04)
    state |= XCB_MOD_MASK_1;
  if (modifiers & 0x08)
    state |= XCB_MOD_MASK_4;

  xcb_key_press_event_t event = {.response_type =
                                     pressed ? XCB_KEY_PRESS : XCB_KEY_RELEASE,
                                 .detail = x11_keycode,
                                 .time = XCB_CURRENT_TIME,
                                 .root = xwm->screen->root,
                                 .event = window->id,
                                 .child = XCB_WINDOW_NONE,
                                 .root_x = 0,
                                 .root_y = 0,
                                 .event_x = 0,
                                 .event_y = 0,
                                 .state = state,
                                 .same_screen = 1};

  xcb_send_event(xwm->conn, 0, window->id,
                 pressed ? XCB_EVENT_MASK_KEY_PRESS
                         : XCB_EVENT_MASK_KEY_RELEASE,
                 (char *)&event);
  xcb_flush(xwm->conn);

  LOG_TRACE("XWM: Sent X11 Key%s to window=%u keycode=%u (x11=%u) state=0x%x",
            pressed ? "Press" : "Release", window->id, keycode, x11_keycode,
            state);
}

static uint8_t ascii_to_keycode(char c, bool *need_shift) {
  *need_shift = false;
  if (c >= 'a' && c <= 'z')
    return (uint8_t)(c - 'a' + KEY_A + 8);
  if (c >= 'A' && c <= 'Z') {
    *need_shift = true;
    return (uint8_t)(c - 'A' + KEY_A + 8);
  }
  if (c >= '1' && c <= '9')
    return (uint8_t)(c - '1' + KEY_1 + 8);
  if (c == '0')
    return KEY_0 + 8;

  switch (c) {
  case ' ':
    return KEY_SPACE + 8;
  case '\n':
    return KEY_ENTER + 8;
  case '\t':
    return KEY_TAB + 8;
  case '-':
    return KEY_MINUS + 8;
  case '=':
    return KEY_EQUAL + 8;
  case '[':
    return KEY_LEFTBRACE + 8;
  case ']':
    return KEY_RIGHTBRACE + 8;
  case '\\':
    return KEY_BACKSLASH + 8;
  case ';':
    return KEY_SEMICOLON + 8;
  case '\'':
    return KEY_APOSTROPHE + 8;
  case '`':
    return KEY_GRAVE + 8;
  case ',':
    return KEY_COMMA + 8;
  case '.':
    return KEY_DOT + 8;
  case '/':
    return KEY_SLASH + 8;
  case '!':
    *need_shift = true;
    return KEY_1 + 8;
  case '@':
    *need_shift = true;
    return KEY_2 + 8;
  case '#':
    *need_shift = true;
    return KEY_3 + 8;
  case '$':
    *need_shift = true;
    return KEY_4 + 8;
  case '%':
    *need_shift = true;
    return KEY_5 + 8;
  case '^':
    *need_shift = true;
    return KEY_6 + 8;
  case '&':
    *need_shift = true;
    return KEY_7 + 8;
  case '*':
    *need_shift = true;
    return KEY_8 + 8;
  case '(':
    *need_shift = true;
    return KEY_9 + 8;
  case ')':
    *need_shift = true;
    return KEY_0 + 8;
  case '_':
    *need_shift = true;
    return KEY_MINUS + 8;
  case '+':
    *need_shift = true;
    return KEY_EQUAL + 8;
  case '{':
    *need_shift = true;
    return KEY_LEFTBRACE + 8;
  case '}':
    *need_shift = true;
    return KEY_RIGHTBRACE + 8;
  case '|':
    *need_shift = true;
    return KEY_BACKSLASH + 8;
  case ':':
    *need_shift = true;
    return KEY_SEMICOLON + 8;
  case '"':
    *need_shift = true;
    return KEY_APOSTROPHE + 8;
  case '~':
    *need_shift = true;
    return KEY_GRAVE + 8;
  case '<':
    *need_shift = true;
    return KEY_COMMA + 8;
  case '>':
    *need_shift = true;
    return KEY_DOT + 8;
  case '?':
    *need_shift = true;
    return KEY_SLASH + 8;
  default:
    return 0;
  }
}

void xwm_inject_keysym(struct xwm *xwm, uint32_t keysym, bool shift) {
  (void)keysym;
  (void)shift;
  (void)xwm;
}

void xwm_inject_text(struct xwm *xwm, const char *text) {
  if (!xwm || !text || !xwm->focus_window)
    return;

  for (const char *p = text; *p; p++) {
    bool need_shift = false;
    uint8_t kc = ascii_to_keycode(*p, &need_shift);
    if (kc == 0)
      continue;

    struct xwm_window *window = xwm->focus_window;
    if (!window || !window->mapped)
      return;

    uint16_t state = need_shift ? XCB_MOD_MASK_SHIFT : 0;
    xcb_key_press_event_t press = {.response_type = XCB_KEY_PRESS,
                                   .detail = kc,
                                   .time = XCB_CURRENT_TIME,
                                   .root = xwm->screen->root,
                                   .event = window->id,
                                   .child = XCB_WINDOW_NONE,
                                   .state = state,
                                   .same_screen = 1};
    xcb_send_event(xwm->conn, 0, window->id, XCB_EVENT_MASK_KEY_PRESS,
                   (char *)&press);

    xcb_key_press_event_t release = press;
    release.response_type = XCB_KEY_RELEASE;
    xcb_send_event(xwm->conn, 0, window->id, XCB_EVENT_MASK_KEY_RELEASE,
                   (char *)&release);
  }
  xcb_flush(xwm->conn);

  LOG_TRACE("XWM: Injected text (%zu chars) to focused window", strlen(text));
}

void xwm_send_axis_event(struct xwm_window *window, int32_t axis, int32_t value,
                         int32_t x, int32_t y) {
  if (!window || !window->xwm || !window->xwm->conn)
    return;

  if (!window->mapped)
    return;

  if (value == 0)
    return;

  uint8_t x11_button;
  if (axis == 0)
    x11_button = (value < 0) ? 4 : 5;
  else
    x11_button = (value < 0) ? 6 : 7;

  int16_t root_x = clamp_i16(window->x + x);
  int16_t root_y = clamp_i16(window->y + y);

  xcb_button_press_event_t press_event = {.response_type = XCB_BUTTON_PRESS,
                                          .detail = x11_button,
                                          .time = XCB_CURRENT_TIME,
                                          .root = window->xwm->screen->root,
                                          .event = window->id,
                                          .child = XCB_WINDOW_NONE,
                                          .root_x = root_x,
                                          .root_y = root_y,
                                          .event_x = clamp_i16(x),
                                          .event_y = clamp_i16(y),
                                          .state = window->xwm->button_state,
                                          .same_screen = 1};
  xcb_send_event(window->xwm->conn, 1, window->id, XCB_EVENT_MASK_BUTTON_PRESS,
                 (char *)&press_event);

  xcb_button_release_event_t release_event = {
      .response_type = XCB_BUTTON_RELEASE,
      .detail = x11_button,
      .time = XCB_CURRENT_TIME,
      .root = window->xwm->screen->root,
      .event = window->id,
      .child = XCB_WINDOW_NONE,
      .root_x = root_x,
      .root_y = root_y,
      .event_x = clamp_i16(x),
      .event_y = clamp_i16(y),
      .state = window->xwm->button_state,
      .same_screen = 1};
  xcb_send_event(window->xwm->conn, 1, window->id,
                 XCB_EVENT_MASK_BUTTON_RELEASE, (char *)&release_event);

  xcb_flush(window->xwm->conn);
}

#define XWM_RANDR_OUTPUT_ID 0x100001
#define XWM_RANDR_CRTC_ID 0x100002
#define XWM_RANDR_MODE_ID 0x100003

bool xwm_init_randr(struct xwm *xwm) {
  if (!xwm || !xwm->conn)
    return false;

  xcb_prefetch_extension_data(xwm->conn, &xcb_randr_id);
  xwm->randr = xcb_get_extension_data(xwm->conn, &xcb_randr_id);

  if (!xwm->randr || !xwm->randr->present) {
    LOG_WARN("XWM: RandR extension not available in Xwayland");
    return false;
  }

  xwm->randr_event_base = xwm->randr->first_event;

  xcb_randr_query_version_cookie_t version_cookie =
      xcb_randr_query_version(xwm->conn, 1, 5);
  xcb_randr_query_version_reply_t *version_reply =
      xcb_randr_query_version_reply(xwm->conn, version_cookie, NULL);

  if (!version_reply) {
    LOG_WARN("XWM: Failed to query RandR version");
    return false;
  }

  LOG_INFO("XWM: RandR %u.%u available (event_base=%u)",
           version_reply->major_version, version_reply->minor_version,
           xwm->randr_event_base);
  free(version_reply);

  xwm->randr_output = XWM_RANDR_OUTPUT_ID;
  xwm->randr_crtc = XWM_RANDR_CRTC_ID;
  xwm->randr_mode = XWM_RANDR_MODE_ID;

  uint32_t width = 1920, height = 1080;
  if (xwm->bridge && xwm->bridge->plexy_conn) {
    plexy_get_screen_size(xwm->bridge->plexy_conn, &width, &height);
  }
  if (width == 0 || height == 0) {
    width = 1920;
    height = 1080;
  }

  xcb_randr_get_screen_resources_current_cookie_t res_cookie =
      xcb_randr_get_screen_resources_current(xwm->conn, xwm->screen->root);
  xcb_randr_get_screen_resources_current_reply_t *res_reply =
      xcb_randr_get_screen_resources_current_reply(xwm->conn, res_cookie, NULL);

  if (res_reply) {
    int num_outputs =
        xcb_randr_get_screen_resources_current_outputs_length(res_reply);
    int num_crtcs =
        xcb_randr_get_screen_resources_current_crtcs_length(res_reply);
    LOG_INFO("XWM: RandR screen resources: %d outputs, %d crtcs", num_outputs,
             num_crtcs);

    xcb_randr_output_t *outputs =
        xcb_randr_get_screen_resources_current_outputs(res_reply);
    xcb_randr_crtc_t *crtcs =
        xcb_randr_get_screen_resources_current_crtcs(res_reply);

    if (num_outputs > 0 && num_crtcs > 0) {

      xcb_randr_get_output_info_cookie_t out_cookie = xcb_randr_get_output_info(
          xwm->conn, outputs[0], res_reply->config_timestamp);
      xcb_randr_get_output_info_reply_t *out_reply =
          xcb_randr_get_output_info_reply(xwm->conn, out_cookie, NULL);

      if (out_reply) {
        int num_modes = xcb_randr_get_output_info_modes_length(out_reply);
        LOG_INFO("XWM: Output has %d modes, connection=%d, crtc=%u", num_modes,
                 out_reply->connection, out_reply->crtc);

        if (num_modes > 0 && out_reply->crtc != XCB_NONE) {
          xcb_randr_mode_t *modes = xcb_randr_get_output_info_modes(out_reply);

          xcb_randr_set_crtc_config_cookie_t cfg_cookie =
              xcb_randr_set_crtc_config(
                  xwm->conn, out_reply->crtc, XCB_CURRENT_TIME,
                  res_reply->config_timestamp, 0, 0, modes[0],
                  XCB_RANDR_ROTATION_ROTATE_0, 1, &outputs[0]);
          xcb_randr_set_crtc_config_reply_t *cfg_reply =
              xcb_randr_set_crtc_config_reply(xwm->conn, cfg_cookie, NULL);

          if (cfg_reply) {
            LOG_INFO("XWM: CRTC configured, status=%d", cfg_reply->status);
            free(cfg_reply);
          }
        }
        free(out_reply);
      }
    }
    free(res_reply);
  }

  xcb_randr_select_input(xwm->conn, xwm->screen->root,
                         XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE |
                             XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE |
                             XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE);

  xcb_flush(xwm->conn);

  LOG_INFO("XWM: RandR initialized - screen %ux%u", width, height);
  return true;
}

void xwm_randr_update_screen(struct xwm *xwm, uint32_t width, uint32_t height) {
  if (!xwm || !xwm->conn || !xwm->randr || !xwm->randr->present)
    return;

  LOG_DEBUG("XWM: RandR screen update %ux%u", width, height);
}
