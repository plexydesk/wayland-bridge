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

extern const struct wl_interface ext_session_lock_surface_v1_interface;
extern const struct wl_interface ext_session_lock_v1_interface;
extern const struct wl_interface wl_output_interface;
extern const struct wl_interface wl_surface_interface;

static const struct wl_interface *ext_session_lock_v1_types[] = {
    NULL,
    NULL,
    NULL,
    &ext_session_lock_v1_interface,
    &ext_session_lock_surface_v1_interface,
    &wl_surface_interface,
    &wl_output_interface,
};

static const struct wl_message ext_session_lock_manager_v1_requests[] = {
    {"destroy", "", ext_session_lock_v1_types + 0},
    {"lock", "n", ext_session_lock_v1_types + 3},
};

WL_PRIVATE const struct wl_interface ext_session_lock_manager_v1_interface = {
    "ext_session_lock_manager_v1",        1, 2,
    ext_session_lock_manager_v1_requests, 0, NULL,
};

static const struct wl_message ext_session_lock_v1_requests[] = {
    {"destroy", "", ext_session_lock_v1_types + 0},
    {"get_lock_surface", "noo", ext_session_lock_v1_types + 4},
    {"unlock_and_destroy", "", ext_session_lock_v1_types + 0},
};

static const struct wl_message ext_session_lock_v1_events[] = {
    {"locked", "", ext_session_lock_v1_types + 0},
    {"finished", "", ext_session_lock_v1_types + 0},
};

WL_PRIVATE const struct wl_interface ext_session_lock_v1_interface = {
    "ext_session_lock_v1",        1, 3,
    ext_session_lock_v1_requests, 2, ext_session_lock_v1_events,
};

static const struct wl_message ext_session_lock_surface_v1_requests[] = {
    {"destroy", "", ext_session_lock_v1_types + 0},
    {"ack_configure", "u", ext_session_lock_v1_types + 0},
};

static const struct wl_message ext_session_lock_surface_v1_events[] = {
    {"configure", "uuu", ext_session_lock_v1_types + 0},
};

WL_PRIVATE const struct wl_interface ext_session_lock_surface_v1_interface = {
    "ext_session_lock_surface_v1",        1, 2,
    ext_session_lock_surface_v1_requests, 1, ext_session_lock_surface_v1_events,
};
