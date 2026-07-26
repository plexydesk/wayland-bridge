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

extern const struct wl_interface wl_buffer_interface;
extern const struct wl_interface xdg_toplevel_interface;
extern const struct wl_interface xdg_toplevel_icon_v1_interface;

static const struct wl_interface *xdg_toplevel_icon_v1_types[] = {
    NULL,
    &xdg_toplevel_icon_v1_interface,
    &xdg_toplevel_interface,
    &xdg_toplevel_icon_v1_interface,
    &wl_buffer_interface,
    NULL,
};

static const struct wl_message xdg_toplevel_icon_manager_v1_requests[] = {
    {"destroy", "", xdg_toplevel_icon_v1_types + 0},
    {"create_icon", "n", xdg_toplevel_icon_v1_types + 1},
    {"set_icon", "o?o", xdg_toplevel_icon_v1_types + 2},
};

static const struct wl_message xdg_toplevel_icon_manager_v1_events[] = {
    {"icon_size", "i", xdg_toplevel_icon_v1_types + 0},
    {"done", "", xdg_toplevel_icon_v1_types + 0},
};

WL_PRIVATE const struct wl_interface xdg_toplevel_icon_manager_v1_interface = {
    "xdg_toplevel_icon_manager_v1",
    1,
    3,
    xdg_toplevel_icon_manager_v1_requests,
    2,
    xdg_toplevel_icon_manager_v1_events,
};

static const struct wl_message xdg_toplevel_icon_v1_requests[] = {
    {"destroy", "", xdg_toplevel_icon_v1_types + 0},
    {"set_name", "s", xdg_toplevel_icon_v1_types + 0},
    {"add_buffer", "oi", xdg_toplevel_icon_v1_types + 4},
};

WL_PRIVATE const struct wl_interface xdg_toplevel_icon_v1_interface = {
    "xdg_toplevel_icon_v1", 1, 3, xdg_toplevel_icon_v1_requests, 0, NULL,
};
