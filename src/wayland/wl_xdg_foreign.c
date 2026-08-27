/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#include "wayland_bridge.h"
#include "xdg-foreign-v2-protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void generate_handle(char *buf, size_t len) {
  static int seeded = 0;
  if (!seeded) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    seeded = 1;
  }
  const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < len - 1; i++)
    buf[i] = hex[rand() % 16];
  buf[len - 1] = '\0';
}

#define MAX_EXPORTS 256

struct exported_surface {
  char handle[65];
  struct wl_resource *resource;
  struct bridge_surface *surface;
  bool in_use;
};

static struct exported_surface exports[MAX_EXPORTS];

static struct exported_surface *find_export_by_handle(const char *handle) {
  for (int i = 0; i < MAX_EXPORTS; i++) {
    if (exports[i].in_use && strcmp(exports[i].handle, handle) == 0)
      return &exports[i];
  }
  return NULL;
}

static struct exported_surface *alloc_export(void) {
  for (int i = 0; i < MAX_EXPORTS; i++) {
    if (!exports[i].in_use)
      return &exports[i];
  }
  return NULL;
}

static void exported_destroy(struct wl_client *client,
                             struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct zxdg_exported_v2_interface exported_impl = {
    .destroy = exported_destroy,
};

static void exported_resource_destroy(struct wl_resource *resource) {
  struct exported_surface *exp = wl_resource_get_user_data(resource);
  if (exp) {
    exp->in_use = false;
    LOG_DEBUG("xdg_foreign: export destroyed handle=%s", exp->handle);
  }
}

struct imported_surface {
  struct wl_resource *resource;
  struct exported_surface *export;
};

static void imported_destroy(struct wl_client *client,
                             struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void imported_set_parent_of(struct wl_client *client,
                                   struct wl_resource *resource,
                                   struct wl_resource *surface_resource) {
  (void)client;
  struct imported_surface *imp = wl_resource_get_user_data(resource);

  if (!imp || !imp->export || !imp->export->in_use) {
    LOG_DEBUG("xdg_foreign: set_parent_of with invalid import");
    return;
  }

  struct bridge_surface *child = wl_resource_get_user_data(surface_resource);
  (void)child;
  LOG_DEBUG("xdg_foreign: set_parent_of child=%p parent=%p", (void *)child,
            (void *)imp->export->surface);
}

static const struct zxdg_imported_v2_interface imported_impl = {
    .destroy = imported_destroy,
    .set_parent_of = imported_set_parent_of,
};

static void imported_resource_destroy(struct wl_resource *resource) {
  struct imported_surface *imp = wl_resource_get_user_data(resource);
  free(imp);
}

static void exporter_destroy(struct wl_client *client,
                             struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void exporter_export_toplevel(struct wl_client *client,
                                     struct wl_resource *resource, uint32_t id,
                                     struct wl_resource *surface_resource) {
  struct bridge_surface *surface = wl_resource_get_user_data(surface_resource);
  struct exported_surface *exp = alloc_export();

  if (!exp) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct wl_resource *exported_resource =
      wl_resource_create(client, &zxdg_exported_v2_interface,
                         wl_resource_get_version(resource), id);

  if (!exported_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  exp->in_use = true;
  exp->surface = surface;
  exp->resource = exported_resource;
  generate_handle(exp->handle, sizeof(exp->handle));

  wl_resource_set_implementation(exported_resource, &exported_impl, exp,
                                 exported_resource_destroy);

  zxdg_exported_v2_send_handle(exported_resource, exp->handle);

  LOG_DEBUG("xdg_foreign: exported surface=%p handle=%s", (void *)surface,
            exp->handle);
}

static const struct zxdg_exporter_v2_interface exporter_impl = {
    .destroy = exporter_destroy,
    .export_toplevel = exporter_export_toplevel,
};

void bind_xdg_exporter(struct wl_client *client, void *data, uint32_t version,
                       uint32_t id) {
  (void)data;
  struct wl_resource *resource =
      wl_resource_create(client, &zxdg_exporter_v2_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &exporter_impl, NULL, NULL);
}

static void importer_destroy(struct wl_client *client,
                             struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void importer_import_toplevel(struct wl_client *client,
                                     struct wl_resource *resource, uint32_t id,
                                     const char *handle) {
  struct wl_resource *imported_resource =
      wl_resource_create(client, &zxdg_imported_v2_interface,
                         wl_resource_get_version(resource), id);

  if (!imported_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct imported_surface *imp = calloc(1, sizeof(*imp));
  if (!imp) {
    wl_resource_destroy(imported_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  imp->resource = imported_resource;
  imp->export = find_export_by_handle(handle);

  wl_resource_set_implementation(imported_resource, &imported_impl, imp,
                                 imported_resource_destroy);

  if (!imp->export) {

    zxdg_imported_v2_send_destroyed(imported_resource);
    LOG_DEBUG("xdg_foreign: import failed, unknown handle=%s", handle);
  } else {
    LOG_DEBUG("xdg_foreign: imported handle=%s surface=%p", handle,
              (void *)imp->export->surface);
  }
}

static const struct zxdg_importer_v2_interface importer_impl = {
    .destroy = importer_destroy,
    .import_toplevel = importer_import_toplevel,
};

void bind_xdg_importer(struct wl_client *client, void *data, uint32_t version,
                       uint32_t id) {
  (void)data;
  struct wl_resource *resource =
      wl_resource_create(client, &zxdg_importer_v2_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &importer_impl, NULL, NULL);
}
