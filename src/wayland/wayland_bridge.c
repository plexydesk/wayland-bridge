/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#define _POSIX_C_SOURCE 200809L
#include "wayland_bridge.h"
#include "alpha-modifier-v1-protocol.h"
#include "content-type-v1-protocol.h"
#include "cursor-shape-v1-protocol.h"
#include "ext-data-control-v1-protocol.h"
#include "ext-foreign-toplevel-list-v1-protocol.h"
#include "ext-idle-notify-v1-protocol.h"
#include "ext-session-lock-v1-protocol.h"
#include "fifo-v1-protocol.h"
#include "fractional-scale-v1-protocol.h"
#include "idle-inhibit-v1-protocol.h"
#include "input-timestamps-v1-protocol.h"
#include "keyboard-shortcuts-inhibit-v1-protocol.h"
#include "linux-dmabuf-v1-protocol.h"
#include "linux-drm-syncobj-v1-protocol.h"
#include "pointer-constraints-v1-protocol.h"
#include "pointer-gestures-unstable-v1-protocol.h"
#include "presentation-time-protocol.h"
#include "primary-selection-v1-protocol.h"
#include "relative-pointer-v1-protocol.h"
#include "single-pixel-buffer-v1-protocol.h"
#include "tearing-control-v1-protocol.h"
#include "text-input-v3-protocol.h"
#include "viewporter-protocol.h"
#include "weston_compositor.h"
#include "xdg-activation-v1-protocol.h"
#include "xdg-decoration-v1-protocol.h"
#include "xdg-dialog-v1-protocol.h"
#include "xdg-foreign-v2-protocol.h"
#include "xdg-output-v1-protocol.h"
#include "xdg-shell-protocol.h"
#include "xdg-toplevel-drag-v1-protocol.h"
#include "xdg-toplevel-icon-v1-protocol.h"
#include "xwayland-keyboard-grab-v1-protocol.h"
#include "xwm.h"
#include "xx-input-method-v2-protocol.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#ifndef PLEXY_XWAYLAND_WIRE_DEBUG
#define PLEXY_XWAYLAND_WIRE_DEBUG 0
#endif

static pid_t xwayland_pid = -1;
static int xwayland_display_num = -1;
static int xwayland_wm_fd = -1;
static struct xwm *xwm = NULL;
static struct wl_client *xwayland_wl_client = NULL;
static bool xwayland_restart_pending = false;

static bool start_xwm_async(void);

struct wayland_bridge *bridge = NULL;
static FILE *log_file = NULL;
static FILE *protocol_log_file = NULL;
static struct wl_protocol_logger *protocol_logger = NULL;
static BridgeLogLevel current_log_level =
    (BridgeLogLevel)PLEXY_BRIDGE_DEFAULT_LOG_LEVEL;

struct xwm *xwm_get_instance(void) { return xwm; }

static const char *level_names[] = {"ERROR", "WARN", "INFO", "DEBUG", "TRACE"};

static int bridge_env_flag_enabled(const char *name) {
  const char *value = getenv(name);
  if (!value || !value[0]) {
    return 0;
  }

  if (value[0] == '0') {
    return 0;
  }

  if (strcasecmp(value, "false") == 0 || strcasecmp(value, "no") == 0 ||
      strcasecmp(value, "off") == 0) {
    return 0;
  }

  return 1;
}

static int bridge_parse_log_level(const char *value, BridgeLogLevel *level) {
  if (!value || !value[0] || !level) {
    return 0;
  }

  if (value[1] == '\0' && value[0] >= '0' && value[0] <= '4') {
    *level = (BridgeLogLevel)(value[0] - '0');
    return 1;
  }

  if (strcasecmp(value, "error") == 0) {
    *level = LOG_LEVEL_ERROR;
    return 1;
  }
  if (strcasecmp(value, "warn") == 0 || strcasecmp(value, "warning") == 0) {
    *level = LOG_LEVEL_WARN;
    return 1;
  }
  if (strcasecmp(value, "info") == 0) {
    *level = LOG_LEVEL_INFO;
    return 1;
  }
  if (strcasecmp(value, "debug") == 0) {
    *level = LOG_LEVEL_DEBUG;
    return 1;
  }
  if (strcasecmp(value, "trace") == 0) {
    *level = LOG_LEVEL_TRACE;
    return 1;
  }

  return 0;
}

void bridge_log_set_level(BridgeLogLevel level) { current_log_level = level; }

BridgeLogLevel bridge_log_get_level(void) { return current_log_level; }

void bridge_log_impl(BridgeLogLevel level, const char *fmt, ...) {
  if (level > current_log_level)
    return;

  va_list args;
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  struct tm *tm_info = localtime(&ts.tv_sec);

  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);

  printf("[%s.%03ld] [%5s] ", timestamp, ts.tv_nsec / 1000000,
         level_names[level]);
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  printf("\n");

  if (log_file) {
    fprintf(log_file, "[%s.%03ld] [%5s] ", timestamp, ts.tv_nsec / 1000000,
            level_names[level]);
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);
    fprintf(log_file, "\n");
  }
}

void bridge_log(const char *fmt, ...) {
  if (LOG_LEVEL_INFO > current_log_level)
    return;

  va_list args;
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  struct tm *tm_info = localtime(&ts.tv_sec);

  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);

  printf("[%s.%03ld] [ INFO] ", timestamp, ts.tv_nsec / 1000000);
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  printf("\n");

  if (log_file) {
    fprintf(log_file, "[%s.%03ld] [ INFO] ", timestamp, ts.tv_nsec / 1000000);
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);
    fprintf(log_file, "\n");
  }
}

static void
protocol_log_callback(void *user_data, enum wl_protocol_logger_type direction,
                      const struct wl_protocol_logger_message *msg) {
  (void)user_data;
  if (!protocol_log_file || !msg || !msg->resource || !msg->message)
    return;

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  struct tm *tm_info = localtime(&ts.tv_sec);
  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);

  const char *dir = (direction == WL_PROTOCOL_LOGGER_EVENT) ? "->" : "<-";
  const char *iface = wl_resource_get_class(msg->resource);
  uint32_t id = wl_resource_get_id(msg->resource);

  fprintf(protocol_log_file, "[%s.%03ld] %s %s@%u.%s(", timestamp,
          ts.tv_nsec / 1000000, dir, iface ? iface : "?", id,
          msg->message->name);

  const char *sig = msg->message->signature;
  int ai = 0;
  for (const char *p = sig; *p && ai < msg->arguments_count; p++) {
    if (*p >= '0' && *p <= '9')
      continue;
    if (*p == '?')
      continue;
    if (ai > 0)
      fprintf(protocol_log_file, ", ");
    switch (*p) {
    case 'i':
      fprintf(protocol_log_file, "%d", msg->arguments[ai].i);
      break;
    case 'u':
      fprintf(protocol_log_file, "%u", msg->arguments[ai].u);
      break;
    case 'f': {
      double v = wl_fixed_to_double(msg->arguments[ai].f);
      fprintf(protocol_log_file, "%.4f", v);
      break;
    }
    case 's':
      fprintf(protocol_log_file, "\"%s\"",
              msg->arguments[ai].s ? msg->arguments[ai].s : "(null)");
      break;
    case 'o':
      if (msg->arguments[ai].o) {
        struct wl_resource *r = (struct wl_resource *)msg->arguments[ai].o;
        const char *rc = wl_resource_get_class(r);
        fprintf(protocol_log_file, "%s@%u", rc ? rc : "?",
                wl_resource_get_id(r));
      } else {
        fprintf(protocol_log_file, "nil");
      }
      break;
    case 'n':
      fprintf(protocol_log_file, "new@%u", msg->arguments[ai].n);
      break;
    case 'a':
      fprintf(protocol_log_file, "array[%d]",
              msg->arguments[ai].a ? (int)msg->arguments[ai].a->size : 0);
      break;
    case 'h':
      fprintf(protocol_log_file, "fd=%d", msg->arguments[ai].h);
      break;
    default:
      fprintf(protocol_log_file, "?");
      break;
    }
    ai++;
  }

  fprintf(protocol_log_file, ")\n");
  fflush(protocol_log_file);
}

static void init_protocol_logger(void) {
  if (!bridge || !bridge->display)
    return;
  if (!bridge_env_flag_enabled("PLEXY_PROTOCOL_DEBUG"))
    return;

  const char *log_dir = getenv("PLEXY_LOG_DIR");
  if (!log_dir)
    log_dir = "logs";
  char path[512];
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  struct tm *tm = localtime(&ts.tv_sec);
  snprintf(path, sizeof(path),
           "%s/wayland-protocol-%04d%02d%02d-%02d%02d%02d.log", log_dir,
           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
           tm->tm_min, tm->tm_sec);
  protocol_log_file = fopen(path, "w");
  if (!protocol_log_file) {
    LOG_WARN("Failed to open protocol log: %s", path);
    return;
  }
  fprintf(protocol_log_file, "=== Wayland Protocol Debug Log ===\n");
  fprintf(
      protocol_log_file,
      "Legend: -> = event (server→client), <- = request (client→server)\n\n");
  fflush(protocol_log_file);

  protocol_logger = wl_display_add_protocol_logger(bridge->display,
                                                   protocol_log_callback, NULL);
  bridge_log("Protocol debug logging enabled: %s", path);
}

static bool start_xwayland(void) {
  const char *wayland_display = getenv("WAYLAND_DISPLAY");
  if (!wayland_display) {
    LOG_ERROR("Cannot start Xwayland: WAYLAND_DISPLAY not set");
    return false;
  }

  const char *xwayland_bin = getenv("PLEXY_XWAYLAND_PATH");
  static char resolved[512];

  if (!xwayland_bin || access(xwayland_bin, X_OK) != 0) {
    xwayland_bin = NULL;

    char self[512] = {0};
    ssize_t len = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (len > 0) {
      self[len] = '\0';

      char *slash = strrchr(self, '/');
      if (slash)
        *slash = '\0';

      static const char *rel_paths[] = {
          "/bin/Xwayland", "/xserver/build-xwayland/hw/xwayland/Xwayland",
          NULL};
      for (const char **rp = rel_paths; *rp; rp++) {
        snprintf(resolved, sizeof(resolved), "%s%s", self, *rp);
        if (access(resolved, X_OK) == 0) {
          xwayland_bin = resolved;
          break;
        }
      }
    }
  }

  if (!xwayland_bin) {

    const char *path_env = getenv("PATH");
    if (path_env) {
      char pathbuf[4096];
      snprintf(pathbuf, sizeof(pathbuf), "%s", path_env);
      char *saveptr = NULL;
      for (char *dir = strtok_r(pathbuf, ":", &saveptr); dir;
           dir = strtok_r(NULL, ":", &saveptr)) {
        snprintf(resolved, sizeof(resolved), "%s/Xwayland", dir);
        if (access(resolved, X_OK) == 0) {
          xwayland_bin = resolved;
          break;
        }
      }
    }
    if (!xwayland_bin) {
      LOG_WARN("Xwayland not found in PATH - X11 apps won't work");
      return false;
    }
  }
  bridge_log("Using Xwayland: %s", xwayland_bin);

  int display_num = 0;
  char lock_path[64];
  while (display_num < 100) {
    snprintf(lock_path, sizeof(lock_path), "/tmp/.X%d-lock", display_num);
    if (access(lock_path, F_OK) != 0) {
      break;
    }
    display_num++;
  }

  if (display_num >= 100) {
    LOG_ERROR("No available X display numbers (0-99 all in use)");
    return false;
  }

  char display_str[16];
  snprintf(display_str, sizeof(display_str), ":%d", display_num);

  int dpi = 96;
  if (bridge && bridge->plexy_conn) {
    const float scale = plexy_get_ui_scale(bridge->plexy_conn);
    if (scale > 0.0f) {
      const float scaled_dpi = 96.0f * scale;
      dpi = (int)(scaled_dpi + 0.5f);
      if (dpi < 96) {
        dpi = 96;
      }
    }
  }
  char dpi_str[16];
  snprintf(dpi_str, sizeof(dpi_str), "%d", dpi);

  bridge_log("Starting Xwayland on display %s (dpi=%s)", display_str, dpi_str);

  int wm_fd[2] = {-1, -1};
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, wm_fd) != 0) {
    LOG_ERROR("Failed to create WM socketpair: %s", strerror(errno));
    return false;
  }

  pid_t pid = fork();
  if (pid < 0) {
    LOG_ERROR("Failed to fork for Xwayland: %s", strerror(errno));
    close(wm_fd[0]);
    close(wm_fd[1]);
    return false;
  }

  if (pid == 0) {

    close(wm_fd[0]);

    const char *xwayland_debug = getenv("PLEXY_XWAYLAND_DEBUG");
    if (PLEXY_XWAYLAND_WIRE_DEBUG ||
        (xwayland_debug && xwayland_debug[0] && xwayland_debug[0] != '0')) {
      setenv("WAYLAND_DEBUG", "1", 1);
      freopen("/tmp/xwayland-debug.log", "w", stderr);
    }

    char wm_fd_str[16];
    snprintf(wm_fd_str, sizeof(wm_fd_str), "%d", wm_fd[1]);

    execl(xwayland_bin, "Xwayland", display_str, "-rootless", "-wm", wm_fd_str,
          "-dpi", dpi_str, "-noreset", "-ac", "-nolisten", "tcp",
          "-noTouchPointerEmulation", "-deferglyphs", "all", "-extension",
          "MIT-SCREEN-SAVER", "-extension", "DPMS", NULL);

    fprintf(stderr, "Failed to exec Xwayland: %s\n", strerror(errno));
    _exit(127);
  }

  close(wm_fd[1]);
  xwayland_wm_fd = wm_fd[0];
  xwayland_pid = pid;
  xwayland_display_num = display_num;

  setenv("DISPLAY", display_str, 1);

  const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
  if (runtime_dir) {
    char display_file[512];
    snprintf(display_file, sizeof(display_file), "%s/xwayland-display",
             runtime_dir);
    FILE *f = fopen(display_file, "w");
    if (f) {
      fprintf(f, "export DISPLAY=%s\n", display_str);
      fclose(f);
      chmod(display_file, 0644);
      bridge_log("DISPLAY=%s written to %s", display_str, display_file);
    }
  }

  struct timespec sleep_time = {0, 500000000};
  nanosleep(&sleep_time, NULL);

  int status;
  pid_t result = waitpid(pid, &status, WNOHANG);
  if (result == pid) {
    LOG_ERROR("Xwayland exited immediately (status %d)", status);
    xwayland_pid = -1;
    xwayland_display_num = -1;
    return false;
  }

  bridge_log("Xwayland started (PID %d) - X11 apps can use DISPLAY=%s", pid,
             display_str);
  return true;
}

static void stop_xwayland(void) {

  if (xwm) {
    xwm_destroy(xwm);
    xwm = NULL;
  }

  if (xwayland_pid > 0) {
    bridge_log("Stopping Xwayland (PID %d)", xwayland_pid);
    kill(xwayland_pid, SIGTERM);

    int status;
    for (int i = 0; i < 10; i++) {
      pid_t result = waitpid(xwayland_pid, &status, WNOHANG);
      if (result == xwayland_pid) {
        bridge_log("Xwayland terminated");
        xwayland_pid = -1;
        xwayland_display_num = -1;
        return;
      }
      struct timespec wait_time = {0, 50000000};
      nanosleep(&wait_time, NULL);
    }

    kill(xwayland_pid, SIGKILL);
    waitpid(xwayland_pid, &status, 0);
    bridge_log("Xwayland force-killed");
    xwayland_pid = -1;
    xwayland_display_num = -1;
  }
}

static void restart_xwayland(void) {
  if (!bridge || !bridge->running) {
    xwayland_restart_pending = false;
    return;
  }

  if (xwayland_pid > 0 || xwayland_wl_client != NULL) {
    xwayland_restart_pending = false;
    return;
  }

  LOG_WARN("Xwayland died unexpectedly; restarting...");
  stop_xwayland();

  if (!start_xwayland()) {
    LOG_ERROR("Failed to restart Xwayland");
    xwayland_restart_pending = false;
    return;
  }

  if (!start_xwm_async()) {
    LOG_ERROR("Failed to restart XWM after Xwayland restart");
    stop_xwayland();
    xwayland_restart_pending = false;
    return;
  }

  bridge_log("Xwayland restart initiated");
  xwayland_restart_pending = false;
}

static void on_xwayland_client_destroy(struct wl_listener *listener,
                                       void *data) {
  (void)data;

  bridge_log("Xwayland Wayland client disconnected");
  xwayland_wl_client = NULL;

  wl_list_remove(&listener->link);
  free(listener);

  if (!xwayland_restart_pending) {
    xwayland_restart_pending = true;
    restart_xwayland();
  }
}

static int xwayland_sigchld_handler(int signal_number, void *data) {
  (void)signal_number;
  (void)data;

  int status;
  pid_t pid;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (pid == xwayland_pid) {
      bridge_log("Xwayland process %d exited (status %d)", pid, status);
      xwayland_pid = -1;
      xwayland_display_num = -1;

      if (!xwayland_restart_pending) {
        xwayland_restart_pending = true;
        restart_xwayland();
      }
    }
  }
  return 0;
}

static bool start_xwm_async(void) {
  if (xwayland_display_num < 0) {
    LOG_ERROR("Cannot start XWM: Xwayland not running");
    return false;
  }

  xwm = xwm_create_async(bridge, xwayland_display_num);
  if (!xwm) {
    LOG_ERROR("Failed to create XWM (allocation failure)");
    return false;
  }

  bridge_log(
      "XWM async connection initiated for display :%d - will retry until ready",
      xwayland_display_num);
  return true;
}

bool bridge_is_xwayland_client(struct wl_client *client) {
  if (xwayland_pid <= 0 || !client)
    return false;

  pid_t pid;
  wl_client_get_credentials(client, &pid, NULL, NULL);
  return pid == xwayland_pid;
}

struct wl_client *bridge_get_xwayland_client(void) {
  return xwayland_wl_client;
}

int bridge_get_xwayland_wm_fd(void) {
  int fd = xwayland_wm_fd;
  xwayland_wm_fd = -1;
  return fd;
}

static void bridge_client_created(struct wl_listener *listener, void *data) {
  (void)listener;
  struct wl_client *client = data;

  if (bridge_is_xwayland_client(client)) {
    xwayland_wl_client = client;
    bridge_log("Xwayland client connected (PID %d)", xwayland_pid);

    wl_client_set_max_buffer_size(client, 4 * 1024 * 1024);

    struct wl_listener *destroy_listener = calloc(1, sizeof(*destroy_listener));
    if (destroy_listener) {
      destroy_listener->notify = on_xwayland_client_destroy;
      wl_client_add_destroy_listener(client, destroy_listener);
    } else {
      LOG_ERROR("Failed to allocate Xwayland client destroy listener");
    }
  }
}

static struct wl_listener client_created_listener = {
    .notify = bridge_client_created,
};

void bridge_notify_surface_created(struct bridge_surface *surface) {
  if (!surface)
    return;

  if (!xwm_is_ready(xwm))
    return;

  struct wl_client *client = wl_resource_get_client(surface->resource);
  if (!bridge_is_xwayland_client(client))
    return;

  uint32_t surface_id = wl_resource_get_id(surface->resource);
  xwm_surface_created(xwm, surface, surface_id);
}

static void signal_handler(int sig) {
  if (bridge && bridge->display) {
    bridge_log("Received signal %d, shutting down...", sig);
    bridge->running = false;
    wl_display_terminate(bridge->display);
  }
}

struct bridge_surface *bridge_surface_create(struct wl_resource *resource) {
  struct bridge_surface *surface = calloc(1, sizeof(*surface));
  if (!surface)
    return NULL;

  surface->resource = resource;
  surface->magic = 0xBD5FACE0;
  wl_signal_init(&surface->commit_signal);
  surface->xdg_toplevel_decoration = NULL;
  surface->xdg_decoration_mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
  surface->icon_name = NULL;
  surface->title = NULL;
  surface->buffer_scale = 1;
  surface->configure_serial = 0;
  surface->configured = false;
  wl_list_init(&surface->frame_callbacks);
  wl_list_init(&surface->presentation_feedbacks);
  wl_list_init(&surface->previous_buffer_destroy_listener.link);
  surface->previous_buffer_destroy_listener.notify = NULL;

  wl_list_init(&surface->current_buffer_destroy_listener.link);
  surface->current_buffer_destroy_listener.notify = NULL;

  wl_list_init(&surface->pending_buffer_destroy_listener.link);
  surface->pending_buffer_destroy_listener.notify = NULL;

  wl_list_init(&surface->subsurfaces);
  wl_list_init(&surface->subsurface_link);
  wl_list_init(&surface->client_destroy_listener.link);
  surface->is_subsurface = false;
  surface->subsurface_parent = NULL;
  surface->toplevel_parent = NULL;
  surface->subsurface_x = 0;
  surface->subsurface_y = 0;
  surface->subsurface_sync = true;
  surface->input_region_set = false;
  surface->opaque_region_set = false;
  surface->requested_cursor_shape = 1;
  surface->requested_cursor_visible = true;
  surface->pointer_lock_cursor_hidden = false;

  wl_list_insert(&bridge->surfaces, &surface->link);
  wl_resource_set_user_data(resource, surface);

  LOG_DEBUG("Surface created: %p", (void *)surface);

  return surface;
}

void bridge_surface_destroy(struct bridge_surface *surface) {
  if (!surface)
    return;

  LOG_DEBUG("Surface destroy: %p", (void *)surface);

  struct wl_resource *saved_resource = surface->resource;
  surface->resource = NULL;

  struct wl_resource *callback, *tmp;
  wl_resource_for_each_safe(callback, tmp, &surface->frame_callbacks) {

    wl_list_remove(wl_resource_get_link(callback));
    wl_list_init(wl_resource_get_link(callback));
  }

  bridge_surface_clear_presentation_feedbacks(surface);

  if (surface->previous_buffer) {
    wl_list_remove(&surface->previous_buffer_destroy_listener.link);
    surface->previous_buffer = NULL;
  }

  if (surface->current_buffer) {
    wl_list_remove(&surface->current_buffer_destroy_listener.link);
    surface->current_buffer = NULL;
  }

  if (surface->pending_buffer) {
    wl_list_remove(&surface->pending_buffer_destroy_listener.link);
    surface->pending_buffer = NULL;
  }

  if (surface->front_plexy_buffer) {
    if (bridge->running)
      plexy_destroy_buffer(surface->front_plexy_buffer);
    surface->front_plexy_buffer = NULL;
  }
  if (surface->back_plexy_buffer) {
    if (bridge->running)
      plexy_destroy_buffer(surface->back_plexy_buffer);
    surface->back_plexy_buffer = NULL;
  }

  if (surface->plexy_window) {
    LOG_DEBUG("Destroying Plexy window for surface %p", (void *)surface);
    PlexyWindow *window = surface->plexy_window;
    if (bridge->running)
      plexy_destroy_window(window);
    bridge_surface_clear_plexy_window(surface);
  }

  if (bridge->pointer_focus == surface) {
    LOG_TRACE("Clearing pointer focus");
    bridge->pointer_focus = NULL;
  }
  if (bridge->seat_pointer_surface == saved_resource) {

    if (saved_resource) {
      struct ptr_leave_ctx {
        struct wl_resource *surf;
        uint32_t serial;
      } plc = {
          .surf = saved_resource,
          .serial = wl_display_next_serial(bridge->display),
      };
      void ptr_leave_cb(struct wl_resource * ptr, void *data) {
        struct ptr_leave_ctx *ctx = data;
        wl_pointer_send_leave(ptr, ctx->serial, ctx->surf);
        if (wl_resource_get_version(ptr) >= WL_POINTER_FRAME_SINCE_VERSION)
          wl_pointer_send_frame(ptr);
      }
      bridge_for_each_pointer(surface, ptr_leave_cb, &plc);
    }
    bridge->seat_pointer_surface = NULL;
    bridge->seat_pointer_resource = NULL;
  }
  if (bridge->keyboard_focus == surface) {
    LOG_TRACE("Clearing keyboard focus");
    if (saved_resource) {
      struct kbd_leave_ctx {
        struct wl_resource *surf;
        uint32_t serial;
      } klc = {
          .surf = saved_resource,
          .serial = wl_display_next_serial(bridge->display),
      };
      void kbd_leave_cb(struct wl_resource * kbd, void *data) {
        struct kbd_leave_ctx *ctx = data;
        wl_keyboard_send_leave(kbd, ctx->serial, ctx->surf);
      }
      bridge_for_each_keyboard(surface, kbd_leave_cb, &klc);
    }
    bridge->keyboard_focus = NULL;
  }

  free(surface->icon_name);
  free(surface->title);
  free(surface->app_id);
  free(surface->input_region.ops);
  surface->input_region.ops = NULL;
  surface->input_region.count = 0;
  surface->input_region.capacity = 0;
  free(surface->opaque_region.ops);
  surface->opaque_region.ops = NULL;
  surface->opaque_region.count = 0;
  surface->opaque_region.capacity = 0;

  if (surface->is_subsurface && surface->subsurface_parent) {
    wl_list_remove(&surface->subsurface_link);
  }

  struct bridge_surface *child, *child_tmp;
  wl_list_for_each_safe(child, child_tmp, &surface->subsurfaces,
                        subsurface_link) {
    child->subsurface_parent = NULL;
    wl_list_remove(&child->subsurface_link);
    wl_list_init(&child->subsurface_link);
  }

  struct bridge_surface *other;
  wl_list_for_each(other, &bridge->surfaces, link) {
    if (other != surface && other->toplevel_parent == surface) {
      other->toplevel_parent = NULL;
    }
  }

  wl_list_remove(&surface->link);
  surface->magic = 0xDEAD;
  free(surface);
}

struct bridge_surface *
bridge_surface_from_resource(struct wl_resource *resource) {
  if (!resource)
    return NULL;
  return wl_resource_get_user_data(resource);
}

void bridge_surface_set_plexy_window(struct bridge_surface *surface,
                                     PlexyWindow *window) {
  if (!surface)
    return;

  if (bridge && bridge->cached_window_id_surface == surface) {
    bridge->cached_window_id_surface = NULL;
    bridge->cached_window_id = 0;
  }

  surface->plexy_window = window;
  surface->plexy_window_id = window ? plexy_window_get_id(window) : 0;

  if (bridge && window) {
    bridge->cached_window_id_surface = surface;
    bridge->cached_window_id = surface->plexy_window_id;
  }
}

void bridge_surface_clear_plexy_window(struct bridge_surface *surface) {
  bridge_surface_set_plexy_window(surface, NULL);
}

static void bridge_surface_send_cursor_shape(struct bridge_surface *surface,
                                             uint32_t shape) {
  if (!bridge || !bridge->plexy_conn || !surface ||
      surface->plexy_window_id == 0) {
    return;
  }

  plexy_set_cursor_shape(bridge->plexy_conn, surface->plexy_window_id, shape);
}

static uint32_t
bridge_surface_effective_cursor_shape(const struct bridge_surface *surface) {
  if (!surface) {
    return 0;
  }

  if (surface->pointer_lock_cursor_hidden ||
      !surface->requested_cursor_visible) {
    return 0;
  }

  return surface->requested_cursor_shape ? surface->requested_cursor_shape : 1;
}

void bridge_surface_update_cursor_shape(struct bridge_surface *surface,
                                        uint32_t shape) {
  if (!surface) {
    return;
  }

  if (shape == 0) {
    surface->requested_cursor_visible = false;
  } else {
    surface->requested_cursor_shape = shape;
    surface->requested_cursor_visible = true;
  }

  bridge_surface_send_cursor_shape(
      surface, bridge_surface_effective_cursor_shape(surface));
}

void bridge_surface_set_pointer_lock_cursor_hidden(
    struct bridge_surface *surface, bool hidden) {
  if (!surface || surface->pointer_lock_cursor_hidden == hidden) {
    return;
  }

  surface->pointer_lock_cursor_hidden = hidden;
  bridge_surface_send_cursor_shape(
      surface, bridge_surface_effective_cursor_shape(surface));
}

struct bridge_surface *bridge_surface_from_window_id(uint32_t window_id) {
  if (!bridge || window_id == 0)
    return NULL;

  if (bridge->cached_window_id_surface &&
      bridge->cached_window_id == window_id &&
      bridge->cached_window_id_surface->plexy_window_id == window_id) {
    return bridge->cached_window_id_surface;
  }

  struct bridge_surface *surface;
  int count = 0;
  wl_list_for_each(surface, &bridge->surfaces, link) {
    count++;
    if (surface->plexy_window_id == window_id) {
      bridge->cached_window_id_surface = surface;
      bridge->cached_window_id = window_id;
      LOG_TRACE("from_window_id(%u): found surface=%p (is_x11=%d) after "
                "checking %d surfaces",
                window_id, (void *)surface, surface->is_x11, count);
      return surface;
    }
  }
  if (bridge->cached_window_id == window_id) {
    bridge->cached_window_id_surface = NULL;
    bridge->cached_window_id = 0;
  }
  LOG_TRACE("from_window_id(%u): NOT FOUND after checking %d surfaces",
            window_id, count);
  return NULL;
}

void bridge_weston_surface_created(struct wayland_bridge *bridge_ptr,
                                   struct weston_surface *weston_surface) {
  LOG_DEBUG("Weston surface created, will be handled by protocol bindings");
}

static void on_raw_pointer_motion(PlexyConnection *conn, int32_t screen_x,
                                  int32_t screen_y, double dx, double dy,
                                  uint32_t window_id, void *ud) {
  (void)conn;
  (void)ud;
  bridge_raw_pointer_motion(screen_x, screen_y, dx, dy, window_id);
}

static void on_raw_pointer_button(PlexyConnection *conn, uint32_t button,
                                  uint32_t state, void *ud) {
  (void)conn;
  (void)ud;
  bridge_raw_pointer_button(button, state);
}

static void on_raw_pointer_axis(PlexyConnection *conn, int32_t axis,
                                int32_t value, int32_t discrete, void *ud) {
  (void)conn;
  (void)ud;
  bridge_raw_pointer_axis(axis, value, discrete);
}

static void on_window_position(PlexyConnection *conn, uint32_t window_id,
                               int32_t screen_x, int32_t screen_y,
                               int32_t width, int32_t height, uint8_t visible,
                               void *ud) {
  (void)conn;
  (void)ud;
  struct bridge_surface *s = bridge_surface_from_window_id(window_id);
  if (s) {
    s->screen_x = screen_x;
    s->screen_y = screen_y;
    s->screen_width = width;
    s->screen_height = height;
    s->screen_pos_valid = visible ? true : false;
    LOG_DEBUG("on_window_position: wid=%u pos=(%d,%d) size=(%d,%d) vis=%d",
              window_id, screen_x, screen_y, width, height, visible);

    if (s->is_x11 && s->xwm_window) {
      struct xwm_window *xwm_win = (struct xwm_window *)s->xwm_window;

      if (!xwm_win->override_redirect) {
        extern void xwm_window_configure(struct xwm_window * window, int32_t x,
                                         int32_t y, int32_t width,
                                         int32_t height);
        xwm_window_configure(xwm_win, screen_x, screen_y, width, height);
      }
    }
    return;
  }
  LOG_DEBUG("on_window_position: wid=%u NOT FOUND in surface list", window_id);
}

void bridge_enable_raw_input(void) {
  if (!bridge || !bridge->plexy_conn)
    return;
  if (bridge->raw_input_active)
    return;
  bridge->raw_input_active = true;
  plexy_set_raw_pointer_motion_callback(bridge->plexy_conn,
                                        on_raw_pointer_motion, NULL);
  plexy_set_raw_pointer_button_callback(bridge->plexy_conn,
                                        on_raw_pointer_button, NULL);
  plexy_set_raw_pointer_axis_callback(bridge->plexy_conn, on_raw_pointer_axis,
                                      NULL);
  bridge_log("Raw input ENABLED (pointer constraints active)");
}

void bridge_disable_raw_input(void) {
  if (!bridge || !bridge->plexy_conn)
    return;
  if (!bridge->raw_input_active)
    return;
  bridge->raw_input_active = false;
  plexy_set_raw_pointer_motion_callback(bridge->plexy_conn, NULL, NULL);
  plexy_set_raw_pointer_button_callback(bridge->plexy_conn, NULL, NULL);
  plexy_set_raw_pointer_axis_callback(bridge->plexy_conn, NULL, NULL);
  bridge_log("Raw input DISABLED (per-window events resume)");
}

static int bridge_dispatch_plexy(int fd, uint32_t mask, void *data) {
  if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
    bridge_log("Compositor connection lost (mask=0x%x), shutting down...",
               mask);
    bridge->running = false;
    wl_display_terminate(bridge->display);
    return 0;
  }

  const uint64_t output_generation_before =
      bridge && bridge->plexy_conn
          ? plexy_get_output_generation(bridge->plexy_conn)
          : 0;

  int ret = plexy_dispatch(bridge->plexy_conn);
  if (ret <= 0) {
    bridge_log("Compositor disconnected (ret=%d), shutting down...", ret);
    bridge->running = false;
    wl_display_terminate(bridge->display);
    return 0;
  }

  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  while (plexy_dispatch(bridge->plexy_conn) > 0)
    ;
  fcntl(fd, F_SETFL, flags);

  const bool outputs_changed =
      bridge && bridge->plexy_conn &&
      plexy_get_output_generation(bridge->plexy_conn) !=
          output_generation_before;
  if (outputs_changed) {
    bridge_refresh_outputs();
  }

  wl_display_flush_clients(bridge->display);

  return 1;
}

static bool init_wayland_server(void) {
  bridge_log("Initializing Wayland server...");

  bridge->display = wl_display_create();
  if (!bridge->display) {
    LOG_ERROR("Failed to create Wayland display");
    return false;
  }

  const char *socket = wl_display_add_socket_auto(bridge->display);
  if (!socket) {
    LOG_ERROR("Failed to create Wayland socket");
    return false;
  }

  bridge_log("Wayland display socket: %s", socket);
  setenv("WAYLAND_DISPLAY", socket, 1);

  bridge->loop = wl_display_get_event_loop(bridge->display);

  init_protocol_logger();

  wl_display_add_client_created_listener(bridge->display,
                                         &client_created_listener);

  struct wl_event_source *sigchld_source = wl_event_loop_add_signal(
      bridge->loop, SIGCHLD, xwayland_sigchld_handler, NULL);
  if (!sigchld_source) {
    LOG_WARN(
        "Failed to register SIGCHLD handler; Xwayland zombies may persist");
  }

  bridge_log("Registering Wayland protocols...");

  bridge->compositor_global = wl_global_create(
      bridge->display, &wl_compositor_interface, 6, NULL, bind_compositor);
  bridge_log("  wl_compositor v6: %p", (void *)bridge->compositor_global);

  bridge->subcompositor_global =
      wl_global_create(bridge->display, &wl_subcompositor_interface, 1, NULL,
                       bind_subcompositor);
  bridge_log("  wl_subcompositor v1: %p", (void *)bridge->subcompositor_global);

  bridge->shm_global =
      wl_global_create(bridge->display, &wl_shm_interface, 2, NULL, bind_shm);
  bridge_log("  wl_shm v2: %p", (void *)bridge->shm_global);

  bridge->xdg_wm_base_global = wl_global_create(
      bridge->display, &xdg_wm_base_interface, 6, NULL, bind_xdg_wm_base);
  bridge_log("  xdg_wm_base v6: %p", (void *)bridge->xdg_wm_base_global);

  bridge->seat_global =
      wl_global_create(bridge->display, &wl_seat_interface, 7, NULL, bind_seat);
  bridge_log("  wl_seat v7: %p", (void *)bridge->seat_global);

  if (bridge->dmabuf_enabled) {
    bridge->dmabuf_global = wl_global_create(
        bridge->display, &zwp_linux_dmabuf_v1_interface, 4, NULL, bind_dmabuf);
    bridge_log("  zwp_linux_dmabuf_v1 v4: %p", (void *)bridge->dmabuf_global);
  } else {
    bridge->dmabuf_global = NULL;
    bridge_log("  zwp_linux_dmabuf_v1: disabled (SHM-only mode)");
  }

  bridge->data_device_manager_global =
      wl_global_create(bridge->display, &wl_data_device_manager_interface, 3,
                       NULL, bind_data_device_manager);
  bridge_log("  wl_data_device_manager v3: %p",
             (void *)bridge->data_device_manager_global);

  bridge->text_input_manager_global =
      wl_global_create(bridge->display, &zwp_text_input_manager_v3_interface, 1,
                       NULL, bind_text_input_manager);
  bridge_log("  zwp_text_input_manager_v3 v1: %p",
             (void *)bridge->text_input_manager_global);

  struct wl_global *primary_selection_global = wl_global_create(
      bridge->display, &zwp_primary_selection_device_manager_v1_interface, 1,
      NULL, bind_primary_selection_device_manager);
  bridge_log("  zwp_primary_selection_device_manager_v1 v1: %p",
             (void *)primary_selection_global);

  struct wl_global *data_control_global =
      wl_global_create(bridge->display, &ext_data_control_manager_v1_interface,
                       1, NULL, bind_data_control_manager);
  bridge_log("  ext_data_control_manager_v1 v1: %p",
             (void *)data_control_global);

  struct wl_global *xdg_output_global =
      wl_global_create(bridge->display, &zxdg_output_manager_v1_interface, 3,
                       NULL, bind_xdg_output_manager);
  bridge_log("  zxdg_output_manager_v1 v3: %p", (void *)xdg_output_global);

  struct wl_global *viewporter_global = wl_global_create(
      bridge->display, &wp_viewporter_interface, 1, NULL, bind_viewporter);
  bridge_log("  wp_viewporter v1: %p", (void *)viewporter_global);

  struct wl_global *pointer_constraints_global =
      wl_global_create(bridge->display, &zwp_pointer_constraints_v1_interface,
                       1, NULL, bind_pointer_constraints);
  bridge_log("  zwp_pointer_constraints_v1 v1: %p",
             (void *)pointer_constraints_global);

  struct wl_global *relative_pointer_global = wl_global_create(
      bridge->display, &zwp_relative_pointer_manager_v1_interface, 1, NULL,
      bind_relative_pointer_manager);
  bridge_log("  zwp_relative_pointer_manager_v1 v1: %p",
             (void *)relative_pointer_global);

  struct wl_global *fractional_scale_global = wl_global_create(
      bridge->display, &wp_fractional_scale_manager_v1_interface, 1, NULL,
      bind_fractional_scale_manager);
  bridge_log("  wp_fractional_scale_manager_v1 v1: %p",
             (void *)fractional_scale_global);

  struct wl_global *xdg_activation_global =
      wl_global_create(bridge->display, &xdg_activation_v1_interface, 1, NULL,
                       bind_xdg_activation);
  bridge_log("  xdg_activation_v1 v1: %p", (void *)xdg_activation_global);

  struct wl_global *xdg_decoration_global =
      wl_global_create(bridge->display, &zxdg_decoration_manager_v1_interface,
                       1, NULL, bind_xdg_decoration_manager);
  bridge_log("  zxdg_decoration_manager_v1 v1: %p",
             (void *)xdg_decoration_global);

  struct wl_global *xdg_toplevel_icon_global =
      wl_global_create(bridge->display, &xdg_toplevel_icon_manager_v1_interface,
                       1, NULL, bind_xdg_toplevel_icon_manager);
  bridge_log("  xdg_toplevel_icon_manager_v1 v1: %p",
             (void *)xdg_toplevel_icon_global);

  struct wl_global *presentation_global = wl_global_create(
      bridge->display, &wp_presentation_interface, 1, NULL, bind_presentation);
  bridge_log("  wp_presentation v1: %p", (void *)presentation_global);

  struct wl_global *cursor_shape_global =
      wl_global_create(bridge->display, &wp_cursor_shape_manager_v1_interface,
                       1, NULL, bind_cursor_shape_manager);
  bridge_log("  wp_cursor_shape_manager_v1 v1: %p",
             (void *)cursor_shape_global);

  struct wl_global *pointer_gestures_global =
      wl_global_create(bridge->display, &zwp_pointer_gestures_v1_interface, 3,
                       NULL, bind_pointer_gestures);
  bridge_log("  zwp_pointer_gestures_v1 v3: %p",
             (void *)pointer_gestures_global);

  struct wl_global *idle_inhibit_global =
      wl_global_create(bridge->display, &zwp_idle_inhibit_manager_v1_interface,
                       1, NULL, bind_idle_inhibit_manager);
  bridge_log("  zwp_idle_inhibit_manager_v1 v1: %p",
             (void *)idle_inhibit_global);

  struct wl_global *single_pixel_buffer_global = wl_global_create(
      bridge->display, &wp_single_pixel_buffer_manager_v1_interface, 1, NULL,
      bind_single_pixel_buffer_manager);
  bridge_log("  wp_single_pixel_buffer_manager_v1 v1: %p",
             (void *)single_pixel_buffer_global);

  struct wl_global *keyboard_shortcuts_inhibit_global = wl_global_create(
      bridge->display, &zwp_keyboard_shortcuts_inhibit_manager_v1_interface, 1,
      NULL, bind_keyboard_shortcuts_inhibit_manager);
  bridge_log("  zwp_keyboard_shortcuts_inhibit_manager_v1 v1: %p",
             (void *)keyboard_shortcuts_inhibit_global);

  struct wl_global *content_type_global =
      wl_global_create(bridge->display, &wp_content_type_manager_v1_interface,
                       1, NULL, bind_content_type_manager);
  bridge_log("  wp_content_type_manager_v1 v1: %p",
             (void *)content_type_global);

  struct wl_global *tearing_control_global = wl_global_create(
      bridge->display, &wp_tearing_control_manager_v1_interface, 1, NULL,
      bind_tearing_control_manager);
  bridge_log("  wp_tearing_control_manager_v1 v1: %p",
             (void *)tearing_control_global);

  struct wl_global *xdg_exporter_global = wl_global_create(
      bridge->display, &zxdg_exporter_v2_interface, 1, NULL, bind_xdg_exporter);
  bridge_log("  zxdg_exporter_v2 v1: %p", (void *)xdg_exporter_global);

  struct wl_global *xdg_importer_global = wl_global_create(
      bridge->display, &zxdg_importer_v2_interface, 1, NULL, bind_xdg_importer);
  bridge_log("  zxdg_importer_v2 v1: %p", (void *)xdg_importer_global);

  struct wl_global *foreign_toplevel_list_global =
      wl_global_create(bridge->display, &ext_foreign_toplevel_list_v1_interface,
                       1, NULL, bind_foreign_toplevel_list);
  bridge_log("  ext_foreign_toplevel_list_v1 v1: %p",
             (void *)foreign_toplevel_list_global);

  struct wl_global *toplevel_drag_global =
      wl_global_create(bridge->display, &xdg_toplevel_drag_manager_v1_interface,
                       1, NULL, bind_toplevel_drag_manager);
  bridge_log("  xdg_toplevel_drag_manager_v1 v1: %p",
             (void *)toplevel_drag_global);

  struct wl_global *alpha_modifier_global =
      wl_global_create(bridge->display, &wp_alpha_modifier_v1_interface, 1,
                       NULL, bind_alpha_modifier);
  bridge_log("  wp_alpha_modifier_v1 v1: %p", (void *)alpha_modifier_global);

  struct wl_global *input_method_mgr_global =
      wl_global_create(bridge->display, &xx_input_method_manager_v2_interface,
                       4, NULL, bind_input_method_manager);
  bridge_log("  xx_input_method_manager_v2 v4: %p",
             (void *)input_method_mgr_global);

  struct wl_global *idle_notifier_global =
      wl_global_create(bridge->display, &ext_idle_notifier_v1_interface, 2,
                       NULL, bind_idle_notifier);
  bridge_log("  ext_idle_notifier_v1 v2: %p", (void *)idle_notifier_global);

  struct wl_global *session_lock_mgr_global =
      wl_global_create(bridge->display, &ext_session_lock_manager_v1_interface,
                       1, NULL, bind_session_lock_manager);
  bridge_log("  ext_session_lock_manager_v1 v1: %p",
             (void *)session_lock_mgr_global);

  struct wl_global *xdg_wm_dialog_global =
      wl_global_create(bridge->display, &xdg_wm_dialog_v1_interface, 1, NULL,
                       bind_xdg_wm_dialog);
  bridge_log("  xdg_wm_dialog_v1 v1: %p", (void *)xdg_wm_dialog_global);

  struct wl_global *xwayland_kbd_grab_global = wl_global_create(
      bridge->display, &zwp_xwayland_keyboard_grab_manager_v1_interface, 1,
      NULL, bind_xwayland_keyboard_grab_manager);
  bridge_log("  zwp_xwayland_keyboard_grab_manager_v1 v1: %p",
             (void *)xwayland_kbd_grab_global);

  struct wl_global *input_timestamps_global = wl_global_create(
      bridge->display, &zwp_input_timestamps_manager_v1_interface, 1, NULL,
      bind_input_timestamps_manager);
  bridge_log("  zwp_input_timestamps_manager_v1 v1: %p",
             (void *)input_timestamps_global);

  struct wl_global *fifo_global =
      wl_global_create(bridge->display, &wp_fifo_manager_v1_interface, 1, NULL,
                       bind_fifo_manager);
  bridge_log("  wp_fifo_manager_v1 v1: %p", (void *)fifo_global);

  struct wl_global *drm_syncobj_global = wl_global_create(
      bridge->display, &wp_linux_drm_syncobj_manager_v1_interface, 1, NULL,
      bind_drm_syncobj_manager);
  bridge_log("  wp_linux_drm_syncobj_manager_v1 v1: %p",
             (void *)drm_syncobj_global);

  if (!bridge->compositor_global || !bridge->subcompositor_global ||
      !bridge->shm_global || !bridge->xdg_wm_base_global ||
      !bridge->seat_global) {
    LOG_ERROR("Failed to create Wayland globals");
    return false;
  }

  wl_display_flush_clients(bridge->display);

  bridge_log("Wayland server initialized successfully");
  return true;
}

static bool init_plexy_client(void) {
  bridge_log("Connecting to PlexyShell...");

  const char *socket_path = getenv("PLEXY_SOCKET");
  if (!socket_path) {
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (runtime_dir && strlen(runtime_dir) > 0) {
      static char path_buffer[256];
      snprintf(path_buffer, sizeof(path_buffer), "%s/plexy.sock", runtime_dir);
      socket_path = path_buffer;
    } else {
      static char path_buffer[256];
      snprintf(path_buffer, sizeof(path_buffer), "/tmp/plexy-%d.sock",
               getuid());
      socket_path = path_buffer;
    }
  }

  if (access(socket_path, F_OK) != 0) {
    LOG_ERROR("Plexy socket not found: %s", socket_path);
    bridge_log("Is plexyshell running?");
    return false;
  }

  bridge_log("Using Plexy socket: %s", socket_path);

  bridge->plexy_conn = plexy_connect(NULL);
  if (!bridge->plexy_conn) {
    LOG_ERROR("Failed to connect to PlexyShell");
    return false;
  }

  bridge->plexy_fd = plexy_get_fd(bridge->plexy_conn);
  if (bridge->plexy_fd < 0) {
    LOG_ERROR("Invalid Plexy FD: %d", bridge->plexy_fd);
    plexy_disconnect(bridge->plexy_conn);
    bridge->plexy_conn = NULL;
    return false;
  }

  bridge_log("Plexy connection FD: %d", bridge->plexy_fd);

  plexy_set_window_position_callback(bridge->plexy_conn, on_window_position,
                                     NULL);

  struct wl_event_source *plexy_source =
      wl_event_loop_add_fd(bridge->loop, bridge->plexy_fd, WL_EVENT_READABLE,
                           bridge_dispatch_plexy, NULL);

  if (!plexy_source) {
    LOG_ERROR("Failed to add Plexy FD to event loop");
    plexy_disconnect(bridge->plexy_conn);
    bridge->plexy_conn = NULL;
    return false;
  }

  plexy_dispatch_pending(bridge->plexy_conn);

  if (!bridge_create_output_globals()) {
    LOG_ERROR("Failed to create Wayland output globals");
    plexy_disconnect(bridge->plexy_conn);
    bridge->plexy_conn = NULL;
    return false;
  }

  uint32_t screen_w, screen_h;
  plexy_get_screen_size(bridge->plexy_conn, &screen_w, &screen_h);
  float scale = plexy_get_ui_scale(bridge->plexy_conn);
  bridge_log("Connected to PlexyShell - screen: %ux%u, scale: %.2f", screen_w,
             screen_h, scale);

  return true;
}

static int xdg_ping_timer_callback(void *data) {
  xdg_wm_base_send_pings();

  return 5000;
}

static void raise_fd_limit(void) {
  struct rlimit rl;
  if (getrlimit(RLIMIT_NOFILE, &rl) != 0) {
    LOG_WARN("Could not query RLIMIT_NOFILE: %s", strerror(errno));
    return;
  }

  if (rl.rlim_cur >= rl.rlim_max) {
    return;
  }

  rlim_t old_cur = rl.rlim_cur;
  rl.rlim_cur = rl.rlim_max;
  if (setrlimit(RLIMIT_NOFILE, &rl) != 0) {
    LOG_WARN("Could not raise RLIMIT_NOFILE from %ju to %ju: %s",
             (uintmax_t)old_cur, (uintmax_t)rl.rlim_max, strerror(errno));
  } else {
    bridge_log("Raised RLIMIT_NOFILE from %ju to %ju", (uintmax_t)old_cur,
               (uintmax_t)rl.rlim_max);
  }
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  char active_log_path[512] = "";

  const char *level_env = getenv("PLEXY_BRIDGE_LOG_LEVEL");
  BridgeLogLevel parsed_level = LOG_LEVEL_WARN;
  if (bridge_parse_log_level(level_env, &parsed_level)) {
    current_log_level = parsed_level;
  } else {
    current_log_level = (BridgeLogLevel)PLEXY_BRIDGE_DEFAULT_LOG_LEVEL;
  }

  setvbuf(stdout, NULL, _IOLBF, 0);

  const char *log_path_env = getenv("PLEXY_BRIDGE_LOG_FILE");
  if (log_path_env && log_path_env[0]) {
    log_file = fopen(log_path_env, "w");
    if (!log_file) {
      fprintf(stderr, "Could not open log file: %s\n", log_path_env);
    } else {
      setvbuf(log_file, NULL, _IOLBF, 0);
    }
  } else if (bridge_env_flag_enabled("PLEXY_BRIDGE_LOG_TO_FILE")) {
    char log_path[512];
    const char *log_dir = getenv("PLEXY_BRIDGE_LOG_DIR");
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", tm_info);
    if (!log_dir || !log_dir[0]) {
      log_dir = "logs";
    }
    snprintf(log_path, sizeof(log_path), "%s/wayland-bridge-%s.log", log_dir,
             timestamp);
    if (mkdir(log_dir, 0755) != 0 && errno != EEXIST) {
      fprintf(stderr, "Could not create log directory: %s (%s)\n", log_dir,
              strerror(errno));
    }
    log_file = fopen(log_path, "w");
    if (!log_file) {
      fprintf(stderr, "Could not open log file: %s\n", log_path);
    } else {
      setvbuf(log_file, NULL, _IOLBF, 0);
      snprintf(active_log_path, sizeof(active_log_path), "%s", log_path);
    }
  }

  printf("PlexyShell Wayland Bridge\n");
  printf("=========================\n");
  if (log_file) {
    printf("Log file: %s\n\n", active_log_path);
  } else {
    printf("Log file: disabled\n\n");
  }

  bridge_log("=== Wayland Bridge Starting ===");
  bridge_log("PID: %d", getpid());

  raise_fd_limit();

  bridge = calloc(1, sizeof(*bridge));
  if (!bridge) {
    LOG_ERROR("Failed to allocate bridge structure");
    if (log_file)
      fclose(log_file);
    return 1;
  }

  bridge_log("Bridge structure allocated");

  wl_list_init(&bridge->surfaces);
  wl_list_init(&bridge->seat_resources);
  wl_list_init(&bridge->pointer_clients);
  wl_list_init(&bridge->keyboard_clients);
  wl_list_init(&bridge->touch_clients);
  wl_list_init(&bridge->text_input_clients);
  bridge->active_input_method = NULL;
  bridge->running = true;

  const char *dmabuf_disable_env = getenv("PLEXY_DISABLE_DMABUF");
  const char *dmabuf_enable_env = getenv("PLEXY_ENABLE_DMABUF");
  bridge->dmabuf_enabled = true;
  if (dmabuf_disable_env && strcmp(dmabuf_disable_env, "1") == 0) {
    bridge->dmabuf_enabled = false;
    bridge_log("DMA-BUF disabled via PLEXY_DISABLE_DMABUF=1");
  } else if (dmabuf_enable_env && strcmp(dmabuf_enable_env, "0") == 0) {
    bridge->dmabuf_enabled = false;
    bridge_log("DMA-BUF disabled via PLEXY_ENABLE_DMABUF=0");
  }

  if (bridge->dmabuf_enabled) {
    if (bridge_dmabuf_init()) {
      bridge_log("DMA-BUF support enabled (default)");
    } else {
      bridge->dmabuf_enabled = false;
      bridge_log(
          "DMA-BUF initialization failed, falling back to SHM-only mode");
    }
  }

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGPIPE, SIG_IGN);
  bridge_log("Signal handlers installed");

  if (!init_wayland_server()) {
    free(bridge);
    bridge = NULL;
    if (log_file)
      fclose(log_file);
    return 1;
  }

  const char *use_weston_env = getenv("PLEXY_USE_WESTON");
  if (use_weston_env && strcmp(use_weston_env, "1") == 0) {
    bridge_log("Enabling Weston integration (PLEXY_USE_WESTON=1)");
    bridge->use_weston = true;
    bridge->weston = weston_bridge_init(bridge);
    if (!bridge->weston) {
      LOG_ERROR("Failed to initialize Weston bridge");
    }
  } else {
    bridge_log("Weston integration disabled (default)");
  }

  if (!init_plexy_client()) {
    wl_display_destroy(bridge->display);
    free(bridge);
    bridge = NULL;
    if (log_file)
      fclose(log_file);
    return 1;
  }

  struct wl_event_source *ping_timer = wl_event_loop_add_timer(
      bridge->loop, (wl_event_loop_timer_func_t)xdg_ping_timer_callback, NULL);
  if (ping_timer) {
    wl_event_source_timer_update(ping_timer, 5000);
    bridge_log("Added xdg_wm_base ping timer");
  }

  const char *skip_xwm = getenv("PLEXY_SKIP_XWM");
  if (!skip_xwm && start_xwayland()) {
    bridge_log("X11 applications supported via Xwayland");

    if (start_xwm_async()) {
      bridge_log("XWM async startup initiated - X11 windows will be visible "
                 "once connected");
    } else {
      LOG_WARN("XWM async startup failed - X11 windows will not display");
    }
  } else if (skip_xwm) {
    bridge_log("XWM disabled via PLEXY_SKIP_XWM environment variable");
  }

  bridge_log("=== Wayland bridge ready ===");
  bridge_log("Clients can connect with: WAYLAND_DISPLAY=%s",
             getenv("WAYLAND_DISPLAY"));
  bridge_log("Entering main event loop...");

  printf("\nWayland bridge ready!\n");
  printf("Clients can connect with: WAYLAND_DISPLAY=%s\n",
         getenv("WAYLAND_DISPLAY"));
  printf("\nPress Ctrl+C to stop.\n\n");

  wl_display_run(bridge->display);

  bridge_log("=== Event loop exited ===");
  bridge_log("Shutting down...");

  if (bridge)
    bridge->running = false;

  stop_xwayland();
  if (bridge->dmabuf_enabled) {
    bridge_dmabuf_cleanup();
  }

  if (bridge->weston) {
    weston_bridge_cleanup(bridge->weston);
    bridge->weston = NULL;
  }

  if (protocol_logger) {
    wl_protocol_logger_destroy(protocol_logger);
    protocol_logger = NULL;
  }

  wl_display_destroy(bridge->display);
  plexy_disconnect(bridge->plexy_conn);
  free(bridge);
  bridge = NULL;

  bridge_log("=== Wayland Bridge Stopped ===");

  if (protocol_log_file) {
    fclose(protocol_log_file);
    protocol_log_file = NULL;
  }
  if (log_file) {
    fclose(log_file);
    log_file = NULL;
  }

  return 0;
}
