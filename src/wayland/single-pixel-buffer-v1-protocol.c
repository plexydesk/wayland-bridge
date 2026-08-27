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

static const struct wl_interface *single_pixel_buffer_v1_types[] = {
    &wl_buffer_interface, NULL, NULL, NULL, NULL,
};

static const struct wl_message wp_single_pixel_buffer_manager_v1_requests[] = {
    {"destroy", "", single_pixel_buffer_v1_types + 0},
    {"create_u32_rgba_buffer", "nuuuu", single_pixel_buffer_v1_types + 0},
};

WL_PRIVATE const struct wl_interface
    wp_single_pixel_buffer_manager_v1_interface = {
        "wp_single_pixel_buffer_manager_v1",        1, 2,
        wp_single_pixel_buffer_manager_v1_requests, 0, NULL,
};
