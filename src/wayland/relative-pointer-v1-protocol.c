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
extern const struct wl_interface zwp_relative_pointer_v1_interface;

static const struct wl_interface *relative_pointer_unstable_v1_types[] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &zwp_relative_pointer_v1_interface,
    &wl_pointer_interface,
};

static const struct wl_message zwp_relative_pointer_manager_v1_requests[] = {
    {"destroy", "", relative_pointer_unstable_v1_types + 0},
    {"get_relative_pointer", "no", relative_pointer_unstable_v1_types + 6},
};

WL_PRIVATE const struct wl_interface zwp_relative_pointer_manager_v1_interface =
    {
        "zwp_relative_pointer_manager_v1",        1, 2,
        zwp_relative_pointer_manager_v1_requests, 0, NULL,
};

static const struct wl_message zwp_relative_pointer_v1_requests[] = {
    {"destroy", "", relative_pointer_unstable_v1_types + 0},
};

static const struct wl_message zwp_relative_pointer_v1_events[] = {
    {"relative_motion", "uuffff", relative_pointer_unstable_v1_types + 0},
};

WL_PRIVATE const struct wl_interface zwp_relative_pointer_v1_interface = {
    "zwp_relative_pointer_v1",        1, 1,
    zwp_relative_pointer_v1_requests, 1, zwp_relative_pointer_v1_events,
};
