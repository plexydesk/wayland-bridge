/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#ifndef XWM_H
#define XWM_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-server.h>
#include <xcb/composite.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xfixes.h>
#include <xcb/xtest.h>

struct wayland_bridge;
struct bridge_surface;
struct PlexyWindow;
typedef struct PlexyWindow PlexyWindow;

struct xwm_atoms {

  xcb_atom_t wm_protocols;
  xcb_atom_t wm_normal_hints;
  xcb_atom_t wm_take_focus;
  xcb_atom_t wm_delete_window;
  xcb_atom_t wm_state;
  xcb_atom_t wm_s0;
  xcb_atom_t wm_client_machine;
  xcb_atom_t wm_change_state;

  xcb_atom_t net_frame_extents;
  xcb_atom_t net_wm_cm_s0;
  xcb_atom_t net_wm_name;
  xcb_atom_t net_wm_pid;
  xcb_atom_t net_wm_icon;
  xcb_atom_t net_wm_state;
  xcb_atom_t net_wm_state_maximized_vert;
  xcb_atom_t net_wm_state_maximized_horz;
  xcb_atom_t net_wm_state_fullscreen;
  xcb_atom_t net_wm_user_time;
  xcb_atom_t net_wm_icon_name;
  xcb_atom_t net_wm_desktop;
  xcb_atom_t net_wm_window_type;
  xcb_atom_t net_wm_window_type_desktop;
  xcb_atom_t net_wm_window_type_dock;
  xcb_atom_t net_wm_window_type_toolbar;
  xcb_atom_t net_wm_window_type_menu;
  xcb_atom_t net_wm_window_type_utility;
  xcb_atom_t net_wm_window_type_splash;
  xcb_atom_t net_wm_window_type_dialog;
  xcb_atom_t net_wm_window_type_dropdown;
  xcb_atom_t net_wm_window_type_popup;
  xcb_atom_t net_wm_window_type_tooltip;
  xcb_atom_t net_wm_window_type_notification;
  xcb_atom_t net_wm_window_type_combo;
  xcb_atom_t net_wm_window_type_dnd;
  xcb_atom_t net_wm_window_type_normal;
  xcb_atom_t net_wm_moveresize;
  xcb_atom_t net_supporting_wm_check;
  xcb_atom_t net_supported;
  xcb_atom_t net_active_window;
  xcb_atom_t net_client_list;
  xcb_atom_t net_wm_ping;

  xcb_atom_t wm_hints;
  xcb_atom_t motif_wm_hints;

  xcb_atom_t clipboard;
  xcb_atom_t clipboard_manager;
  xcb_atom_t targets;
  xcb_atom_t utf8_string;
  xcb_atom_t wl_selection;
  xcb_atom_t incr;
  xcb_atom_t timestamp;
  xcb_atom_t multiple;
  xcb_atom_t compound_text;
  xcb_atom_t text;
  xcb_atom_t string;
  xcb_atom_t window;
  xcb_atom_t text_plain_utf8;
  xcb_atom_t text_plain;

  xcb_atom_t primary;
  xcb_atom_t _wl_primary_selection;

  xcb_atom_t net_wm_state_above;
  xcb_atom_t net_wm_state_below;
  xcb_atom_t net_wm_state_modal;
  xcb_atom_t net_wm_state_hidden;

  xcb_atom_t wl_surface_id;
  xcb_atom_t wl_surface_serial;
  xcb_atom_t allow_commits;
  xcb_atom_t xquadro_window_id;
};

struct xwm_size_hints {
  uint32_t flags;
  int32_t x, y;
  int32_t width, height;
  int32_t min_width, min_height;
  int32_t max_width, max_height;
  int32_t width_inc, height_inc;
  struct {
    int32_t x;
    int32_t y;
  } min_aspect, max_aspect;
  int32_t base_width, base_height;
  int32_t win_gravity;
};

#define XWM_US_POSITION (1L << 0)
#define XWM_US_SIZE (1L << 1)
#define XWM_P_POSITION (1L << 2)
#define XWM_P_SIZE (1L << 3)
#define XWM_P_MIN_SIZE (1L << 4)
#define XWM_P_MAX_SIZE (1L << 5)
#define XWM_P_RESIZE_INC (1L << 6)
#define XWM_P_ASPECT (1L << 7)
#define XWM_P_BASE_SIZE (1L << 8)
#define XWM_P_WIN_GRAVITY (1L << 9)

struct xwm_motif_hints {
  uint32_t flags;
  uint32_t functions;
  uint32_t decorations;
  int32_t input_mode;
  uint32_t status;
};

#define XWM_MWM_HINTS_FUNCTIONS (1L << 0)
#define XWM_MWM_HINTS_DECORATIONS (1L << 1)

#define XWM_MWM_FUNC_ALL (1L << 0)
#define XWM_MWM_FUNC_RESIZE (1L << 1)
#define XWM_MWM_FUNC_MOVE (1L << 2)
#define XWM_MWM_FUNC_MINIMIZE (1L << 3)
#define XWM_MWM_FUNC_MAXIMIZE (1L << 4)
#define XWM_MWM_FUNC_CLOSE (1L << 5)

#define XWM_MWM_DECOR_ALL (1L << 0)
#define XWM_MWM_DECOR_BORDER (1L << 1)
#define XWM_MWM_DECOR_RESIZEH (1L << 2)
#define XWM_MWM_DECOR_TITLE (1L << 3)
#define XWM_MWM_DECOR_MENU (1L << 4)
#define XWM_MWM_DECOR_MINIMIZE (1L << 5)
#define XWM_MWM_DECOR_MAXIMIZE (1L << 6)

#define XWM_ICCCM_WITHDRAWN_STATE 0
#define XWM_ICCCM_NORMAL_STATE 1
#define XWM_ICCCM_ICONIC_STATE 3

struct xwm_window {
  struct xwm *xwm;
  xcb_window_t id;

  int32_t x, y;
  int32_t width, height;
  int32_t saved_width, saved_height;
  int32_t create_width, create_height;

  char *name;
  char *class;
  char *machine;
  int32_t pid;

  uint32_t surface_id;
  uint64_t surface_serial;
  uint32_t xquadro_window_id;
  struct bridge_surface *surface;
  struct wl_listener surface_destroy_listener;

  bool override_redirect;
  bool mapped;
  bool fullscreen;
  bool maximized_vert;
  bool maximized_horz;
  bool above;
  bool below;
  bool modal;
  bool hidden;
  bool properties_dirty;
  bool has_alpha;
  bool decorate;

  uint32_t protocols;
  bool delete_window;
  bool take_focus;
  bool wants_input;
  xcb_atom_t type;
  struct xwm_size_hints size_hints;
  struct xwm_motif_hints motif_hints;

  struct xwm_window *transient_for;

  struct xwm_window *popup_parent_window;
  PlexyWindow *popup_parent_plexy;

  struct wl_list link;
};

enum xwm_state {
  XWM_UNINITIALIZED,
  XWM_CONNECTING,
  XWM_CONNECTED,
  XWM_READY,
  XWM_FAILED
};

struct xwm {

  xcb_connection_t *conn;
  xcb_screen_t *screen;
  const xcb_query_extension_reply_t *xfixes;
  bool xtest_present;

  struct wl_event_source *source;
  struct wl_event_loop *loop;

  struct wl_list window_list;
  struct wl_list unpaired_list;
  struct wl_list unpaired_surface_list;
  struct xwm_window *focus_window;
  struct xwm_window *pending_focus_window;
  xcb_window_t no_focus_window;
  uint16_t last_focus_seq;

  struct wayland_bridge *bridge;

  xcb_window_t wm_window;

  struct xwm_atoms atoms;

  int display_number;

  struct wl_global *xwayland_shell_global;
  bool shell_bound;

  enum xwm_state state;
  int connection_attempts;
  struct wl_event_source *retry_timer;

  bool debug;

  struct xwm_hash_table *window_hash;

  uint16_t button_state;
  xcb_window_t button_press_window;

  const xcb_query_extension_reply_t *randr;
  uint8_t randr_event_base;
  xcb_randr_output_t randr_output;
  xcb_randr_crtc_t randr_crtc;
  xcb_randr_mode_t randr_mode;

  bool clipboard_owner;
  bool primary_owner;

  bool incr_active;
  xcb_atom_t incr_property;
  char *incr_buffer;
  size_t incr_buffer_len;
  size_t incr_buffer_cap;
};

struct xwm *xwm_create(struct wayland_bridge *bridge, int display_number);

struct xwm *xwm_create_async(struct wayland_bridge *bridge, int display_number);

bool xwm_is_ready(struct xwm *xwm);

void xwm_destroy(struct xwm *xwm);

void xwm_surface_created(struct xwm *xwm, struct bridge_surface *surface,
                         uint32_t surface_id);

struct xwm_window *xwm_get_window(struct xwm *xwm, xcb_window_t id);

void xwm_set_focus(struct xwm *xwm, struct xwm_window *window);

bool xwm_window_is_focus_inactive(struct xwm_window *window);

void xwm_send_enter_event(struct xwm_window *window, int32_t x, int32_t y);
void xwm_send_leave_event(struct xwm_window *window, int32_t x, int32_t y);
void xwm_send_motion_event(struct xwm_window *window, int32_t x, int32_t y);
void xwm_send_button_event(struct xwm_window *window, uint32_t button,
                           bool pressed, int32_t x, int32_t y);
void xwm_send_axis_event(struct xwm_window *window, int32_t axis, int32_t value,
                         int32_t x, int32_t y);
void xwm_send_key_event(struct xwm *xwm, uint32_t keycode, bool pressed,
                        uint32_t modifiers);
void xwm_inject_text(struct xwm *xwm, const char *text);
void xwm_inject_keysym(struct xwm *xwm, uint32_t keysym, bool shift);
void xwm_send_focus_in_event(struct xwm_window *window);
void xwm_send_focus_out_event(struct xwm_window *window);
void xwm_send_expose_event(struct xwm_window *window);

void xwm_window_close(struct xwm_window *window);

void xwm_window_configure(struct xwm_window *window, int32_t x, int32_t y,
                          int32_t width, int32_t height);

char *xwm_get_atom_name(struct xwm *xwm, xcb_atom_t atom);

struct wl_display *xwm_get_display(struct xwm *xwm);

struct wl_client *bridge_get_xwayland_client(void);
int bridge_get_xwayland_wm_fd(void);

struct xwm_xwayland_surface {
  struct xwm *xwm;
  struct wl_resource *resource;
  struct bridge_surface *surface;
  uint64_t serial;
  struct wl_listener surface_commit_listener;
  struct wl_listener surface_destroy_listener;
  struct wl_list link;
};

void xwm_init_atoms(struct xwm *xwm);
void xwm_window_read_properties(struct xwm_window *window);
void xwm_window_set_wm_state(struct xwm_window *window, int32_t state);
void xwm_window_set_net_wm_state(struct xwm_window *window);
void xwm_window_try_pair_surface(struct xwm_window *window);

bool xwm_init_randr(struct xwm *xwm);
void xwm_randr_update_screen(struct xwm *xwm, uint32_t width, uint32_t height);

struct xwm_hash_table;
struct xwm_hash_table *xwm_hash_table_create(void);
void xwm_hash_table_destroy(struct xwm_hash_table *table);
void xwm_hash_table_insert(struct xwm_hash_table *table, uint32_t key,
                           void *value);
void *xwm_hash_table_lookup(struct xwm_hash_table *table, uint32_t key);
void xwm_hash_table_remove(struct xwm_hash_table *table, uint32_t key);

#endif
