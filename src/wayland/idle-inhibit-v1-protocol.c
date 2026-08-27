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
extern const struct wl_interface zwp_idle_inhibitor_v1_interface;

static const struct wl_interface *idle_inhibit_unstable_v1_types[] = {
    &zwp_idle_inhibitor_v1_interface,
    &wl_surface_interface,
};

static const struct wl_message zwp_idle_inhibit_manager_v1_requests[] = {
    {"destroy", "", idle_inhibit_unstable_v1_types + 0},
    {"create_inhibitor", "no", idle_inhibit_unstable_v1_types + 0},
};

WL_PRIVATE const struct wl_interface zwp_idle_inhibit_manager_v1_interface = {
    "zwp_idle_inhibit_manager_v1",        1, 2,
    zwp_idle_inhibit_manager_v1_requests, 0, NULL,
};

static const struct wl_message zwp_idle_inhibitor_v1_requests[] = {
    {"destroy", "", idle_inhibit_unstable_v1_types + 0},
};

WL_PRIVATE const struct wl_interface zwp_idle_inhibitor_v1_interface = {
    "zwp_idle_inhibitor_v1", 1, 1, zwp_idle_inhibitor_v1_requests, 0, NULL,
};
