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

extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface xwayland_surface_v1_interface;

static const struct wl_interface *xwayland_shell_v1_types[] = {
    NULL,
    NULL,
    &xwayland_surface_v1_interface,
    &wl_surface_interface,
};

static const struct wl_message xwayland_shell_v1_requests[] = {
    {"destroy", "", xwayland_shell_v1_types + 0},
    {"get_xwayland_surface", "no", xwayland_shell_v1_types + 2},
};

WL_PRIVATE const struct wl_interface xwayland_shell_v1_interface = {
    "xwayland_shell_v1", 1, 2, xwayland_shell_v1_requests, 0, NULL,
};

static const struct wl_message xwayland_surface_v1_requests[] = {
    {"set_serial", "uu", xwayland_shell_v1_types + 0},
    {"destroy", "", xwayland_shell_v1_types + 0},
};

WL_PRIVATE const struct wl_interface xwayland_surface_v1_interface = {
    "xwayland_surface_v1", 1, 2, xwayland_surface_v1_requests, 0, NULL,
};
