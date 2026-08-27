/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */


#define _GNU_SOURCE
#include "ext-idle-notify-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>
#include <time.h>

struct idle_notification {
  struct wl_resource *resource;
  struct wl_event_source *timer;
  uint32_t timeout_ms;
  bool idled;
  bool ignore_inhibitors;
  struct wl_list link;
};

static struct wl_list idle_notifications;
static bool idle_notifications_initialized = false;

static void ensure_list_init(void) {
  if (!idle_notifications_initialized) {
    wl_list_init(&idle_notifications);
    idle_notifications_initialized = true;
  }
}

static int idle_timer_callback(void *data) {
  struct idle_notification *notif = data;
  if (!notif || !notif->resource || notif->idled)
    return 0;

  notif->idled = true;
  ext_idle_notification_v1_send_idled(notif->resource);
  return 0;
}

void bridge_idle_notify_activity(void) {
  if (!idle_notifications_initialized)
    return;

  struct idle_notification *notif;
  wl_list_for_each(notif, &idle_notifications, link) {
    if (notif->idled) {
      notif->idled = false;
      ext_idle_notification_v1_send_resumed(notif->resource);
    }
    if (notif->timer)
      wl_event_source_timer_update(notif->timer, notif->timeout_ms);
  }
}

static void notification_destroy(struct wl_client *client,
                                 struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct ext_idle_notification_v1_interface notification_impl = {
    .destroy = notification_destroy,
};

static void notification_resource_destroy(struct wl_resource *resource) {
  struct idle_notification *notif = wl_resource_get_user_data(resource);
  if (!notif)
    return;
  if (notif->timer)
    wl_event_source_remove(notif->timer);
  wl_list_remove(&notif->link);
  free(notif);
}

static void create_notification(struct wl_client *client,
                                struct wl_resource *resource, uint32_t id,
                                uint32_t timeout, struct wl_resource *seat,
                                bool ignore_inhibitors) {
  (void)resource;
  (void)seat;
  ensure_list_init();

  struct wl_resource *notif_res =
      wl_resource_create(client, &ext_idle_notification_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!notif_res) {
    wl_client_post_no_memory(client);
    return;
  }

  struct idle_notification *notif = calloc(1, sizeof(*notif));
  if (!notif) {
    wl_resource_destroy(notif_res);
    wl_resource_post_no_memory(resource);
    return;
  }

  notif->resource = notif_res;
  notif->timeout_ms = timeout;
  notif->idled = false;
  notif->ignore_inhibitors = ignore_inhibitors;
  wl_list_insert(&idle_notifications, &notif->link);

  wl_resource_set_implementation(notif_res, &notification_impl, notif,
                                 notification_resource_destroy);

  if (bridge && bridge->display) {
    struct wl_event_loop *loop = wl_display_get_event_loop(bridge->display);
    notif->timer = wl_event_loop_add_timer(loop, idle_timer_callback, notif);
    if (notif->timer)
      wl_event_source_timer_update(notif->timer, timeout);
  }
}

static void notifier_destroy(struct wl_client *client,
                             struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void notifier_get_idle_notification(struct wl_client *client,
                                           struct wl_resource *resource,
                                           uint32_t id, uint32_t timeout,
                                           struct wl_resource *seat) {
  create_notification(client, resource, id, timeout, seat, false);
}

static void notifier_get_input_idle_notification(struct wl_client *client,
                                                 struct wl_resource *resource,
                                                 uint32_t id, uint32_t timeout,
                                                 struct wl_resource *seat) {
  create_notification(client, resource, id, timeout, seat, true);
}

static const struct ext_idle_notifier_v1_interface notifier_impl = {
    .destroy = notifier_destroy,
    .get_idle_notification = notifier_get_idle_notification,
    .get_input_idle_notification = notifier_get_input_idle_notification,
};

void bind_idle_notifier(struct wl_client *client, void *data, uint32_t version,
                        uint32_t id) {
  (void)data;
  struct wl_resource *resource =
      wl_resource_create(client, &ext_idle_notifier_v1_interface, version, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &notifier_impl, NULL, NULL);
}
