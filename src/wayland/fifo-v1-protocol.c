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
extern const struct wl_interface wp_fifo_v1_interface;

static const struct wl_interface *fifo_v1_types[] = {
    &wp_fifo_v1_interface,
    &wl_surface_interface,
};

static const struct wl_message wp_fifo_manager_v1_requests[] = {
    {"destroy", "", fifo_v1_types + 0},
    {"get_fifo", "no", fifo_v1_types + 0},
};

WL_PRIVATE const struct wl_interface wp_fifo_manager_v1_interface = {
    "wp_fifo_manager_v1", 1, 2, wp_fifo_manager_v1_requests, 0, NULL,
};

static const struct wl_message wp_fifo_v1_requests[] = {
    {"set_barrier", "", fifo_v1_types + 0},
    {"wait_barrier", "", fifo_v1_types + 0},
    {"destroy", "", fifo_v1_types + 0},
};

WL_PRIVATE const struct wl_interface wp_fifo_v1_interface = {
    "wp_fifo_v1", 1, 3, wp_fifo_v1_requests, 0, NULL,
};
