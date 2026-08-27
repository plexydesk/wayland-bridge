/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#define _GNU_SOURCE
#include "ext-data-control-v1-protocol.h"
#include "wayland_bridge.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct dc_source_data {
  struct wl_resource *resource;
  struct wl_client *client;
  struct wl_array mime_types;
  bool used;
};

struct dc_offer_data {
  struct wl_resource *source_resource;
  bool use_external_text;
  char external_text[PLEXY_CLIPBOARD_MAX];
};

struct dc_device_data {
  struct wl_resource *resource;
  struct wl_client *client;
  struct wl_list link;
};

struct dc_capture_ctx {
  struct wl_event_source *source;
  int fd;
  char buf[1024];
  size_t len;
};

static struct wl_list dc_devices;
static bool dc_initialized = false;
static bool dc_listener_registered = false;
static struct wl_resource *current_dc_source = NULL;

static void free_mime_types(struct wl_array *mime_types) {
  char **mime = NULL;
  wl_array_for_each(mime, mime_types) { free(*mime); }
  wl_array_release(mime_types);
}

static const char *pick_text_mime(struct dc_source_data *source) {
  char **mime = NULL;
  if (!source)
    return NULL;
  wl_array_for_each(mime, &source->mime_types) {
    if (*mime && strcmp(*mime, "text/plain;charset=utf-8") == 0)
      return *mime;
  }
  wl_array_for_each(mime, &source->mime_types) {
    if (*mime && strcmp(*mime, "text/plain") == 0)
      return *mime;
  }
  wl_array_for_each(mime, &source->mime_types) {
    if (*mime && strstr(*mime, "text/") == *mime)
      return *mime;
  }
  return NULL;
}

static bool source_has_mime(struct dc_source_data *source,
                            const char *mime_type) {
  char **mime = NULL;
  if (!source || !mime_type)
    return false;
  wl_array_for_each(mime, &source->mime_types) {
    if (*mime && strcmp(*mime, mime_type) == 0)
      return true;
  }
  return false;
}

static bool mime_is_textual(const char *mime_type) {
  if (!mime_type)
    return false;
  if (strcmp(mime_type, "text/plain;charset=utf-8") == 0)
    return true;
  if (strcmp(mime_type, "text/plain") == 0)
    return true;
  if (strcmp(mime_type, "text/uri-list") == 0)
    return true;
  return strncmp(mime_type, "text/", 5) == 0;
}

static void capture_destroy(struct dc_capture_ctx *ctx) {
  if (!ctx)
    return;
  if (ctx->source)
    wl_event_source_remove(ctx->source);
  if (ctx->fd >= 0)
    close(ctx->fd);
  free(ctx);
}

static int capture_cb(int fd, uint32_t mask, void *data) {
  struct dc_capture_ctx *ctx = data;
  if (!ctx)
    return 0;

  if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
    ctx->buf[ctx->len < sizeof(ctx->buf) ? ctx->len : sizeof(ctx->buf) - 1] =
        '\0';
    LOG_INFO("data_control: capture_cb HUP/ERR: %zu bytes captured", ctx->len);
    bridge_clipboard_set_text(ctx->buf);
    capture_destroy(ctx);
    return 0;
  }

  for (;;) {
    if (ctx->len >= sizeof(ctx->buf) - 1)
      break;
    ssize_t n =
        read(fd, ctx->buf + ctx->len, (sizeof(ctx->buf) - 1) - ctx->len);
    if (n > 0) {
      ctx->len += (size_t)n;
      continue;
    }
    if (n == 0) {
      ctx->buf[ctx->len] = '\0';
      LOG_INFO("data_control: capture_cb EOF: %zu bytes captured", ctx->len);
      bridge_clipboard_set_text(ctx->buf);
      capture_destroy(ctx);
      return 0;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      break;
    capture_destroy(ctx);
    return 0;
  }

  return 0;
}

static void capture_source_text(struct wl_resource *src_resource) {
  if (!src_resource || !bridge || !bridge->loop)
    return;

  struct dc_source_data *src = wl_resource_get_user_data(src_resource);
  const char *mime = pick_text_mime(src);
  if (!mime) {
    LOG_WARN("data_control: capture_source_text: no text MIME type in source");
    return;
  }
  LOG_INFO("data_control: capture_source_text: requesting mime='%s'", mime);

  int pipefd[2];
  if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) < 0)
    return;

  struct dc_capture_ctx *ctx = calloc(1, sizeof(*ctx));
  if (!ctx) {
    close(pipefd[0]);
    close(pipefd[1]);
    return;
  }

  ctx->fd = pipefd[0];
  ctx->len = 0;

  ext_data_control_source_v1_send_send(src_resource, mime, pipefd[1]);
  close(pipefd[1]);

  ctx->source = wl_event_loop_add_fd(
      bridge->loop, ctx->fd,
      WL_EVENT_READABLE | WL_EVENT_HANGUP | WL_EVENT_ERROR, capture_cb, ctx);
  if (!ctx->source) {
    capture_destroy(ctx);
  }
}

static struct wl_resource *
create_offer_for_device(struct dc_device_data *dev,
                        struct wl_resource *source_resource,
                        bool use_external_text, const char *text);

static void send_dc_updates(struct wl_client *skip) {
  struct dc_device_data *d = NULL;
  wl_list_for_each(d, &dc_devices, link) {
    if (skip && d->client == skip)
      continue;
    struct wl_resource *offer_sel = NULL;
    struct wl_resource *offer_primary = NULL;

    if (current_dc_source) {
      offer_sel = create_offer_for_device(d, current_dc_source, false, NULL);
      offer_primary =
          create_offer_for_device(d, current_dc_source, false, NULL);
    } else if (bridge_clipboard_has_text()) {
      char *text = malloc(PLEXY_CLIPBOARD_MAX);
      if (text && bridge_clipboard_get_text(text, PLEXY_CLIPBOARD_MAX)) {
        offer_sel = create_offer_for_device(d, NULL, true, text);
        offer_primary = create_offer_for_device(d, NULL, true, text);
      }
      free(text);
    }

    if (offer_sel) {
      ext_data_control_device_v1_send_data_offer(d->resource, offer_sel);
      if (current_dc_source) {
        struct dc_source_data *src =
            wl_resource_get_user_data(current_dc_source);
        if (src) {
          char **mime = NULL;
          wl_array_for_each(mime, &src->mime_types) {
            if (*mime)
              ext_data_control_offer_v1_send_offer(offer_sel, *mime);
          }
        }
      } else {
        ext_data_control_offer_v1_send_offer(offer_sel,
                                             "text/plain;charset=utf-8");
        ext_data_control_offer_v1_send_offer(offer_sel, "text/plain");
        ext_data_control_offer_v1_send_offer(offer_sel, "text/uri-list");
      }
    }

    if (offer_primary) {
      ext_data_control_device_v1_send_data_offer(d->resource, offer_primary);
      if (current_dc_source) {
        struct dc_source_data *src =
            wl_resource_get_user_data(current_dc_source);
        if (src) {
          char **mime = NULL;
          wl_array_for_each(mime, &src->mime_types) {
            if (*mime)
              ext_data_control_offer_v1_send_offer(offer_primary, *mime);
          }
        }
      } else {
        ext_data_control_offer_v1_send_offer(offer_primary,
                                             "text/plain;charset=utf-8");
        ext_data_control_offer_v1_send_offer(offer_primary, "text/plain");
        ext_data_control_offer_v1_send_offer(offer_primary, "text/uri-list");
      }
    }

    ext_data_control_device_v1_send_selection(d->resource, offer_sel);
    ext_data_control_device_v1_send_primary_selection(d->resource,
                                                      offer_primary);
  }
}

static void clipboard_changed_cb(void *user_data) {
  (void)user_data;
  current_dc_source = NULL;
  send_dc_updates(NULL);
}

static void ensure_dc_state(void) {
  if (!dc_initialized) {
    wl_list_init(&dc_devices);
    dc_initialized = true;
  }
  if (!dc_listener_registered) {
    bridge_clipboard_register_change_listener(clipboard_changed_cb, NULL);
    dc_listener_registered = true;
  }
  bridge_ensure_clipboard_callback();
}

static void offer_receive(struct wl_client *client,
                          struct wl_resource *resource, const char *mime_type,
                          int32_t fd) {
  (void)client;
  struct dc_offer_data *offer = wl_resource_get_user_data(resource);
  if (!offer || fd < 0 || !mime_type) {
    if (fd >= 0)
      close(fd);
    return;
  }

  if (offer->use_external_text) {
    if (!mime_is_textual(mime_type)) {
      close(fd);
      return;
    }
    size_t len = strnlen(offer->external_text, sizeof(offer->external_text));
    size_t written = 0;
    while (written < len) {
      ssize_t n = write(fd, offer->external_text + written, len - written);
      if (n <= 0)
        break;
      written += (size_t)n;
    }
    close(fd);
    return;
  }

  struct dc_source_data *src =
      wl_resource_get_user_data(offer->source_resource);
  if (!source_has_mime(src, mime_type)) {
    close(fd);
    return;
  }

  ext_data_control_source_v1_send_send(offer->source_resource, mime_type, fd);
  close(fd);
}

static void offer_destroy(struct wl_client *client,
                          struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct ext_data_control_offer_v1_interface offer_impl = {
    .receive = offer_receive,
    .destroy = offer_destroy,
};

static void offer_resource_destroy(struct wl_resource *resource) {
  struct dc_offer_data *offer = wl_resource_get_user_data(resource);
  free(offer);
}

static struct wl_resource *
create_offer_for_device(struct dc_device_data *dev,
                        struct wl_resource *source_resource,
                        bool use_external_text, const char *text) {
  struct wl_resource *offer =
      wl_resource_create(dev->client, &ext_data_control_offer_v1_interface,
                         wl_resource_get_version(dev->resource), 0);
  if (!offer)
    return NULL;

  struct dc_offer_data *data = calloc(1, sizeof(*data));
  if (!data) {
    wl_resource_destroy(offer);
    return NULL;
  }
  data->source_resource = source_resource;
  data->use_external_text = use_external_text;
  if (text) {
    strncpy(data->external_text, text, sizeof(data->external_text) - 1);
    data->external_text[sizeof(data->external_text) - 1] = '\0';
  }

  wl_resource_set_implementation(offer, &offer_impl, data,
                                 offer_resource_destroy);
  return offer;
}

static void source_offer(struct wl_client *client, struct wl_resource *resource,
                         const char *mime_type) {
  (void)client;
  struct dc_source_data *src = wl_resource_get_user_data(resource);
  if (!src || !mime_type)
    return;

  char **slot = wl_array_add(&src->mime_types, sizeof(*slot));
  if (!slot)
    return;
  *slot = strdup(mime_type);
}

static void source_destroy_req(struct wl_client *client,
                               struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct ext_data_control_source_v1_interface source_impl = {
    .offer = source_offer,
    .destroy = source_destroy_req,
};

static void source_resource_destroy(struct wl_resource *resource) {
  struct dc_source_data *src = wl_resource_get_user_data(resource);
  if (!src)
    return;

  bool was_current = current_dc_source == resource;
  if (was_current) {
    current_dc_source = NULL;
    bridge_clipboard_set_text("");
  }

  free_mime_types(&src->mime_types);
  free(src);
}

static void device_set_common(struct wl_client *client,
                              struct wl_resource *source, bool primary) {
  (void)primary;

  if (source && wl_resource_get_client(source) != client)
    return;
  if (source &&
      !wl_resource_instance_of(source, &ext_data_control_source_v1_interface,
                               &source_impl))
    return;

  if (source) {
    struct dc_source_data *src = wl_resource_get_user_data(source);
    if (!src || src->used)
      return;
    src->used = true;
  }

  LOG_INFO("data_control: set_selection from client=%p source=%s",
           (void *)client,
           source ? "non-null (new clipboard)" : "null (clipboard cleared)");

  if (current_dc_source && current_dc_source != source) {
    ext_data_control_source_v1_send_cancelled(current_dc_source);
  }

  current_dc_source = source;
  if (!source) {
    bridge_clipboard_set_text("");
  } else {
    capture_source_text(source);
  }

  send_dc_updates(client);
}

static void device_set_selection(struct wl_client *client,
                                 struct wl_resource *resource,
                                 struct wl_resource *source) {
  (void)resource;
  device_set_common(client, source, false);
}

static void device_destroy(struct wl_client *client,
                           struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void device_set_primary_selection(struct wl_client *client,
                                         struct wl_resource *resource,
                                         struct wl_resource *source) {
  (void)resource;
  device_set_common(client, source, true);
}

static const struct ext_data_control_device_v1_interface device_impl = {
    .set_selection = device_set_selection,
    .destroy = device_destroy,
    .set_primary_selection = device_set_primary_selection,
};

static void device_resource_destroy(struct wl_resource *resource) {
  struct dc_device_data *d = wl_resource_get_user_data(resource);
  if (!d)
    return;
  wl_list_remove(&d->link);
  free(d);
}

static void manager_create_data_source(struct wl_client *client,
                                       struct wl_resource *resource,
                                       uint32_t id) {
  struct wl_resource *src_res =
      wl_resource_create(client, &ext_data_control_source_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!src_res) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct dc_source_data *src = calloc(1, sizeof(*src));
  if (!src) {
    wl_resource_destroy(src_res);
    wl_resource_post_no_memory(resource);
    return;
  }
  src->resource = src_res;
  src->client = client;
  src->used = false;
  wl_array_init(&src->mime_types);

  wl_resource_set_implementation(src_res, &source_impl, src,
                                 source_resource_destroy);
}

static void manager_get_data_device(struct wl_client *client,
                                    struct wl_resource *resource, uint32_t id,
                                    struct wl_resource *seat) {
  (void)seat;

  struct wl_resource *dev_res =
      wl_resource_create(client, &ext_data_control_device_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!dev_res) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct dc_device_data *d = calloc(1, sizeof(*d));
  if (!d) {
    wl_resource_destroy(dev_res);
    wl_resource_post_no_memory(resource);
    return;
  }
  d->resource = dev_res;
  d->client = client;
  wl_list_insert(&dc_devices, &d->link);

  wl_resource_set_implementation(dev_res, &device_impl, d,
                                 device_resource_destroy);
  send_dc_updates(NULL);
}

static void manager_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct ext_data_control_manager_v1_interface manager_impl = {
    .create_data_source = manager_create_data_source,
    .get_data_device = manager_get_data_device,
    .destroy = manager_destroy,
};

void bind_data_control_manager(struct wl_client *client, void *data,
                               uint32_t version, uint32_t id) {
  (void)data;
  ensure_dc_state();

  struct wl_resource *res = wl_resource_create(
      client, &ext_data_control_manager_v1_interface, version, id);
  if (!res) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(res, &manager_impl, NULL, NULL);
}
