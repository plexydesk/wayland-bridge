/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef WAYLAND_BRIDGE_H
#define WAYLAND_BRIDGE_H

#include "../../include/plexy.h"
#include <stdbool.h>
#include <stddef.h>
#include <wayland-server.h>

struct weston_bridge;
struct weston_surface;

static inline uint32_t plexy_mods_to_xkb(uint32_t plexy) {
  uint32_t xkb = 0;
  if (plexy & 0x01)
    xkb |= 0x01;
  if (plexy & 0x02)
    xkb |= 0x04;
  if (plexy & 0x04)
    xkb |= 0x08;
  if (plexy & 0x08)
    xkb |= 0x40;
  return xkb;
}

struct bridge_region_op {
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
  bool add;
};

struct bridge_region_state {
  struct bridge_region_op *ops;
  size_t count;
  size_t capacity;
};

struct bridge_surface {
  struct wl_resource *resource;
  uint32_t magic;
  struct wl_list link;
  struct wl_signal commit_signal;

  struct weston_surface *weston_surface;
  struct wl_listener weston_destroy_listener;

  struct wl_resource *xdg_surface;
  struct wl_resource *xdg_toplevel;
  struct wl_resource *xdg_popup;
  struct wl_resource *xdg_toplevel_decoration;
  uint32_t xdg_decoration_mode;

  char *icon_name;

  char *title;
  char *app_id;
  struct {
    int32_t x, y, width, height;
  } window_geometry;
  bool geometry_set;
  bool is_modal;

  int32_t min_width, min_height;
  int32_t max_width, max_height;

  bool maximized;
  bool fullscreen;
  bool activated;
  bool resizing;
  bool tiled_left;
  bool tiled_right;
  bool suspended;
  uint32_t last_configured_width;
  uint32_t last_configured_height;

  struct bridge_surface *popup_parent;
  struct bridge_surface *toplevel_parent;

  struct bridge_surface *subsurface_parent;
  struct wl_list subsurfaces;
  struct wl_list subsurface_link;
  int32_t subsurface_x, subsurface_y;
  bool is_subsurface;
  bool subsurface_sync;

  bool is_x11;
  void *xwm_window;

  PlexyWindow *plexy_window;
  uint32_t plexy_window_id;
  uint32_t plexy_window_width;
  uint32_t plexy_window_height;
  uint32_t current_output_id;
  uint32_t entered_output_ids[4];
  uint32_t entered_output_count;
  uint32_t requested_cursor_shape;
  bool requested_cursor_visible;
  bool pointer_lock_cursor_hidden;

  int32_t screen_x, screen_y;
  int32_t screen_width, screen_height;
  bool screen_pos_valid;

  struct wl_resource *pending_buffer;
  struct wl_listener pending_buffer_destroy_listener;
  int32_t pending_dx;
  int32_t pending_dy;
  bool has_damage;
  struct {
    int32_t x, y, width, height;
  } damage;

  struct wl_resource *current_buffer;
  struct wl_listener current_buffer_destroy_listener;
  struct wl_resource *previous_buffer;
  struct wl_listener previous_buffer_destroy_listener;
  PlexyBuffer *front_plexy_buffer;
  PlexyBuffer *back_plexy_buffer;
  uint32_t buffer_scale;
  bool frame_pending;

  struct wl_list frame_callbacks;
  struct wl_list presentation_feedbacks;

  uint32_t configure_serial;
  uint32_t last_acked_serial;
  bool configured;

  struct wl_list pending_configures;

  bool input_region_set;
  struct bridge_region_state input_region;
  bool opaque_region_set;
  struct bridge_region_state opaque_region;

  struct wl_resource *viewport_resource;
  bool viewport_src_set;
  bool viewport_dst_set;
  wl_fixed_t viewport_src_x;
  wl_fixed_t viewport_src_y;
  wl_fixed_t viewport_src_width;
  wl_fixed_t viewport_src_height;
  int32_t viewport_dst_width;
  int32_t viewport_dst_height;

  bool has_pointer_position;
  int32_t pointer_x;
  int32_t pointer_y;

  bool pending_enter;
  int32_t pending_enter_x;
  int32_t pending_enter_y;

  struct wl_listener client_destroy_listener;

  void *syncobj_surface;

  uint32_t current_buf_release_handle;
  uint64_t current_buf_release_point;
  bool current_buf_has_release;
  uint32_t prev_buf_release_handle;
  uint64_t prev_buf_release_point;
  bool prev_buf_has_release;

  void *fifo_surface;
};

static inline bool bridge_surface_uses_logical_content_buffer(
    const struct bridge_surface *surface) {
  return surface && !surface->is_x11 && !surface->fullscreen;
}

static inline bool
bridge_surface_uses_window_geometry_crop(const struct bridge_surface *surface) {
  return bridge_surface_uses_logical_content_buffer(surface) &&
         surface->xdg_toplevel && !surface->xdg_popup &&
         !surface->is_subsurface && surface->geometry_set &&
         surface->window_geometry.width > 0 &&
         surface->window_geometry.height > 0;
}

struct bridge_buffer {
  struct wl_resource *resource;
  struct wl_shm_buffer *shm_buffer;
  PlexyBuffer *plexy_buffer;
  bool uploaded;
};

struct bridge_shm_buffer;
struct bridge_shm_buffer *
bridge_shm_buffer_from_resource(struct wl_resource *resource);
uint32_t bridge_shm_buffer_get_width(const struct bridge_shm_buffer *buffer);
uint32_t bridge_shm_buffer_get_height(const struct bridge_shm_buffer *buffer);
uint32_t bridge_shm_buffer_get_stride(const struct bridge_shm_buffer *buffer);
uint32_t bridge_shm_buffer_get_format(const struct bridge_shm_buffer *buffer);
int32_t bridge_shm_buffer_get_offset(const struct bridge_shm_buffer *buffer);
void *bridge_shm_buffer_get_pool_data(const struct bridge_shm_buffer *buffer);
size_t bridge_shm_buffer_get_pool_size(const struct bridge_shm_buffer *buffer);
void *bridge_shm_buffer_get_data(const struct bridge_shm_buffer *buffer);

struct bridge_output {
  struct wl_resource *resource;
  uint32_t output_id;
};

struct wayland_bridge {

  struct wl_display *display;
  struct wl_event_loop *loop;
  int wayland_fd;

  struct weston_bridge *weston;
  bool use_weston;

  struct wl_global *compositor_global;
  struct wl_global *subcompositor_global;
  struct wl_global *shm_global;
  struct wl_global *xdg_wm_base_global;
  struct wl_global *seat_global;
  struct wl_global *output_global;
  struct wl_global *data_device_manager_global;
  struct wl_global *text_input_manager_global;
  struct wl_global *dmabuf_global;
  bool dmabuf_enabled;

  struct wl_list seat_resources;
  struct wl_list pointer_clients;
  struct wl_list keyboard_clients;
  struct wl_list touch_clients;
  struct wl_list text_input_clients;
  struct wl_resource *active_input_method;

  bool session_locked;

  struct bridge_surface *pointer_focus;
  struct bridge_surface *keyboard_focus;

  struct wl_resource *seat_pointer_resource;
  struct wl_resource *seat_pointer_surface;

  uint32_t mods_depressed;
  uint32_t mods_latched;
  uint32_t mods_locked;
  uint32_t mods_group;

  struct wl_list surfaces;
  struct bridge_surface *cached_window_id_surface;
  uint32_t cached_window_id;

  PlexyConnection *plexy_conn;
  int plexy_fd;

  bool running;
  bool raw_input_active;

  struct bridge_surface *grabbed_popup;
};

extern struct wayland_bridge *bridge;

#define PLEXY_BRIDGE_LOG_LEVEL_ERROR 0
#define PLEXY_BRIDGE_LOG_LEVEL_WARN 1
#define PLEXY_BRIDGE_LOG_LEVEL_INFO 2
#define PLEXY_BRIDGE_LOG_LEVEL_DEBUG 3
#define PLEXY_BRIDGE_LOG_LEVEL_TRACE 4

typedef enum {
  LOG_LEVEL_ERROR = PLEXY_BRIDGE_LOG_LEVEL_ERROR,
  LOG_LEVEL_WARN = PLEXY_BRIDGE_LOG_LEVEL_WARN,
  LOG_LEVEL_INFO = PLEXY_BRIDGE_LOG_LEVEL_INFO,
  LOG_LEVEL_DEBUG = PLEXY_BRIDGE_LOG_LEVEL_DEBUG,
  LOG_LEVEL_TRACE = PLEXY_BRIDGE_LOG_LEVEL_TRACE,
} BridgeLogLevel;

#ifndef PLEXY_BRIDGE_COMPILED_LOG_LEVEL
#define PLEXY_BRIDGE_COMPILED_LOG_LEVEL PLEXY_BRIDGE_LOG_LEVEL_INFO
#endif

#ifndef PLEXY_BRIDGE_DEFAULT_LOG_LEVEL
#define PLEXY_BRIDGE_DEFAULT_LOG_LEVEL PLEXY_BRIDGE_LOG_LEVEL_INFO
#endif

void bridge_log_set_level(BridgeLogLevel level);
BridgeLogLevel bridge_log_get_level(void);

void bridge_log_impl(BridgeLogLevel level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

void bridge_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define LOG_ERROR(...) bridge_log_impl(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_WARN(...) bridge_log_impl(LOG_LEVEL_WARN, __VA_ARGS__)
#define LOG_INFO(...) bridge_log_impl(LOG_LEVEL_INFO, __VA_ARGS__)
#if PLEXY_BRIDGE_COMPILED_LOG_LEVEL >= PLEXY_BRIDGE_LOG_LEVEL_DEBUG
#define LOG_DEBUG(...) bridge_log_impl(LOG_LEVEL_DEBUG, __VA_ARGS__)
#else
#define LOG_DEBUG(...)                                                         \
  do {                                                                         \
  } while (0)
#endif
#if PLEXY_BRIDGE_COMPILED_LOG_LEVEL >= PLEXY_BRIDGE_LOG_LEVEL_TRACE
#define LOG_TRACE(...) bridge_log_impl(LOG_LEVEL_TRACE, __VA_ARGS__)
#else
#define LOG_TRACE(...)                                                         \
  do {                                                                         \
  } while (0)
#endif

struct bridge_surface *bridge_surface_create(struct wl_resource *resource);
void bridge_surface_destroy(struct bridge_surface *surface);
struct bridge_surface *
bridge_surface_from_resource(struct wl_resource *resource);
struct bridge_surface *bridge_surface_from_window_id(uint32_t window_id);
void bridge_surface_set_plexy_window(struct bridge_surface *surface,
                                     PlexyWindow *window);
void bridge_surface_clear_plexy_window(struct bridge_surface *surface);
bool bridge_surface_input_region_contains(const struct bridge_surface *surface,
                                          int32_t x, int32_t y);
void bridge_sync_input_region_to_plexy(struct bridge_surface *surface);
void bridge_surface_update_cursor_shape(struct bridge_surface *surface,
                                        uint32_t shape);
void bridge_surface_set_pointer_lock_cursor_hidden(
    struct bridge_surface *surface, bool hidden);

void bind_compositor(struct wl_client *client, void *data, uint32_t version,
                     uint32_t id);
void bind_subcompositor(struct wl_client *client, void *data, uint32_t version,
                        uint32_t id);
void bind_shm(struct wl_client *client, void *data, uint32_t version,
              uint32_t id);
void bind_xdg_wm_base(struct wl_client *client, void *data, uint32_t version,
                      uint32_t id);
void xdg_wm_base_send_pings(void);
void bind_seat(struct wl_client *client, void *data, uint32_t version,
               uint32_t id);
void bind_output(struct wl_client *client, void *data, uint32_t version,
                 uint32_t id);
bool bridge_create_output_globals(void);
void bridge_refresh_outputs(void);
void bridge_refresh_xdg_outputs(void);
bool bridge_get_output_info(uint32_t output_id, PlexyOutputInfo *out_info);
bool bridge_get_default_output_info(PlexyOutputInfo *out_info);
bool bridge_wl_output_resource_get_info(struct wl_resource *output_resource,
                                        PlexyOutputInfo *out_info);
bool bridge_surface_get_output_info(struct bridge_surface *surface,
                                    PlexyOutputInfo *out_info);
float bridge_output_scale_factor(const PlexyOutputInfo *info);
float bridge_surface_scale_factor(struct bridge_surface *surface);
uint32_t bridge_physical_to_logical_extent(uint32_t physical_extent,
                                           float scale);
void bridge_physical_to_surface_size(struct bridge_surface *surface,
                                     uint32_t physical_width,
                                     uint32_t physical_height,
                                     uint32_t *logical_width,
                                     uint32_t *logical_height);
void bind_data_device_manager(struct wl_client *client, void *data,
                              uint32_t version, uint32_t id);
void bind_text_input_manager(struct wl_client *client, void *data,
                             uint32_t version, uint32_t id);
void bind_primary_selection_device_manager(struct wl_client *client, void *data,
                                           uint32_t version, uint32_t id);
void bind_data_control_manager(struct wl_client *client, void *data,
                               uint32_t version, uint32_t id);
void bridge_data_device_send_selection_to_client(struct wl_client *client);
void bridge_data_device_send_selection_to_focus(void);
void bridge_data_device_pointer_motion(struct bridge_surface *surface,
                                       int32_t x, int32_t y);
void bridge_data_device_pointer_button(struct bridge_surface *surface,
                                       bool pressed, int32_t x, int32_t y);
void bridge_ensure_clipboard_callback(void);
void bridge_clipboard_set_text(const char *text);
bool bridge_clipboard_get_text(char *out, size_t out_size);
bool bridge_clipboard_has_text(void);
void bridge_clipboard_register_change_listener(
    void (*callback)(void *user_data), void *user_data);
void bridge_primary_set_text(const char *text);
bool bridge_primary_get_text(char *out, size_t out_size);
bool bridge_primary_has_text(void);
void bridge_primary_register_change_listener(void (*callback)(void *user_data),
                                             void *user_data);

struct wl_resource *
bridge_get_pointer_for_surface(struct bridge_surface *surface);
struct wl_resource *
bridge_get_keyboard_for_surface(struct bridge_surface *surface);
struct wl_resource *
bridge_get_touch_for_surface(struct bridge_surface *surface);

void bridge_text_input_notify_focus(struct bridge_surface *surface,
                                    bool focused);
void bridge_text_input_try_commit_key(struct bridge_surface *surface,
                                      uint32_t keycode, uint32_t modifiers);
void bridge_ime_notify_text_input_state(bool enabled);
bool bridge_pointer_constraints_apply_motion(struct bridge_surface *surface,
                                             int32_t *x, int32_t *y);
void bridge_pointer_constraints_set_focus(struct bridge_surface *surface,
                                          bool focused);
bool bridge_pointer_constraints_has_active(struct bridge_surface *surface);
void bridge_relative_pointer_send_motion(struct bridge_surface *surface,
                                         double dx, double dy);

void bridge_raw_pointer_motion(int32_t screen_x, int32_t screen_y, double dx,
                               double dy, uint32_t window_id);
void bridge_raw_pointer_button(uint32_t button, uint32_t state);
void bridge_raw_pointer_axis(int32_t axis, int32_t value, int32_t discrete);

int bridge_for_each_pointer(struct bridge_surface *surface,
                            void (*callback)(struct wl_resource *pointer,
                                             void *data),
                            void *data);

int bridge_for_each_keyboard(struct bridge_surface *surface,
                             void (*callback)(struct wl_resource *keyboard,
                                              void *data),
                             void *data);

void pointer_send_frame(struct wl_resource *pointer);

void forward_pointer_enter(PlexyWindow *window, int32_t x, int32_t y,
                           void *user_data);
void forward_pointer_leave(PlexyWindow *window, void *user_data);
void forward_pointer_motion(PlexyWindow *window, int32_t x, int32_t y,
                            void *user_data);
void forward_pointer_button(PlexyWindow *window, uint32_t button, bool pressed,
                            int32_t x, int32_t y, void *user_data);
void forward_pointer_axis(PlexyWindow *window, int32_t axis, int32_t value,
                          int32_t discrete, void *user_data);
void forward_touch_down(PlexyWindow *window, int32_t touch_id, int32_t x,
                        int32_t y, uint32_t time_ms, void *user_data);
void forward_touch_motion(PlexyWindow *window, int32_t touch_id, int32_t x,
                          int32_t y, uint32_t time_ms, void *user_data);
void forward_touch_up(PlexyWindow *window, int32_t touch_id, uint32_t time_ms,
                      void *user_data);
void forward_touch_cancel(PlexyWindow *window, void *user_data);
void forward_touch_frame(PlexyWindow *window, void *user_data);
void forward_key(PlexyWindow *window, uint32_t keycode, bool pressed,
                 uint32_t modifiers, void *user_data);
void forward_modifiers(PlexyWindow *window, uint32_t depressed,
                       uint32_t latched, uint32_t locked, uint32_t group,
                       void *user_data);
void forward_focus_in(PlexyWindow *window, void *user_data);
void forward_focus_out(PlexyWindow *window, void *user_data);
void forward_configure(PlexyWindow *window, uint32_t width, uint32_t height,
                       uint32_t state_flags, void *user_data);
void forward_close(PlexyWindow *window, void *user_data);
void forward_frame_done(PlexyWindow *window, void *user_data);
void forward_enter_output(PlexyWindow *window, uint32_t output_id,
                          void *user_data);
void forward_leave_output(PlexyWindow *window, uint32_t output_id,
                          void *user_data);
void bridge_send_xdg_toplevel_bounds(struct bridge_surface *surface);

void bridge_weston_surface_created(struct wayland_bridge *bridge,
                                   struct weston_surface *surface);

struct bridge_dmabuf_buffer;

bool bridge_dmabuf_init(void);
void bridge_dmabuf_cleanup(void);
void bind_dmabuf(struct wl_client *client, void *data, uint32_t version,
                 uint32_t id);

void bind_xdg_output_manager(struct wl_client *client, void *data,
                             uint32_t version, uint32_t id);

void bind_viewporter(struct wl_client *client, void *data, uint32_t version,
                     uint32_t id);

void bind_pointer_constraints(struct wl_client *client, void *data,
                              uint32_t version, uint32_t id);

void bind_relative_pointer_manager(struct wl_client *client, void *data,
                                   uint32_t version, uint32_t id);

void bind_fractional_scale_manager(struct wl_client *client, void *data,
                                   uint32_t version, uint32_t id);

void bind_xdg_activation(struct wl_client *client, void *data, uint32_t version,
                         uint32_t id);

void bind_xdg_decoration_manager(struct wl_client *client, void *data,
                                 uint32_t version, uint32_t id);

void bind_xdg_toplevel_icon_manager(struct wl_client *client, void *data,
                                    uint32_t version, uint32_t id);

void bind_presentation(struct wl_client *client, void *data, uint32_t version,
                       uint32_t id);
void bridge_surface_send_presentation_feedback(struct bridge_surface *surface);
void bridge_surface_clear_presentation_feedbacks(
    struct bridge_surface *surface);

void bind_cursor_shape_manager(struct wl_client *client, void *data,
                               uint32_t version, uint32_t id);

void bind_idle_inhibit_manager(struct wl_client *client, void *data,
                               uint32_t version, uint32_t id);

void bind_single_pixel_buffer_manager(struct wl_client *client, void *data,
                                      uint32_t version, uint32_t id);

void bind_keyboard_shortcuts_inhibit_manager(struct wl_client *client,
                                             void *data, uint32_t version,
                                             uint32_t id);

void bind_content_type_manager(struct wl_client *client, void *data,
                               uint32_t version, uint32_t id);

void bind_tearing_control_manager(struct wl_client *client, void *data,
                                  uint32_t version, uint32_t id);

void bind_xdg_exporter(struct wl_client *client, void *data, uint32_t version,
                       uint32_t id);

void bind_xdg_importer(struct wl_client *client, void *data, uint32_t version,
                       uint32_t id);

void bind_foreign_toplevel_list(struct wl_client *client, void *data,
                                uint32_t version, uint32_t id);

void bind_toplevel_drag_manager(struct wl_client *client, void *data,
                                uint32_t version, uint32_t id);

void bind_alpha_modifier(struct wl_client *client, void *data, uint32_t version,
                         uint32_t id);
void bind_input_method_manager(struct wl_client *client, void *data,
                               uint32_t version, uint32_t id);
void bind_idle_notifier(struct wl_client *client, void *data, uint32_t version,
                        uint32_t id);
void bind_session_lock_manager(struct wl_client *client, void *data,
                               uint32_t version, uint32_t id);
void bind_xdg_wm_dialog(struct wl_client *client, void *data, uint32_t version,
                        uint32_t id);
void bind_xwayland_keyboard_grab_manager(struct wl_client *client, void *data,
                                         uint32_t version, uint32_t id);
void bind_input_timestamps_manager(struct wl_client *client, void *data,
                                   uint32_t version, uint32_t id);
void bind_fifo_manager(struct wl_client *client, void *data, uint32_t version,
                       uint32_t id);
void bind_drm_syncobj_manager(struct wl_client *client, void *data,
                              uint32_t version, uint32_t id);

bool bridge_syncobj_process_commit(struct bridge_surface *surface);

void bridge_syncobj_signal_release(struct bridge_surface *surface);

bool bridge_fifo_should_defer_frame(struct bridge_surface *surface);

void bridge_fifo_on_frame_done(struct bridge_surface *surface);

int bridge_dmabuf_get_drm_fd(void);
void bridge_idle_notify_activity(void);
void bridge_input_timestamps_send(struct wl_resource *input_resource);
bool bridge_xwayland_kbd_grab_active(struct wl_resource *surface);

void bridge_subsurface_enforce_zorder(struct bridge_surface *parent);

void bind_pointer_gestures(struct wl_client *client, void *data,
                           uint32_t version, uint32_t id);
void bridge_pointer_gesture_swipe_begin(struct bridge_surface *surface,
                                        uint32_t fingers);
void bridge_pointer_gesture_swipe_update(struct bridge_surface *surface,
                                         int32_t dx, int32_t dy);
void bridge_pointer_gesture_swipe_end(struct bridge_surface *surface,
                                      bool cancelled);
void bridge_pointer_gesture_pinch_begin(struct bridge_surface *surface,
                                        uint32_t fingers);
void bridge_pointer_gesture_pinch_update(struct bridge_surface *surface,
                                         int32_t dx, int32_t dy, double scale,
                                         double rotation);
void bridge_pointer_gesture_pinch_end(struct bridge_surface *surface,
                                      bool cancelled);
void bridge_pointer_gesture_hold_begin(struct bridge_surface *surface,
                                       uint32_t fingers);
void bridge_pointer_gesture_hold_end(struct bridge_surface *surface,
                                     bool cancelled);

bool bridge_is_dmabuf_buffer(struct wl_resource *buffer_resource);
struct bridge_dmabuf_buffer *
bridge_dmabuf_buffer_from_resource(struct wl_resource *resource);
uint32_t bridge_dmabuf_get_width(struct bridge_dmabuf_buffer *buffer);
uint32_t bridge_dmabuf_get_height(struct bridge_dmabuf_buffer *buffer);
uint32_t bridge_dmabuf_get_texture(struct bridge_dmabuf_buffer *buffer);
uint32_t bridge_dmabuf_get_format(struct bridge_dmabuf_buffer *buffer);
int bridge_dmabuf_get_fd(struct bridge_dmabuf_buffer *buffer);
uint32_t bridge_dmabuf_get_stride(struct bridge_dmabuf_buffer *buffer);
uint64_t bridge_dmabuf_get_modifier(struct bridge_dmabuf_buffer *buffer);

void bridge_notify_surface_created(struct bridge_surface *surface);

bool bridge_is_xwayland_client(struct wl_client *client);

struct wl_client *bridge_get_xwayland_client(void);

bool bridge_create_x11_window(struct bridge_surface *surface, const char *title,
                              int32_t x, int32_t y, int32_t width,
                              int32_t height, uint32_t window_type);

bool bridge_create_x11_popup(struct bridge_surface *surface, const char *title,
                             PlexyWindow *parent_window, int32_t x, int32_t y,
                             int32_t width, int32_t height,
                             uint32_t window_type);
bool bridge_update_x11_popup(struct bridge_surface *surface, int32_t x,
                             int32_t y, int32_t width, int32_t height,
                             uint32_t window_type);

void bridge_enable_raw_input(void);
void bridge_disable_raw_input(void);

bool bridge_pointer_constraints_is_locked(struct bridge_surface *surface);

#endif
