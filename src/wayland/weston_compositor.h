/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef WESTON_COMPOSITOR_H
#define WESTON_COMPOSITOR_H

#include <wayland-server.h>

struct wayland_bridge;
struct weston_compositor;
struct weston_surface;
struct weston_bridge;

struct weston_bridge *weston_bridge_init(struct wayland_bridge *plexy_bridge);

void weston_bridge_cleanup(struct weston_bridge *wb);

struct weston_compositor *
weston_bridge_get_compositor(struct weston_bridge *wb);

struct weston_seat *weston_bridge_get_seat(struct weston_bridge *wb);

void bridge_weston_surface_created(struct wayland_bridge *bridge,
                                   struct weston_surface *surface);

#endif
