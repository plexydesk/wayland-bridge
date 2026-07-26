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
extern const struct wl_interface wp_linux_drm_syncobj_surface_v1_interface;
extern const struct wl_interface wp_linux_drm_syncobj_timeline_v1_interface;

static const struct wl_interface *linux_drm_syncobj_v1_types[] = {
    &wp_linux_drm_syncobj_surface_v1_interface,
    &wl_surface_interface,
    &wp_linux_drm_syncobj_timeline_v1_interface,
    NULL,
    &wp_linux_drm_syncobj_timeline_v1_interface,
    NULL,
    NULL,
    &wp_linux_drm_syncobj_timeline_v1_interface,
    NULL,
    NULL,
};

static const struct wl_message wp_linux_drm_syncobj_manager_v1_requests[] = {
    {"destroy", "", linux_drm_syncobj_v1_types + 0},
    {"get_surface", "no", linux_drm_syncobj_v1_types + 0},
    {"import_timeline", "nh", linux_drm_syncobj_v1_types + 2},
};

WL_PRIVATE const struct wl_interface wp_linux_drm_syncobj_manager_v1_interface =
    {
        "wp_linux_drm_syncobj_manager_v1",        1, 3,
        wp_linux_drm_syncobj_manager_v1_requests, 0, NULL,
};

static const struct wl_message wp_linux_drm_syncobj_timeline_v1_requests[] = {
    {"destroy", "", linux_drm_syncobj_v1_types + 0},
};

WL_PRIVATE const struct wl_interface
    wp_linux_drm_syncobj_timeline_v1_interface = {
        "wp_linux_drm_syncobj_timeline_v1",        1, 1,
        wp_linux_drm_syncobj_timeline_v1_requests, 0, NULL,
};

static const struct wl_message wp_linux_drm_syncobj_surface_v1_requests[] = {
    {"destroy", "", linux_drm_syncobj_v1_types + 0},
    {"set_acquire_point", "ouu", linux_drm_syncobj_v1_types + 4},
    {"set_release_point", "ouu", linux_drm_syncobj_v1_types + 7},
};

WL_PRIVATE const struct wl_interface wp_linux_drm_syncobj_surface_v1_interface =
    {
        "wp_linux_drm_syncobj_surface_v1",        1, 3,
        wp_linux_drm_syncobj_surface_v1_requests, 0, NULL,
};
