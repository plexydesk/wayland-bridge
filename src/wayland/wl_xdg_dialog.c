/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */


#define _GNU_SOURCE
#include "wayland_bridge.h"
#include "xdg-dialog-v1-protocol.h"
#include <stdlib.h>

struct xdg_dialog {
  struct wl_resource *resource;
  struct wl_resource *toplevel;
  bool modal;
};

static void dialog_destroy(struct wl_client *client,
                           struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void dialog_set_modal(struct wl_client *client,
                             struct wl_resource *resource) {
  (void)client;
  struct xdg_dialog *dialog = wl_resource_get_user_data(resource);
  if (!dialog)
    return;

  dialog->modal = true;

  if (!bridge || !dialog->toplevel)
    return;
  struct bridge_surface *surface;
  wl_list_for_each(surface, &bridge->surfaces, link) {
    if (surface->xdg_toplevel == dialog->toplevel) {
      surface->is_modal = true;
      if (surface->plexy_window)
        plexy_window_set_type(surface->plexy_window, PLEXY_WINDOW_TYPE_DIALOG);
      break;
    }
  }
}

static void dialog_unset_modal(struct wl_client *client,
                               struct wl_resource *resource) {
  (void)client;
  struct xdg_dialog *dialog = wl_resource_get_user_data(resource);
  if (!dialog)
    return;

  dialog->modal = false;

  if (!bridge || !dialog->toplevel)
    return;
  struct bridge_surface *surface;
  wl_list_for_each(surface, &bridge->surfaces, link) {
    if (surface->xdg_toplevel == dialog->toplevel) {
      surface->is_modal = false;
      if (surface->plexy_window && !surface->toplevel_parent)
        plexy_window_set_type(surface->plexy_window, PLEXY_WINDOW_TYPE_NORMAL);
      break;
    }
  }
}

static const struct xdg_dialog_v1_interface dialog_impl = {
    .destroy = dialog_destroy,
    .set_modal = dialog_set_modal,
    .unset_modal = dialog_unset_modal,
};

static void dialog_resource_destroy(struct wl_resource *resource) {
  struct xdg_dialog *dialog = wl_resource_get_user_data(resource);
  if (dialog) {

    if (dialog->modal && bridge && dialog->toplevel) {
      struct bridge_surface *surface;
      wl_list_for_each(surface, &bridge->surfaces, link) {
        if (surface->xdg_toplevel == dialog->toplevel) {
          surface->is_modal = false;
          if (surface->plexy_window && !surface->toplevel_parent)
            plexy_window_set_type(surface->plexy_window,
                                  PLEXY_WINDOW_TYPE_NORMAL);
          break;
        }
      }
    }
    free(dialog);
  }
}

static void wm_dialog_destroy(struct wl_client *client,
                              struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void wm_dialog_get_xdg_dialog(struct wl_client *client,
                                     struct wl_resource *resource, uint32_t id,
                                     struct wl_resource *toplevel) {
  struct wl_resource *dialog_res = wl_resource_create(
      client, &xdg_dialog_v1_interface, wl_resource_get_version(resource), id);
  if (!dialog_res) {
    wl_client_post_no_memory(client);
    return;
  }

  struct xdg_dialog *dialog = calloc(1, sizeof(*dialog));
  if (!dialog) {
    wl_resource_destroy(dialog_res);
    wl_resource_post_no_memory(resource);
    return;
  }

  dialog->resource = dialog_res;
  dialog->toplevel = toplevel;
  dialog->modal = false;

  wl_resource_set_implementation(dialog_res, &dialog_impl, dialog,
                                 dialog_resource_destroy);
}

static const struct xdg_wm_dialog_v1_interface wm_dialog_impl = {
    .destroy = wm_dialog_destroy,
    .get_xdg_dialog = wm_dialog_get_xdg_dialog,
};

void bind_xdg_wm_dialog(struct wl_client *client, void *data, uint32_t version,
                        uint32_t id) {
  (void)data;
  struct wl_resource *resource =
      wl_resource_create(client, &xdg_wm_dialog_v1_interface, version, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &wm_dialog_impl, NULL, NULL);
}
