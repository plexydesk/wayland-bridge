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
extern const struct wl_interface zxdg_exported_v2_interface;
extern const struct wl_interface zxdg_imported_v2_interface;

static const struct wl_interface *xdg_foreign_unstable_v2_types[] = {
    NULL,
    &zxdg_exported_v2_interface,
    &wl_surface_interface,
    &zxdg_imported_v2_interface,
    NULL,
    &wl_surface_interface,
};

static const struct wl_message zxdg_exporter_v2_requests[] = {
    {"destroy", "", xdg_foreign_unstable_v2_types + 0},
    {"export_toplevel", "no", xdg_foreign_unstable_v2_types + 1},
};

WL_PRIVATE const struct wl_interface zxdg_exporter_v2_interface = {
    "zxdg_exporter_v2", 1, 2, zxdg_exporter_v2_requests, 0, NULL,
};

static const struct wl_message zxdg_importer_v2_requests[] = {
    {"destroy", "", xdg_foreign_unstable_v2_types + 0},
    {"import_toplevel", "ns", xdg_foreign_unstable_v2_types + 3},
};

WL_PRIVATE const struct wl_interface zxdg_importer_v2_interface = {
    "zxdg_importer_v2", 1, 2, zxdg_importer_v2_requests, 0, NULL,
};

static const struct wl_message zxdg_exported_v2_requests[] = {
    {"destroy", "", xdg_foreign_unstable_v2_types + 0},
};

static const struct wl_message zxdg_exported_v2_events[] = {
    {"handle", "s", xdg_foreign_unstable_v2_types + 0},
};

WL_PRIVATE const struct wl_interface zxdg_exported_v2_interface = {
    "zxdg_exported_v2",        1, 1,
    zxdg_exported_v2_requests, 1, zxdg_exported_v2_events,
};

static const struct wl_message zxdg_imported_v2_requests[] = {
    {"destroy", "", xdg_foreign_unstable_v2_types + 0},
    {"set_parent_of", "o", xdg_foreign_unstable_v2_types + 5},
};

static const struct wl_message zxdg_imported_v2_events[] = {
    {"destroyed", "", xdg_foreign_unstable_v2_types + 0},
};

WL_PRIVATE const struct wl_interface zxdg_imported_v2_interface = {
    "zxdg_imported_v2",        1, 2,
    zxdg_imported_v2_requests, 1, zxdg_imported_v2_events,
};
