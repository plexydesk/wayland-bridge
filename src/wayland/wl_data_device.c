/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#define _GNU_SOURCE
#include "wayland_bridge.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-protocol.h>

struct data_source_data {
  struct wl_resource *resource;
  struct wl_client *client;
  struct wl_array mime_types;
  uint32_t dnd_actions;
};

struct data_offer_data {
  struct wl_resource *resource;
  struct wl_resource *source_resource;
  struct wl_client *client;
  bool use_external_text;
  char external_text[PLEXY_CLIPBOARD_MAX];
  uint32_t preferred_action;
};

struct data_device_data {
  struct wl_resource *resource;
  struct wl_client *client;
  struct wl_list link;
};

struct clipboard_listener {
  void (*callback)(void *user_data);
  void *user_data;
};

static struct wl_list data_devices;
static bool data_devices_initialized = false;
static struct wl_resource *current_selection_source = NULL;
static bool external_selection_active = false;
static char external_selection_text[PLEXY_CLIPBOARD_MAX];
static struct wl_event_source *clipboard_capture_source = NULL;
static int clipboard_capture_fd = -1;
static char clipboard_capture_buf[PLEXY_CLIPBOARD_MAX];
static size_t clipboard_capture_len = 0;
static bool clipboard_callback_registered = false;
static struct clipboard_listener clipboard_listeners[16];
static size_t clipboard_listener_count = 0;

static struct wl_resource *active_drag_source = NULL;
static struct wl_resource *active_drag_target_device = NULL;
static struct wl_resource *active_drag_target_surface = NULL;
static bool active_drag_has_target = false;

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

static uint32_t get_timestamp_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000ull + ts.tv_nsec / 1000000ull);
}

static void notify_clipboard_listeners(void) {
  for (size_t i = 0; i < clipboard_listener_count; ++i) {
    if (clipboard_listeners[i].callback) {
      clipboard_listeners[i].callback(clipboard_listeners[i].user_data);
    }
  }
}

static void stop_clipboard_capture(void) {
  if (clipboard_capture_source) {
    wl_event_source_remove(clipboard_capture_source);
    clipboard_capture_source = NULL;
  }
  if (clipboard_capture_fd >= 0) {
    close(clipboard_capture_fd);
    clipboard_capture_fd = -1;
  }
  clipboard_capture_len = 0;
}

static int clipboard_capture_cb(int fd, uint32_t mask, void *data) {
  (void)data;
  if (fd < 0)
    return 0;

  if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
    clipboard_capture_buf[clipboard_capture_len < sizeof(clipboard_capture_buf)
                              ? clipboard_capture_len
                              : sizeof(clipboard_capture_buf) - 1] = '\0';
    if (clipboard_capture_len > 0) {
      external_selection_active = true;
      strncpy(external_selection_text, clipboard_capture_buf,
              sizeof(external_selection_text) - 1);
      external_selection_text[sizeof(external_selection_text) - 1] = '\0';
      notify_clipboard_listeners();
    }
    if (bridge && bridge->plexy_conn) {
      plexy_set_clipboard_text(bridge->plexy_conn, NULL, clipboard_capture_buf);
    }
    stop_clipboard_capture();
    return 0;
  }

  for (;;) {
    if (clipboard_capture_len >= sizeof(clipboard_capture_buf) - 1)
      break;
    ssize_t n =
        read(fd, clipboard_capture_buf + clipboard_capture_len,
             (sizeof(clipboard_capture_buf) - 1) - clipboard_capture_len);
    if (n > 0) {
      clipboard_capture_len += (size_t)n;
      continue;
    }
    if (n == 0) {
      clipboard_capture_buf[clipboard_capture_len] = '\0';
      external_selection_active = true;
      strncpy(external_selection_text, clipboard_capture_buf,
              sizeof(external_selection_text) - 1);
      external_selection_text[sizeof(external_selection_text) - 1] = '\0';
      notify_clipboard_listeners();
      if (bridge && bridge->plexy_conn) {
        plexy_set_clipboard_text(bridge->plexy_conn, NULL,
                                 clipboard_capture_buf);
      }
      stop_clipboard_capture();
      return 0;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      break;
    }
    stop_clipboard_capture();
    return 0;
  }

  return 0;
}

static void on_plexy_clipboard_text(PlexyConnection *conn, const char *text,
                                    void *user_data) {
  (void)conn;
  (void)user_data;

  if (!text || text[0] == '\0') {
    external_selection_active = false;
    external_selection_text[0] = '\0';
  } else {
    external_selection_active = true;
    strncpy(external_selection_text, text, sizeof(external_selection_text) - 1);
    external_selection_text[sizeof(external_selection_text) - 1] = '\0';
  }

  current_selection_source = NULL;
  notify_clipboard_listeners();

  if (!bridge || !bridge->keyboard_focus || !bridge->keyboard_focus->is_x11) {
    bridge_data_device_send_selection_to_focus();
  }
}

void bridge_clipboard_set_text(const char *text) {
  if (!text || text[0] == '\0') {
    external_selection_active = false;
    external_selection_text[0] = '\0';
  } else {
    external_selection_active = true;
    strncpy(external_selection_text, text, sizeof(external_selection_text) - 1);
    external_selection_text[sizeof(external_selection_text) - 1] = '\0';
    LOG_INFO("bridge_clipboard_set_text: %zu bytes set from X11",
             strlen(external_selection_text));
  }
  current_selection_source = NULL;
  notify_clipboard_listeners();
  if (bridge && bridge->plexy_conn) {
    plexy_set_clipboard_text(bridge->plexy_conn, NULL, external_selection_text);
  }

  if (!bridge || !bridge->keyboard_focus || !bridge->keyboard_focus->is_x11) {
    bridge_data_device_send_selection_to_focus();
  }
}

bool bridge_clipboard_get_text(char *out, size_t out_size) {
  if (!out || out_size == 0)
    return false;
  if (!external_selection_active) {
    out[0] = '\0';
    return false;
  }
  strncpy(out, external_selection_text, out_size - 1);
  out[out_size - 1] = '\0';
  return true;
}

bool bridge_clipboard_has_text(void) { return external_selection_active; }

void bridge_clipboard_register_change_listener(
    void (*callback)(void *user_data), void *user_data) {
  if (!callback)
    return;
  if (clipboard_listener_count >=
      (sizeof(clipboard_listeners) / sizeof(clipboard_listeners[0]))) {
    return;
  }
  clipboard_listeners[clipboard_listener_count].callback = callback;
  clipboard_listeners[clipboard_listener_count].user_data = user_data;
  clipboard_listener_count++;
}

static void ensure_data_device_state(void) {
  if (!data_devices_initialized) {
    wl_list_init(&data_devices);
    data_devices_initialized = true;
  }

  if (!clipboard_callback_registered && bridge && bridge->plexy_conn) {
    plexy_set_clipboard_text_callback(bridge->plexy_conn,
                                      on_plexy_clipboard_text, NULL);
    plexy_request_clipboard_text(bridge->plexy_conn, NULL);
    clipboard_callback_registered = true;
  }
}

void bridge_ensure_clipboard_callback(void) { ensure_data_device_state(); }

static bool mime_is_offered(struct data_source_data *source,
                            const char *mime_type) {
  if (!source || !mime_type)
    return false;

  char **mime = NULL;
  wl_array_for_each(mime, &source->mime_types) {
    if (*mime && strcmp(*mime, mime_type) == 0) {
      return true;
    }
  }
  return false;
}

static const char *pick_text_mime(struct data_source_data *source) {
  if (!source)
    return NULL;
  char **mime = NULL;

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

static void
mirror_selection_to_plexy_clipboard(struct wl_resource *source_resource) {
  stop_clipboard_capture();
  if (!source_resource || !bridge || !bridge->loop || !bridge->plexy_conn)
    return;

  struct data_source_data *source = wl_resource_get_user_data(source_resource);
  if (!source)
    return;

  const char *mime = pick_text_mime(source);
  if (!mime)
    return;

  int pipefd[2];
  if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) < 0) {
    return;
  }

  wl_data_source_send_send(source_resource, mime, pipefd[1]);
  close(pipefd[1]);

  clipboard_capture_fd = pipefd[0];
  clipboard_capture_len = 0;
  clipboard_capture_source =
      wl_event_loop_add_fd(bridge->loop, clipboard_capture_fd,
                           WL_EVENT_READABLE | WL_EVENT_HANGUP | WL_EVENT_ERROR,
                           clipboard_capture_cb, NULL);
  if (!clipboard_capture_source) {
    stop_clipboard_capture();
  }
}

static void free_mime_types(struct wl_array *mime_types) {
  char **mime = NULL;
  wl_array_for_each(mime, mime_types) { free(*mime); }
  wl_array_release(mime_types);
}

static void data_offer_accept(struct wl_client *client,
                              struct wl_resource *resource, uint32_t serial,
                              const char *mime_type) {
  (void)client;
  (void)serial;

  struct data_offer_data *offer = wl_resource_get_user_data(resource);
  if (!offer || !offer->source_resource)
    return;

  wl_data_source_send_target(offer->source_resource, mime_type);
}

static void data_offer_receive(struct wl_client *client,
                               struct wl_resource *resource,
                               const char *mime_type, int32_t fd) {
  (void)client;

  struct data_offer_data *offer = wl_resource_get_user_data(resource);
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

  if (!offer->source_resource) {
    close(fd);
    return;
  }

  struct data_source_data *source =
      wl_resource_get_user_data(offer->source_resource);
  if (!source || !mime_is_offered(source, mime_type)) {
    close(fd);
    return;
  }

  wl_data_source_send_send(offer->source_resource, mime_type, fd);
  close(fd);
}

static void data_offer_destroy(struct wl_client *client,
                               struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void data_offer_finish(struct wl_client *client,
                              struct wl_resource *resource) {
  (void)client;
  struct data_offer_data *offer = wl_resource_get_user_data(resource);
  if (!offer || !offer->source_resource)
    return;

  wl_data_source_send_dnd_finished(offer->source_resource);
}

static void data_offer_set_actions(struct wl_client *client,
                                   struct wl_resource *resource,
                                   uint32_t dnd_actions,
                                   uint32_t preferred_action) {
  (void)client;
  struct data_offer_data *offer = wl_resource_get_user_data(resource);
  if (!offer)
    return;

  offer->preferred_action = preferred_action;

  if (!offer->source_resource)
    return;
  struct data_source_data *source =
      wl_resource_get_user_data(offer->source_resource);
  if (!source)
    return;

  uint32_t possible = source->dnd_actions & dnd_actions;
  uint32_t chosen = 0;
  if (possible & preferred_action)
    chosen = preferred_action;
  else if (possible & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    chosen = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
  else if (possible & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    chosen = WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
  else if (possible & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
    chosen = WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;

  if (wl_resource_get_version(resource) >= WL_DATA_OFFER_ACTION_SINCE_VERSION)
    wl_data_offer_send_action(resource, chosen);
  wl_data_source_send_action(offer->source_resource, chosen);
}

static const struct wl_data_offer_interface data_offer_implementation = {
    .accept = data_offer_accept,
    .receive = data_offer_receive,
    .destroy = data_offer_destroy,
    .finish = data_offer_finish,
    .set_actions = data_offer_set_actions,
};

static void data_offer_resource_destroy(struct wl_resource *resource) {
  struct data_offer_data *offer = wl_resource_get_user_data(resource);
  free(offer);
}

static struct data_device_data *
find_data_device_for_client(struct wl_client *client) {
  struct data_device_data *device_data = NULL;
  wl_list_for_each(device_data, &data_devices, link) {
    if (device_data->client == client)
      return device_data;
  }
  return NULL;
}

static struct wl_resource *
create_data_offer_for_source(struct data_device_data *device_data,
                             struct wl_resource *source_resource) {
  struct wl_resource *offer_resource =
      wl_resource_create(device_data->client, &wl_data_offer_interface,
                         wl_resource_get_version(device_data->resource), 0);

  if (!offer_resource) {
    wl_resource_post_no_memory(device_data->resource);
    return NULL;
  }

  struct data_offer_data *offer_data = calloc(1, sizeof(*offer_data));
  if (!offer_data) {
    wl_resource_destroy(offer_resource);
    wl_resource_post_no_memory(device_data->resource);
    return NULL;
  }

  offer_data->resource = offer_resource;
  offer_data->source_resource = source_resource;
  offer_data->client = device_data->client;
  offer_data->use_external_text = false;
  offer_data->external_text[0] = '\0';

  wl_resource_set_implementation(offer_resource, &data_offer_implementation,
                                 offer_data, data_offer_resource_destroy);

  wl_data_device_send_data_offer(device_data->resource, offer_resource);

  return offer_resource;
}

static struct wl_resource *
create_data_offer_for_device(struct data_device_data *device_data) {
  return create_data_offer_for_source(device_data, current_selection_source);
}

void bridge_data_device_send_selection_to_client(struct wl_client *client) {
  if (!client)
    return;
  ensure_data_device_state();

  struct data_device_data *device_data = NULL;
  wl_list_for_each(device_data, &data_devices, link) {
    if (device_data->client != client)
      continue;

    if (!current_selection_source && !external_selection_active) {
      wl_data_device_send_selection(device_data->resource, NULL);
      continue;
    }

    struct wl_resource *offer_resource =
        create_data_offer_for_device(device_data);
    if (!offer_resource)
      continue;
    struct data_offer_data *offer_data =
        wl_resource_get_user_data(offer_resource);

    if (!current_selection_source && external_selection_active) {
      offer_data->use_external_text = true;
      strncpy(offer_data->external_text, external_selection_text,
              sizeof(offer_data->external_text) - 1);
      offer_data->external_text[sizeof(offer_data->external_text) - 1] = '\0';
      wl_data_offer_send_offer(offer_resource, "text/plain;charset=utf-8");
      wl_data_offer_send_offer(offer_resource, "text/plain");
      wl_data_offer_send_offer(offer_resource, "text/uri-list");
      wl_data_device_send_selection(device_data->resource, offer_resource);
      continue;
    }

    struct data_source_data *source =
        wl_resource_get_user_data(current_selection_source);
    if (!source) {
      wl_data_device_send_selection(device_data->resource, NULL);
      continue;
    }

    char **mime = NULL;
    wl_array_for_each(mime, &source->mime_types) {
      if (*mime) {
        wl_data_offer_send_offer(offer_resource, *mime);
      }
    }

    wl_data_device_send_selection(device_data->resource, offer_resource);
  }
}

void bridge_data_device_send_selection_to_focus(void) {
  if (!bridge || !bridge->keyboard_focus || !bridge->keyboard_focus->resource)
    return;
  if (bridge->keyboard_focus->magic != 0xBD5FACE0) {
    LOG_ERROR("send_selection_to_focus: USE-AFTER-FREE on keyboard_focus=%p "
              "magic=0x%x",
              (void *)bridge->keyboard_focus, bridge->keyboard_focus->magic);
    bridge->keyboard_focus = NULL;
    return;
  }

  struct wl_client *focused_client =
      wl_resource_get_client(bridge->keyboard_focus->resource);
  bridge_data_device_send_selection_to_client(focused_client);
}

static void data_source_offer(struct wl_client *client,
                              struct wl_resource *resource,
                              const char *mime_type) {
  (void)client;

  struct data_source_data *source_data = wl_resource_get_user_data(resource);
  if (!source_data || !mime_type)
    return;

  char **slot = wl_array_add(&source_data->mime_types, sizeof(*slot));
  if (!slot) {
    wl_resource_post_no_memory(resource);
    return;
  }
  *slot = strdup(mime_type);
  if (!*slot) {
    *slot = NULL;
    wl_resource_post_no_memory(resource);
    return;
  }
}

static void data_source_destroy(struct wl_client *client,
                                struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void data_source_set_actions(struct wl_client *client,
                                    struct wl_resource *resource,
                                    uint32_t dnd_actions) {
  (void)client;
  struct data_source_data *source_data = wl_resource_get_user_data(resource);
  if (!source_data)
    return;
  source_data->dnd_actions = dnd_actions;
}

static const struct wl_data_source_interface data_source_implementation = {
    .offer = data_source_offer,
    .destroy = data_source_destroy,
    .set_actions = data_source_set_actions,
};

static void data_source_resource_destroy(struct wl_resource *resource) {
  struct data_source_data *source_data = wl_resource_get_user_data(resource);
  if (!source_data)
    return;

  bool was_current_selection = current_selection_source == resource;
  if (was_current_selection) {
    current_selection_source = NULL;
    stop_clipboard_capture();
  }

  free_mime_types(&source_data->mime_types);
  free(source_data);

  if (was_current_selection) {
    bridge_clipboard_set_text("");
  }
}

static void data_device_start_drag(struct wl_client *client,
                                   struct wl_resource *resource,
                                   struct wl_resource *source,
                                   struct wl_resource *origin,
                                   struct wl_resource *icon, uint32_t serial) {
  (void)resource;
  (void)icon;
  (void)serial;

  if (!source)
    return;
  if (wl_resource_get_client(source) != client)
    return;
  if (!wl_resource_instance_of(source, &wl_data_source_interface,
                               &data_source_implementation))
    return;

  if (origin && wl_resource_get_client(origin) != client)
    return;

  active_drag_source = source;
  active_drag_target_device = NULL;
  active_drag_target_surface = NULL;
  active_drag_has_target = false;
}

static void clear_drag_target(void) {
  if (active_drag_target_device && active_drag_target_surface) {
    wl_data_device_send_leave(active_drag_target_device);
  }
  active_drag_target_device = NULL;
  active_drag_target_surface = NULL;
  active_drag_has_target = false;
}

void bridge_data_device_pointer_motion(struct bridge_surface *surface,
                                       int32_t x, int32_t y) {
  if (!active_drag_source || !bridge || !surface || !surface->resource)
    return;

  struct wl_client *target_client = wl_resource_get_client(surface->resource);
  struct data_device_data *target_device_data =
      find_data_device_for_client(target_client);
  struct wl_resource *target_device =
      target_device_data ? target_device_data->resource : NULL;

  if (target_device != active_drag_target_device) {
    clear_drag_target();
    if (!target_device)
      return;

    struct wl_resource *offer =
        create_data_offer_for_source(target_device_data, active_drag_source);
    if (!offer)
      return;

    struct data_source_data *source_data =
        wl_resource_get_user_data(active_drag_source);
    if (!source_data)
      return;
    char **mime = NULL;
    wl_array_for_each(mime, &source_data->mime_types) {
      if (*mime)
        wl_data_offer_send_offer(offer, *mime);
    }

    uint32_t source_actions = source_data->dnd_actions;
    if (source_actions == 0) {
      source_actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    }
    if (wl_resource_get_version(offer) >=
        WL_DATA_OFFER_SOURCE_ACTIONS_SINCE_VERSION) {
      wl_data_offer_send_source_actions(offer, source_actions);
      uint32_t chosen_action =
          (source_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
              ? WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY
              : source_actions;
      wl_data_offer_send_action(offer, chosen_action);
      wl_data_source_send_action(active_drag_source, chosen_action);
    }

    uint32_t serial = wl_display_next_serial(bridge->display);
    wl_data_device_send_enter(target_device, serial, surface->resource,
                              wl_fixed_from_int(x), wl_fixed_from_int(y),
                              offer);
    active_drag_target_device = target_device;
    active_drag_target_surface = surface->resource;
    active_drag_has_target = true;
    return;
  }

  if (active_drag_target_device) {
    wl_data_device_send_motion(active_drag_target_device, get_timestamp_ms(),
                               wl_fixed_from_int(x), wl_fixed_from_int(y));
  }
}

void bridge_data_device_pointer_button(struct bridge_surface *surface,
                                       bool pressed, int32_t x, int32_t y) {
  if (pressed || !active_drag_source)
    return;

  bridge_data_device_pointer_motion(surface, x, y);

  if (active_drag_has_target && active_drag_target_device) {
    wl_data_device_send_drop(active_drag_target_device);
    wl_data_source_send_dnd_drop_performed(active_drag_source);
    active_drag_target_device = NULL;
    active_drag_target_surface = NULL;
    active_drag_has_target = false;
  } else {
    wl_data_source_send_cancelled(active_drag_source);
    clear_drag_target();
  }

  active_drag_source = NULL;
}

static void data_device_set_selection(struct wl_client *client,
                                      struct wl_resource *resource,
                                      struct wl_resource *source,
                                      uint32_t serial) {
  (void)serial;

  if (source && wl_resource_get_client(source) != client) {
    LOG_WARN("wl_data_device.set_selection ignored: source does not belong to "
             "requesting client");
    return;
  }

  if (source && !wl_resource_instance_of(source, &wl_data_source_interface,
                                         &data_source_implementation)) {
    LOG_WARN(
        "wl_data_device.set_selection ignored: source is not wl_data_source");
    return;
  }

  if (current_selection_source && current_selection_source != source) {
    wl_data_source_send_cancelled(current_selection_source);
  }

  external_selection_active = false;
  external_selection_text[0] = '\0';
  current_selection_source = source;
  if (!source) {

    notify_clipboard_listeners();
    if (bridge && bridge->plexy_conn)
      plexy_set_clipboard_text(bridge->plexy_conn, NULL, "");
  } else {

    mirror_selection_to_plexy_clipboard(source);
  }
  bridge_data_device_send_selection_to_focus();
}

static void data_device_release(struct wl_client *client,
                                struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct wl_data_device_interface data_device_implementation = {
    .start_drag = data_device_start_drag,
    .set_selection = data_device_set_selection,
    .release = data_device_release,
};

static void data_device_resource_destroy(struct wl_resource *resource) {
  struct data_device_data *device_data = wl_resource_get_user_data(resource);
  if (!device_data)
    return;

  wl_list_remove(&device_data->link);
  free(device_data);
}

static void data_device_manager_create_data_source(struct wl_client *client,
                                                   struct wl_resource *resource,
                                                   uint32_t id) {
  ensure_data_device_state();

  struct wl_resource *source = wl_resource_create(
      client, &wl_data_source_interface, wl_resource_get_version(resource), id);

  if (!source) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct data_source_data *source_data = calloc(1, sizeof(*source_data));
  if (!source_data) {
    wl_resource_destroy(source);
    wl_resource_post_no_memory(resource);
    return;
  }

  source_data->resource = source;
  source_data->client = client;
  wl_array_init(&source_data->mime_types);

  wl_resource_set_implementation(source, &data_source_implementation,
                                 source_data, data_source_resource_destroy);
}

static void data_device_manager_get_data_device(struct wl_client *client,
                                                struct wl_resource *resource,
                                                uint32_t id,
                                                struct wl_resource *seat) {
  ensure_data_device_state();

  struct wl_resource *device = wl_resource_create(
      client, &wl_data_device_interface, wl_resource_get_version(resource), id);

  if (!device) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct data_device_data *device_data = calloc(1, sizeof(*device_data));
  if (!device_data) {
    wl_resource_destroy(device);
    wl_resource_post_no_memory(resource);
    return;
  }

  device_data->resource = device;
  device_data->client = client;
  wl_list_insert(&data_devices, &device_data->link);

  wl_resource_set_implementation(device, &data_device_implementation,
                                 device_data, data_device_resource_destroy);
  (void)seat;

  bridge_data_device_send_selection_to_client(client);
}

static const struct wl_data_device_manager_interface
    data_device_manager_implementation = {
        .create_data_source = data_device_manager_create_data_source,
        .get_data_device = data_device_manager_get_data_device,
};

void bind_data_device_manager(struct wl_client *client, void *data,
                              uint32_t version, uint32_t id) {
  ensure_data_device_state();

  struct wl_resource *resource = wl_resource_create(
      client, &wl_data_device_manager_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &data_device_manager_implementation,
                                 NULL, NULL);
  (void)data;
}
