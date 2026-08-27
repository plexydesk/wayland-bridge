/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#define _GNU_SOURCE
#include "primary-selection-v1-protocol.h"
#include "wayland_bridge.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct primary_source_data {
  struct wl_resource *resource;
  struct wl_client *client;
  struct wl_array mime_types;
};

struct primary_offer_data {
  struct wl_resource *source_resource;
  bool use_external_text;
  char external_text[PLEXY_CLIPBOARD_MAX];
};

struct primary_device_data {
  struct wl_resource *resource;
  struct wl_client *client;
  struct wl_list link;
};

struct primary_capture_ctx {
  struct wl_event_source *source;
  int fd;
  struct wl_resource *src_resource;
  char buf[PLEXY_CLIPBOARD_MAX];
  size_t len;
};

static struct wl_list primary_devices;
static bool primary_devices_initialized = false;
static bool clipboard_listener_registered = false;
static struct wl_resource *current_primary_source = NULL;

static void ensure_primary_device_list(void) {
  if (!primary_devices_initialized) {
    wl_list_init(&primary_devices);
    primary_devices_initialized = true;
  }
}

static void free_mime_types(struct wl_array *mime_types) {
  char **mime = NULL;
  wl_array_for_each(mime, mime_types) { free(*mime); }
  wl_array_release(mime_types);
}

static const char *pick_text_mime(struct primary_source_data *source) {
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

static bool source_has_mime(struct primary_source_data *source,
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

static void capture_destroy(struct primary_capture_ctx *ctx) {
  if (!ctx)
    return;
  if (ctx->source)
    wl_event_source_remove(ctx->source);
  if (ctx->fd >= 0)
    close(ctx->fd);
  free(ctx);
}

static int capture_cb(int fd, uint32_t mask, void *data) {
  struct primary_capture_ctx *ctx = data;
  if (!ctx)
    return 0;

  if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
    ctx->buf[ctx->len < sizeof(ctx->buf) ? ctx->len : sizeof(ctx->buf) - 1] =
        '\0';
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

  struct primary_source_data *src = wl_resource_get_user_data(src_resource);
  const char *mime = pick_text_mime(src);
  if (!mime)
    return;

  int pipefd[2];
  if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) < 0)
    return;

  struct primary_capture_ctx *ctx = calloc(1, sizeof(*ctx));
  if (!ctx) {
    close(pipefd[0]);
    close(pipefd[1]);
    return;
  }

  ctx->fd = pipefd[0];
  ctx->src_resource = src_resource;
  ctx->len = 0;
  wl_resource_set_user_data(src_resource, src);

  zwp_primary_selection_source_v1_send_send(src_resource, mime, pipefd[1]);
  close(pipefd[1]);

  ctx->source = wl_event_loop_add_fd(
      bridge->loop, ctx->fd,
      WL_EVENT_READABLE | WL_EVENT_HANGUP | WL_EVENT_ERROR, capture_cb, ctx);
  if (!ctx->source) {
    capture_destroy(ctx);
  }
}

static struct primary_device_data *
find_device_for_client(struct wl_client *client) {
  struct primary_device_data *d = NULL;
  wl_list_for_each(d, &primary_devices, link) {
    if (d->client == client)
      return d;
  }
  return NULL;
}

static struct wl_resource *
create_offer_for_device(struct primary_device_data *dev,
                        struct wl_resource *source_resource,
                        bool use_external_text, const char *text);

static void send_primary_selection_updates(void) {
  ensure_primary_device_list();

  if (wl_list_empty(&primary_devices))
    return;

  struct wl_client *focused_client = NULL;
  if (bridge && bridge->keyboard_focus && bridge->keyboard_focus->resource) {
    if (bridge->keyboard_focus->magic != 0xBD5FACE0) {
      LOG_ERROR(
          "primary_selection: ignoring stale keyboard_focus=%p magic=0x%x",
          (void *)bridge->keyboard_focus, bridge->keyboard_focus->magic);
      bridge->keyboard_focus = NULL;
    } else {
      focused_client = wl_resource_get_client(bridge->keyboard_focus->resource);
    }
  }

  struct primary_device_data *d = NULL;
  wl_list_for_each(d, &primary_devices, link) {
    if (!focused_client || d->client != focused_client) {
      zwp_primary_selection_device_v1_send_selection(d->resource, NULL);
      continue;
    }

    struct wl_resource *offer = NULL;
    if (current_primary_source) {
      offer = create_offer_for_device(d, current_primary_source, false, NULL);
    } else if (bridge_clipboard_has_text()) {
      char *text = malloc(PLEXY_CLIPBOARD_MAX);
      if (text && bridge_clipboard_get_text(text, PLEXY_CLIPBOARD_MAX)) {
        offer = create_offer_for_device(d, NULL, true, text);
      }
      free(text);
    }

    if (!offer) {
      zwp_primary_selection_device_v1_send_selection(d->resource, NULL);
      continue;
    }

    zwp_primary_selection_device_v1_send_data_offer(d->resource, offer);
    if (current_primary_source) {
      struct primary_source_data *src =
          wl_resource_get_user_data(current_primary_source);
      if (src) {
        char **mime = NULL;
        wl_array_for_each(mime, &src->mime_types) {
          if (*mime)
            zwp_primary_selection_offer_v1_send_offer(offer, *mime);
        }
      }
    } else {
      zwp_primary_selection_offer_v1_send_offer(offer,
                                                "text/plain;charset=utf-8");
      zwp_primary_selection_offer_v1_send_offer(offer, "text/plain");
      zwp_primary_selection_offer_v1_send_offer(offer, "text/uri-list");
    }
    zwp_primary_selection_device_v1_send_selection(d->resource, offer);
  }
}

static void clipboard_changed_cb(void *user_data) {
  (void)user_data;
  current_primary_source = NULL;
  send_primary_selection_updates();
}

static void ensure_primary_state(void) {
  ensure_primary_device_list();
  if (!clipboard_listener_registered) {
    bridge_clipboard_register_change_listener(clipboard_changed_cb, NULL);
    clipboard_listener_registered = true;
  }
}

static void offer_receive(struct wl_client *client,
                          struct wl_resource *resource, const char *mime_type,
                          int32_t fd) {
  (void)client;
  struct primary_offer_data *offer = wl_resource_get_user_data(resource);
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

  struct primary_source_data *src =
      wl_resource_get_user_data(offer->source_resource);
  if (!source_has_mime(src, mime_type)) {
    close(fd);
    return;
  }

  zwp_primary_selection_source_v1_send_send(offer->source_resource, mime_type,
                                            fd);
  close(fd);
}

static void offer_destroy(struct wl_client *client,
                          struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct zwp_primary_selection_offer_v1_interface offer_impl = {
    .receive = offer_receive,
    .destroy = offer_destroy,
};

static void offer_resource_destroy(struct wl_resource *resource) {
  struct primary_offer_data *offer = wl_resource_get_user_data(resource);
  free(offer);
}

static struct wl_resource *
create_offer_for_device(struct primary_device_data *dev,
                        struct wl_resource *source_resource,
                        bool use_external_text, const char *text) {
  struct wl_resource *offer =
      wl_resource_create(dev->client, &zwp_primary_selection_offer_v1_interface,
                         wl_resource_get_version(dev->resource), 0);
  if (!offer)
    return NULL;

  struct primary_offer_data *data = calloc(1, sizeof(*data));
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
  struct primary_source_data *src = wl_resource_get_user_data(resource);
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

static const struct zwp_primary_selection_source_v1_interface source_impl = {
    .offer = source_offer,
    .destroy = source_destroy_req,
};

static void source_resource_destroy(struct wl_resource *resource) {
  struct primary_source_data *src = wl_resource_get_user_data(resource);
  if (!src)
    return;

  bool was_current = current_primary_source == resource;
  if (was_current) {
    current_primary_source = NULL;
    bridge_clipboard_set_text("");
  }

  free_mime_types(&src->mime_types);
  free(src);
}

static void device_set_selection(struct wl_client *client,
                                 struct wl_resource *resource,
                                 struct wl_resource *source, uint32_t serial) {
  (void)resource;
  (void)serial;

  if (source && wl_resource_get_client(source) != client)
    return;
  if (source &&
      !wl_resource_instance_of(
          source, &zwp_primary_selection_source_v1_interface, &source_impl))
    return;

  if (current_primary_source && current_primary_source != source) {
    zwp_primary_selection_source_v1_send_cancelled(current_primary_source);
  }

  current_primary_source = source;
  if (!source) {
    bridge_clipboard_set_text("");
  } else {
    capture_source_text(source);
  }
  send_primary_selection_updates();
}

static void device_destroy(struct wl_client *client,
                           struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct zwp_primary_selection_device_v1_interface device_impl = {
    .set_selection = device_set_selection,
    .destroy = device_destroy,
};

static void device_resource_destroy(struct wl_resource *resource) {
  struct primary_device_data *d = wl_resource_get_user_data(resource);
  if (!d)
    return;
  wl_list_remove(&d->link);
  free(d);
}

static void manager_create_source(struct wl_client *client,
                                  struct wl_resource *resource, uint32_t id) {
  struct wl_resource *src_res =
      wl_resource_create(client, &zwp_primary_selection_source_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!src_res) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct primary_source_data *src = calloc(1, sizeof(*src));
  if (!src) {
    wl_resource_destroy(src_res);
    wl_resource_post_no_memory(resource);
    return;
  }
  src->resource = src_res;
  src->client = client;
  wl_array_init(&src->mime_types);

  wl_resource_set_implementation(src_res, &source_impl, src,
                                 source_resource_destroy);
}

static void manager_get_device(struct wl_client *client,
                               struct wl_resource *resource, uint32_t id,
                               struct wl_resource *seat) {
  (void)seat;
  struct wl_resource *dev_res =
      wl_resource_create(client, &zwp_primary_selection_device_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!dev_res) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct primary_device_data *dev = calloc(1, sizeof(*dev));
  if (!dev) {
    wl_resource_destroy(dev_res);
    wl_resource_post_no_memory(resource);
    return;
  }
  dev->resource = dev_res;
  dev->client = client;
  wl_list_insert(&primary_devices, &dev->link);

  wl_resource_set_implementation(dev_res, &device_impl, dev,
                                 device_resource_destroy);
  send_primary_selection_updates();
}

static void manager_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct zwp_primary_selection_device_manager_v1_interface
    manager_impl = {
        .create_source = manager_create_source,
        .get_device = manager_get_device,
        .destroy = manager_destroy,
};

void bind_primary_selection_device_manager(struct wl_client *client, void *data,
                                           uint32_t version, uint32_t id) {
  (void)data;
  ensure_primary_state();

  struct wl_resource *res = wl_resource_create(
      client, &zwp_primary_selection_device_manager_v1_interface, version, id);
  if (!res) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(res, &manager_impl, NULL, NULL);
}

static char external_primary_text[PLEXY_CLIPBOARD_MAX] = {0};
static bool external_primary_active = false;

struct primary_change_listener {
  void (*callback)(void *user_data);
  void *user_data;
};
static struct primary_change_listener primary_listeners[4];
static int primary_listener_count = 0;

static void notify_primary_listeners(void) {
  for (int i = 0; i < primary_listener_count; i++) {
    if (primary_listeners[i].callback)
      primary_listeners[i].callback(primary_listeners[i].user_data);
  }
}

void bridge_primary_set_text(const char *text) {
  if (!text || text[0] == '\0') {
    external_primary_active = false;
    external_primary_text[0] = '\0';
  } else {
    external_primary_active = true;
    strncpy(external_primary_text, text, sizeof(external_primary_text) - 1);
    external_primary_text[sizeof(external_primary_text) - 1] = '\0';
  }
  current_primary_source = NULL;
  notify_primary_listeners();
  send_primary_selection_updates();
}

bool bridge_primary_get_text(char *out, size_t out_size) {
  if (!out || out_size == 0)
    return false;
  if (!external_primary_active) {
    out[0] = '\0';
    return false;
  }
  strncpy(out, external_primary_text, out_size - 1);
  out[out_size - 1] = '\0';
  return true;
}

bool bridge_primary_has_text(void) { return external_primary_active; }

void bridge_primary_register_change_listener(void (*callback)(void *user_data),
                                             void *user_data) {
  if (!callback)
    return;
  if (primary_listener_count >=
      (int)(sizeof(primary_listeners) / sizeof(primary_listeners[0])))
    return;
  primary_listeners[primary_listener_count].callback = callback;
  primary_listeners[primary_listener_count].user_data = user_data;
  primary_listener_count++;
}
