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

extern const struct wl_interface wl_seat_interface;
extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface zwp_xwayland_keyboard_grab_v1_interface;

static const struct wl_interface *xwayland_keyboard_grab_unstable_v1_types[] = {
    &zwp_xwayland_keyboard_grab_v1_interface,
    &wl_surface_interface,
    &wl_seat_interface,
};

static const struct wl_message
    zwp_xwayland_keyboard_grab_manager_v1_requests[] = {
        {"destroy", "", xwayland_keyboard_grab_unstable_v1_types + 0},
        {"grab_keyboard", "noo", xwayland_keyboard_grab_unstable_v1_types + 0},
};

WL_PRIVATE const struct wl_interface
    zwp_xwayland_keyboard_grab_manager_v1_interface = {
        "zwp_xwayland_keyboard_grab_manager_v1",        1, 2,
        zwp_xwayland_keyboard_grab_manager_v1_requests, 0, NULL,
};

static const struct wl_message zwp_xwayland_keyboard_grab_v1_requests[] = {
    {"destroy", "", xwayland_keyboard_grab_unstable_v1_types + 0},
};

WL_PRIVATE const struct wl_interface zwp_xwayland_keyboard_grab_v1_interface = {
    "zwp_xwayland_keyboard_grab_v1",        1, 1,
    zwp_xwayland_keyboard_grab_v1_requests, 0, NULL,
};
