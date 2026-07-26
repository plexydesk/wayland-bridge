/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#define _POSIX_C_SOURCE 200809L
#include "weston_compositor.h"
#include "wayland_bridge.h"

#include <libweston/backend-headless.h>
#include <libweston/libweston.h>
#include <libweston/weston-log.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct weston_bridge {
  struct weston_compositor *compositor;
  struct weston_log_context *log_ctx;
  struct weston_log_subscriber *logger;
  struct wl_listener surface_created_listener;
  struct wl_listener output_created_listener;
  struct weston_seat *seat;
  struct wayland_bridge *plexy_bridge;
};

static int bridge_weston_log(const char *fmt, va_list ap) {
  return vfprintf(stderr, fmt, ap);
}

static int bridge_weston_log_cont(const char *fmt, va_list ap) {
  return vfprintf(stderr, fmt, ap);
}

static void handle_surface_created(struct wl_listener *listener, void *data) {
  struct weston_bridge *wb =
      wl_container_of(listener, wb, surface_created_listener);
  struct weston_surface *surface = data;

  LOG_DEBUG("Weston: Surface created callback");
  bridge_weston_surface_created(wb->plexy_bridge, surface);
}

static void handle_output_created(struct wl_listener *listener, void *data) {
  struct weston_bridge *wb =
      wl_container_of(listener, wb, output_created_listener);
  struct weston_head *head = data;

  LOG_DEBUG("Weston: Output/head created");
}

static int weston_bridge_create_compositor(struct weston_bridge *wb) {
  struct weston_compositor *compositor;

  wb->log_ctx = weston_log_ctx_create();
  if (!wb->log_ctx) {
    LOG_ERROR("Weston: Failed to create log context");
    return -1;
  }

  weston_log_set_handler(bridge_weston_log, bridge_weston_log_cont);

  wb->logger = weston_log_subscriber_create_log(stderr);
  weston_log_subscribe(wb->log_ctx, wb->logger, "all");

  compositor = weston_compositor_create(wb->plexy_bridge->display, wb->log_ctx,
                                        wb, NULL);
  if (!compositor) {
    LOG_ERROR("Weston: Failed to create compositor");
    weston_log_subscriber_destroy(wb->logger);
    weston_log_ctx_destroy(wb->log_ctx);
    return -1;
  }

  wb->compositor = compositor;

  struct weston_headless_backend_config config = {{
      .struct_version = WESTON_HEADLESS_BACKEND_CONFIG_VERSION,
      .struct_size = sizeof(struct weston_headless_backend_config),
  }};

  config.renderer = WESTON_RENDERER_PIXMAN;
  config.decorate = false;
  config.refresh = 60000;

  if (weston_compositor_load_backend(compositor, WESTON_BACKEND_HEADLESS,
                                     &config.base) < 0) {
    LOG_ERROR("Weston: Failed to load headless backend");
    weston_compositor_destroy(compositor);
    weston_log_subscriber_destroy(wb->logger);
    weston_log_ctx_destroy(wb->log_ctx);
    return -1;
  }

  LOG_INFO("Weston: Headless backend loaded");

  wb->seat = NULL;

  weston_compositor_flush_heads_changed(compositor);

  LOG_INFO("Weston: Compositor initialized");
  return 0;
}

struct weston_bridge *weston_bridge_init(struct wayland_bridge *plexy_bridge) {
  struct weston_bridge *wb;

  LOG_INFO("Initializing Weston integration...");

  wb = calloc(1, sizeof(*wb));
  if (!wb) {
    LOG_ERROR("Weston: Failed to allocate bridge");
    return NULL;
  }

  wb->plexy_bridge = plexy_bridge;

  if (weston_bridge_create_compositor(wb) < 0) {
    LOG_ERROR("Weston: Failed to create compositor");
    free(wb);
    return NULL;
  }

  wb->surface_created_listener.notify = handle_surface_created;
  wl_signal_add(&wb->compositor->create_surface_signal,
                &wb->surface_created_listener);

  wb->output_created_listener.notify = handle_output_created;
  wl_signal_add(&wb->compositor->output_created_signal,
                &wb->output_created_listener);

  LOG_INFO("Weston integration initialized");

  return wb;
}

void weston_bridge_cleanup(struct weston_bridge *wb) {
  if (!wb)
    return;

  LOG_DEBUG("Cleaning up Weston integration...");

  if (wb->compositor)
    weston_compositor_destroy(wb->compositor);

  if (wb->logger)
    weston_log_subscriber_destroy(wb->logger);

  if (wb->log_ctx)
    weston_log_ctx_destroy(wb->log_ctx);

  free(wb);

  LOG_DEBUG("Weston integration cleaned up");
}

struct weston_compositor *
weston_bridge_get_compositor(struct weston_bridge *wb) {
  return wb ? wb->compositor : NULL;
}

struct weston_seat *weston_bridge_get_seat(struct weston_bridge *wb) {
  return wb ? wb->seat : NULL;
}
