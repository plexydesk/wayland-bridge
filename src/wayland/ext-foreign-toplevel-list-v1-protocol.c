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

extern const struct wl_interface ext_foreign_toplevel_handle_v1_interface;

static const struct wl_interface *ext_foreign_toplevel_list_v1_types[] = {
    NULL,
    &ext_foreign_toplevel_handle_v1_interface,
};

static const struct wl_message ext_foreign_toplevel_list_v1_requests[] = {
    {"stop", "", ext_foreign_toplevel_list_v1_types + 0},
    {"destroy", "", ext_foreign_toplevel_list_v1_types + 0},
};

static const struct wl_message ext_foreign_toplevel_list_v1_events[] = {
    {"toplevel", "n", ext_foreign_toplevel_list_v1_types + 1},
    {"finished", "", ext_foreign_toplevel_list_v1_types + 0},
};

WL_PRIVATE const struct wl_interface ext_foreign_toplevel_list_v1_interface = {
    "ext_foreign_toplevel_list_v1",
    1,
    2,
    ext_foreign_toplevel_list_v1_requests,
    2,
    ext_foreign_toplevel_list_v1_events,
};

static const struct wl_message ext_foreign_toplevel_handle_v1_requests[] = {
    {"destroy", "", ext_foreign_toplevel_list_v1_types + 0},
};

static const struct wl_message ext_foreign_toplevel_handle_v1_events[] = {
    {"closed", "", ext_foreign_toplevel_list_v1_types + 0},
    {"done", "", ext_foreign_toplevel_list_v1_types + 0},
    {"title", "s", ext_foreign_toplevel_list_v1_types + 0},
    {"app_id", "s", ext_foreign_toplevel_list_v1_types + 0},
    {"identifier", "s", ext_foreign_toplevel_list_v1_types + 0},
};

WL_PRIVATE const struct wl_interface ext_foreign_toplevel_handle_v1_interface =
    {
        "ext_foreign_toplevel_handle_v1",
        1,
        1,
        ext_foreign_toplevel_handle_v1_requests,
        5,
        ext_foreign_toplevel_handle_v1_events,
};
