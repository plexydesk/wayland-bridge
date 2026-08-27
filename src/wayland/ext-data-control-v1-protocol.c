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

extern const struct wl_interface ext_data_control_device_v1_interface;
extern const struct wl_interface ext_data_control_offer_v1_interface;
extern const struct wl_interface ext_data_control_source_v1_interface;
extern const struct wl_interface wl_seat_interface;

static const struct wl_interface *ext_data_control_v1_types[] = {
    NULL,
    NULL,
    &ext_data_control_source_v1_interface,
    &ext_data_control_device_v1_interface,
    &wl_seat_interface,
    &ext_data_control_source_v1_interface,
    &ext_data_control_source_v1_interface,
    &ext_data_control_offer_v1_interface,
    &ext_data_control_offer_v1_interface,
    &ext_data_control_offer_v1_interface,
};

static const struct wl_message ext_data_control_manager_v1_requests[] = {
    {"create_data_source", "n", ext_data_control_v1_types + 2},
    {"get_data_device", "no", ext_data_control_v1_types + 3},
    {"destroy", "", ext_data_control_v1_types + 0},
};

WL_PRIVATE const struct wl_interface ext_data_control_manager_v1_interface = {
    "ext_data_control_manager_v1",        1, 3,
    ext_data_control_manager_v1_requests, 0, NULL,
};

static const struct wl_message ext_data_control_device_v1_requests[] = {
    {"set_selection", "?o", ext_data_control_v1_types + 5},
    {"destroy", "", ext_data_control_v1_types + 0},
    {"set_primary_selection", "?o", ext_data_control_v1_types + 6},
};

static const struct wl_message ext_data_control_device_v1_events[] = {
    {"data_offer", "n", ext_data_control_v1_types + 7},
    {"selection", "?o", ext_data_control_v1_types + 8},
    {"finished", "", ext_data_control_v1_types + 0},
    {"primary_selection", "?o", ext_data_control_v1_types + 9},
};

WL_PRIVATE const struct wl_interface ext_data_control_device_v1_interface = {
    "ext_data_control_device_v1",        1, 3,
    ext_data_control_device_v1_requests, 4, ext_data_control_device_v1_events,
};

static const struct wl_message ext_data_control_source_v1_requests[] = {
    {"offer", "s", ext_data_control_v1_types + 0},
    {"destroy", "", ext_data_control_v1_types + 0},
};

static const struct wl_message ext_data_control_source_v1_events[] = {
    {"send", "sh", ext_data_control_v1_types + 0},
    {"cancelled", "", ext_data_control_v1_types + 0},
};

WL_PRIVATE const struct wl_interface ext_data_control_source_v1_interface = {
    "ext_data_control_source_v1",        1, 2,
    ext_data_control_source_v1_requests, 2, ext_data_control_source_v1_events,
};

static const struct wl_message ext_data_control_offer_v1_requests[] = {
    {"receive", "sh", ext_data_control_v1_types + 0},
    {"destroy", "", ext_data_control_v1_types + 0},
};

static const struct wl_message ext_data_control_offer_v1_events[] = {
    {"offer", "s", ext_data_control_v1_types + 0},
};

WL_PRIVATE const struct wl_interface ext_data_control_offer_v1_interface = {
    "ext_data_control_offer_v1",        1, 2,
    ext_data_control_offer_v1_requests, 1, ext_data_control_offer_v1_events,
};
