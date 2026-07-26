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

extern const struct wl_interface wl_output_interface;
extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface wp_presentation_feedback_interface;

static const struct wl_interface *presentation_time_types[] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &wl_surface_interface,
    &wp_presentation_feedback_interface,
    &wl_output_interface,
};

static const struct wl_message wp_presentation_requests[] = {
    {"destroy", "", presentation_time_types + 0},
    {"feedback", "on", presentation_time_types + 7},
};

static const struct wl_message wp_presentation_events[] = {
    {"clock_id", "u", presentation_time_types + 0},
};

WL_PRIVATE const struct wl_interface wp_presentation_interface = {
    "wp_presentation",        2, 2,
    wp_presentation_requests, 1, wp_presentation_events,
};

static const struct wl_message wp_presentation_feedback_events[] = {
    {"sync_output", "o", presentation_time_types + 9},
    {"presented", "uuuuuuu", presentation_time_types + 0},
    {"discarded", "", presentation_time_types + 0},
};

WL_PRIVATE const struct wl_interface wp_presentation_feedback_interface = {
    "wp_presentation_feedback", 2, 0, NULL, 3, wp_presentation_feedback_events,
};
