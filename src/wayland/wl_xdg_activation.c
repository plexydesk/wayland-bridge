/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#define _POSIX_C_SOURCE 200809L
#include "wayland_bridge.h"
#include "xdg-activation-v1-protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ACTIVATION_TOKEN_TTL_SEC 10

struct activation_token_request {
  struct wl_resource *resource;
  struct wl_client *client;
  struct wl_resource *surface;
  uint32_t serial;
  struct wl_resource *seat;
  char *app_id;
  bool committed;
  bool used;
  char token[64];
  struct timespec created_at;
  struct wl_list link;
};

static struct wl_list activation_tokens;
static bool activation_tokens_initialized = false;

static void ensure_activation_tokens_init(void) {
  if (activation_tokens_initialized) {
    return;
  }
  wl_list_init(&activation_tokens);
  activation_tokens_initialized = true;
}

static void activation_token_destroy(struct wl_client *client,
                                     struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void activation_token_set_serial(struct wl_client *client,
                                        struct wl_resource *resource,
                                        uint32_t serial,
                                        struct wl_resource *seat) {
  (void)client;
  struct activation_token_request *token = wl_resource_get_user_data(resource);
  if (!token) {
    return;
  }

  token->serial = serial;
  token->seat = seat;
}

static void activation_token_set_app_id(struct wl_client *client,
                                        struct wl_resource *resource,
                                        const char *app_id) {
  (void)client;
  struct activation_token_request *token = wl_resource_get_user_data(resource);
  if (!token) {
    return;
  }

  free(token->app_id);
  token->app_id = app_id ? strdup(app_id) : NULL;
}

static void activation_token_set_surface(struct wl_client *client,
                                         struct wl_resource *resource,
                                         struct wl_resource *surface) {
  (void)client;
  struct activation_token_request *token = wl_resource_get_user_data(resource);
  if (!token) {
    return;
  }

  token->surface = surface;
}

static void activation_token_commit(struct wl_client *client,
                                    struct wl_resource *resource) {
  (void)client;
  struct activation_token_request *token = wl_resource_get_user_data(resource);
  if (!token) {
    return;
  }

  if (token->used) {
    wl_resource_post_error(resource, XDG_ACTIVATION_TOKEN_V1_ERROR_ALREADY_USED,
                           "activation token already used");
    return;
  }

  if (token->committed) {
    wl_resource_post_error(resource, XDG_ACTIVATION_TOKEN_V1_ERROR_ALREADY_USED,
                           "activation token already committed");
    return;
  }

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  token->created_at = ts;
  snprintf(token->token, sizeof(token->token), "plexy-%ld-%ld-%p", ts.tv_sec,
           ts.tv_nsec, (void *)resource);
  token->committed = true;

  xdg_activation_token_v1_send_done(resource, token->token);
}

static const struct xdg_activation_token_v1_interface activation_token_impl = {
    .destroy = activation_token_destroy,
    .set_serial = activation_token_set_serial,
    .set_app_id = activation_token_set_app_id,
    .set_surface = activation_token_set_surface,
    .commit = activation_token_commit,
};

static void activation_token_resource_destroy(struct wl_resource *resource) {
  struct activation_token_request *token = wl_resource_get_user_data(resource);
  if (!token) {
    return;
  }

  if (token->committed && !token->used) {
    token->resource = NULL;
    return;
  }

  wl_list_remove(&token->link);
  free(token->app_id);
  free(token);
}

static void activation_destroy(struct wl_client *client,
                               struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void activation_get_activation_token(struct wl_client *client,
                                            struct wl_resource *resource,
                                            uint32_t id) {
  ensure_activation_tokens_init();

  struct wl_resource *token_resource =
      wl_resource_create(client, &xdg_activation_token_v1_interface,
                         wl_resource_get_version(resource), id);

  if (!token_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct activation_token_request *token = calloc(1, sizeof(*token));
  if (!token) {
    wl_resource_destroy(token_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  token->resource = token_resource;
  token->client = client;
  wl_list_insert(&activation_tokens, &token->link);

  wl_resource_set_implementation(token_resource, &activation_token_impl, token,
                                 activation_token_resource_destroy);
}

static bool token_is_expired(const struct activation_token_request *token,
                             const struct timespec *now) {
  if (!token || !now) {
    return true;
  }

  time_t dsec = now->tv_sec - token->created_at.tv_sec;
  long dnsec = now->tv_nsec - token->created_at.tv_nsec;
  if (dnsec < 0) {
    dsec -= 1;
    dnsec += 1000000000L;
  }

  if (dsec > ACTIVATION_TOKEN_TTL_SEC) {
    return true;
  }
  if (dsec == ACTIVATION_TOKEN_TTL_SEC && dnsec > 0) {
    return true;
  }
  return false;
}

static struct activation_token_request *
find_active_token(const char *token_str) {
  if (!activation_tokens_initialized || !token_str) {
    return NULL;
  }

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  struct activation_token_request *token, *tmp;
  wl_list_for_each_safe(token, tmp, &activation_tokens, link) {
    if (!token->committed || token->used || token_is_expired(token, &now)) {
      if (token->committed && (token->used || token_is_expired(token, &now)) &&
          !token->resource) {

        wl_list_remove(&token->link);
        free(token->app_id);
        free(token);
      }
      continue;
    }

    if (strcmp(token->token, token_str) == 0) {
      return token;
    }
  }

  return NULL;
}

static void activation_activate(struct wl_client *client,
                                struct wl_resource *resource,
                                const char *token_str,
                                struct wl_resource *surface_resource) {
  (void)resource;

  if (!surface_resource || !token_str) {
    return;
  }

  struct bridge_surface *surface =
      bridge_surface_from_resource(surface_resource);
  if (!surface || !surface->plexy_window) {
    return;
  }

  struct activation_token_request *token = find_active_token(token_str);
  if (!token) {
    LOG_WARN("xdg_activation: rejected invalid/expired token '%s'", token_str);
    return;
  }

  if (token->client != client) {
    LOG_WARN("xdg_activation: rejected token from different client");
    return;
  }

  token->used = true;

  if (bridge && bridge->keyboard_focus && bridge->keyboard_focus != surface) {
    forward_focus_out(bridge->keyboard_focus->plexy_window,
                      bridge->keyboard_focus);
  }

  if (bridge && bridge->plexy_conn) {
    int rc =
        plexy_activate_window(bridge->plexy_conn, surface->plexy_window_id);
    if (rc < 0) {
      LOG_WARN("xdg_activation: failed to raise window id=%u",
               surface->plexy_window_id);
    }
  }

  forward_focus_in(surface->plexy_window, surface);
  LOG_DEBUG("xdg_activation: activated surface=%p token=%s", (void *)surface,
            token_str);

  if (!token->resource) {

    wl_list_remove(&token->link);
    free(token->app_id);
    free(token);
  }
}

static const struct xdg_activation_v1_interface activation_impl = {
    .destroy = activation_destroy,
    .get_activation_token = activation_get_activation_token,
    .activate = activation_activate,
};

void bind_xdg_activation(struct wl_client *client, void *data, uint32_t version,
                         uint32_t id) {
  (void)data;
  ensure_activation_tokens_init();

  struct wl_resource *resource =
      wl_resource_create(client, &xdg_activation_v1_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &activation_impl, NULL, NULL);
}
