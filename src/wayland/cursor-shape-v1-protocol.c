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

extern const struct wl_interface wl_pointer_interface;
extern const struct wl_interface wp_cursor_shape_device_v1_interface;
extern const struct wl_interface zwp_tablet_tool_v2_interface;

static const struct wl_interface *cursor_shape_v1_types[] = {
    NULL,
    NULL,
    &wp_cursor_shape_device_v1_interface,
    &wl_pointer_interface,
    &wp_cursor_shape_device_v1_interface,
    &zwp_tablet_tool_v2_interface,
};

static const struct wl_message wp_cursor_shape_manager_v1_requests[] = {
    {"destroy", "", cursor_shape_v1_types + 0},
    {"get_pointer", "no", cursor_shape_v1_types + 2},
    {"get_tablet_tool_v2", "no", cursor_shape_v1_types + 4},
};

WL_PRIVATE const struct wl_interface wp_cursor_shape_manager_v1_interface = {
    "wp_cursor_shape_manager_v1",        2, 3,
    wp_cursor_shape_manager_v1_requests, 0, NULL,
};

static const struct wl_message wp_cursor_shape_device_v1_requests[] = {
    {"destroy", "", cursor_shape_v1_types + 0},
    {"set_shape", "uu", cursor_shape_v1_types + 0},
};

WL_PRIVATE const struct wl_interface wp_cursor_shape_device_v1_interface = {
    "wp_cursor_shape_device_v1",        2, 2,
    wp_cursor_shape_device_v1_requests, 0, NULL,
};
