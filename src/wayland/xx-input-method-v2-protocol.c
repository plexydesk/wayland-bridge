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
extern const struct wl_interface xx_input_method_v1_interface;
extern const struct wl_interface xx_input_popup_positioner_v1_interface;
extern const struct wl_interface xx_input_popup_surface_v2_interface;

static const struct wl_interface *input_method_experimental_v2_types[] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &xx_input_popup_surface_v2_interface,
    &wl_surface_interface,
    &xx_input_popup_positioner_v1_interface,
    &xx_input_popup_positioner_v1_interface,
    NULL,
    &wl_seat_interface,
    &xx_input_method_v1_interface,
    &xx_input_popup_positioner_v1_interface,
};

static const struct wl_message xx_input_method_v1_requests[] = {
    {"perform_action", "3u", input_method_experimental_v2_types + 0},
    {"commit_string", "s", input_method_experimental_v2_types + 0},
    {"set_preedit_string", "sii", input_method_experimental_v2_types + 0},
    {"delete_surrounding_text", "uu", input_method_experimental_v2_types + 0},
    {"move_cursor", "3ii", input_method_experimental_v2_types + 0},
    {"commit", "u", input_method_experimental_v2_types + 0},
    {"get_input_popup_surface", "2noo", input_method_experimental_v2_types + 7},
    {"destroy", "", input_method_experimental_v2_types + 0},
};

static const struct wl_message xx_input_method_v1_events[] = {
    {"activate", "", input_method_experimental_v2_types + 0},
    {"deactivate", "", input_method_experimental_v2_types + 0},
    {"surrounding_text", "suu", input_method_experimental_v2_types + 0},
    {"text_change_cause", "u", input_method_experimental_v2_types + 0},
    {"content_type", "uu", input_method_experimental_v2_types + 0},
    {"set_available_actions", "3a", input_method_experimental_v2_types + 0},
    {"announce_supported_features", "3u",
     input_method_experimental_v2_types + 0},
    {"announce_protocol_compat", "3u", input_method_experimental_v2_types + 0},
    {"done", "", input_method_experimental_v2_types + 0},
    {"unavailable", "", input_method_experimental_v2_types + 0},
};

WL_PRIVATE const struct wl_interface xx_input_method_v1_interface = {
    "xx_input_method_v1",        4,  8,
    xx_input_method_v1_requests, 10, xx_input_method_v1_events,
};

static const struct wl_message xx_input_popup_surface_v2_requests[] = {
    {"ack_configure", "u", input_method_experimental_v2_types + 0},
    {"reposition", "ou", input_method_experimental_v2_types + 10},
    {"destroy", "", input_method_experimental_v2_types + 0},
};

static const struct wl_message xx_input_popup_surface_v2_events[] = {
    {"start_configure", "uuiiuuu", input_method_experimental_v2_types + 0},
    {"repositioned", "u", input_method_experimental_v2_types + 0},
};

WL_PRIVATE const struct wl_interface xx_input_popup_surface_v2_interface = {
    "xx_input_popup_surface_v2",        1, 3,
    xx_input_popup_surface_v2_requests, 2, xx_input_popup_surface_v2_events,
};

static const struct wl_message xx_input_popup_positioner_v1_requests[] = {
    {"destroy", "", input_method_experimental_v2_types + 0},
    {"set_size", "uu", input_method_experimental_v2_types + 0},
    {"set_anchor", "u", input_method_experimental_v2_types + 0},
    {"set_gravity", "u", input_method_experimental_v2_types + 0},
    {"set_constraint_adjustment", "u", input_method_experimental_v2_types + 0},
    {"set_offset", "ii", input_method_experimental_v2_types + 0},
    {"set_reactive", "", input_method_experimental_v2_types + 0},
};

WL_PRIVATE const struct wl_interface xx_input_popup_positioner_v1_interface = {
    "xx_input_popup_positioner_v1",        1, 7,
    xx_input_popup_positioner_v1_requests, 0, NULL,
};

static const struct wl_message xx_input_method_manager_v2_requests[] = {
    {"get_input_method", "on", input_method_experimental_v2_types + 12},
    {"get_positioner", "n", input_method_experimental_v2_types + 14},
    {"destroy", "", input_method_experimental_v2_types + 0},
};

WL_PRIVATE const struct wl_interface xx_input_method_manager_v2_interface = {
    "xx_input_method_manager_v2",        4, 3,
    xx_input_method_manager_v2_requests, 0, NULL,
};
