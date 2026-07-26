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

extern const struct wl_interface ext_idle_notification_v1_interface;
extern const struct wl_interface wl_seat_interface;

static const struct wl_interface *ext_idle_notify_v1_types[] = {
    &ext_idle_notification_v1_interface, NULL, &wl_seat_interface,
    &ext_idle_notification_v1_interface, NULL, &wl_seat_interface,
};

static const struct wl_message ext_idle_notifier_v1_requests[] = {
    {"destroy", "", ext_idle_notify_v1_types + 0},
    {"get_idle_notification", "nuo", ext_idle_notify_v1_types + 0},
    {"get_input_idle_notification", "2nuo", ext_idle_notify_v1_types + 3},
};

WL_PRIVATE const struct wl_interface ext_idle_notifier_v1_interface = {
    "ext_idle_notifier_v1", 2, 3, ext_idle_notifier_v1_requests, 0, NULL,
};

static const struct wl_message ext_idle_notification_v1_requests[] = {
    {"destroy", "", ext_idle_notify_v1_types + 0},
};

static const struct wl_message ext_idle_notification_v1_events[] = {
    {"idled", "", ext_idle_notify_v1_types + 0},
    {"resumed", "", ext_idle_notify_v1_types + 0},
};

WL_PRIVATE const struct wl_interface ext_idle_notification_v1_interface = {
    "ext_idle_notification_v1",        2, 1,
    ext_idle_notification_v1_requests, 2, ext_idle_notification_v1_events,
};
