/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#include "wayland-util.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#if (__has_attribute(visibility) || defined(__GNUC__) && __GNUC__ >= 4)
#define WL_PRIVATE __attribute__((visibility("hidden")))
#else
#define WL_PRIVATE
#endif

extern const struct wl_interface wl_data_source_interface;
extern const struct wl_interface xdg_toplevel_interface;
extern const struct wl_interface xdg_toplevel_drag_v1_interface;

static const struct wl_interface *xdg_toplevel_drag_v1_types[] = {
    &xdg_toplevel_drag_v1_interface,
    &wl_data_source_interface,
    &xdg_toplevel_interface,
    NULL,
    NULL,
};

static const struct wl_message xdg_toplevel_drag_manager_v1_requests[] = {
    {"destroy", "", xdg_toplevel_drag_v1_types + 0},
    {"get_xdg_toplevel_drag", "no", xdg_toplevel_drag_v1_types + 0},
};

WL_PRIVATE const struct wl_interface xdg_toplevel_drag_manager_v1_interface = {
    "xdg_toplevel_drag_manager_v1",        1, 2,
    xdg_toplevel_drag_manager_v1_requests, 0, NULL,
};

static const struct wl_message xdg_toplevel_drag_v1_requests[] = {
    {"destroy", "", xdg_toplevel_drag_v1_types + 0},
    {"attach", "oii", xdg_toplevel_drag_v1_types + 2},
};

WL_PRIVATE const struct wl_interface xdg_toplevel_drag_v1_interface = {
    "xdg_toplevel_drag_v1", 1, 2, xdg_toplevel_drag_v1_requests, 0, NULL,
};
