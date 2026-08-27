/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#define _GNU_SOURCE
#include "wayland_bridge.h"
#include <fcntl.h>
#include <linux/memfd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-server-protocol.h>
#include <xkbcommon/xkbcommon.h>

static char *keymap_string = NULL;
static size_t keymap_size = 0;

static void init_keymap(void) {
  if (keymap_string)
    return;

  struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (!ctx) {
    LOG_ERROR("Failed to create xkb context");
    return;
  }

  struct xkb_rule_names names = {.rules = "evdev",
                                 .model = "pc105",
                                 .layout = "us",
                                 .variant = "",
                                 .options = ""};

  struct xkb_keymap *keymap =
      xkb_keymap_new_from_names(ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (!keymap) {
    LOG_ERROR("Failed to create xkb keymap");
    xkb_context_unref(ctx);
    return;
  }

  keymap_string = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
  if (keymap_string) {
    keymap_size = strlen(keymap_string) + 1;
    LOG_DEBUG("XKB keymap initialized: %zu bytes", keymap_size);
  }

  xkb_keymap_unref(keymap);
  xkb_context_unref(ctx);
}

struct seat_resource {
  struct wl_resource *resource;
  struct wl_list link;
};

struct pointer_client {
  struct wl_resource *resource;
  struct wl_client *client;
  struct wl_list link;
};

struct keyboard_client {
  struct wl_resource *resource;
  struct wl_client *client;
  struct wl_list link;
};

struct touch_client {
  struct wl_resource *resource;
  struct wl_client *client;
  struct wl_list link;
};

static void pointer_set_cursor(struct wl_client *client,
                               struct wl_resource *resource, uint32_t serial,
                               struct wl_resource *surface, int32_t hotspot_x,
                               int32_t hotspot_y) {
  (void)client;
  (void)serial;
  (void)hotspot_x;
  (void)hotspot_y;

  struct bridge_surface *focus = bridge ? bridge->pointer_focus : NULL;
  if (!bridge || !bridge->plexy_conn || !focus)
    return;
  if (focus->plexy_window_id == 0)
    return;

  if (focus->resource && wl_resource_get_client(focus->resource) !=
                             wl_resource_get_client(resource))
    return;

  if (!surface) {
    bridge_surface_update_cursor_shape(focus, 0);
    bridge->seat_pointer_surface = NULL;
    return;
  }

  bridge->seat_pointer_surface = surface;
  bridge_surface_update_cursor_shape(focus, 1);
}

static void pointer_release(struct wl_client *client,
                            struct wl_resource *resource) {
  LOG_TRACE("pointer: released");
  wl_resource_destroy(resource);
}

static void pointer_resource_destroy(struct wl_resource *resource) {
  struct pointer_client *pc = wl_resource_get_user_data(resource);
  if (pc) {
    LOG_TRACE("pointer: destroyed");
    wl_list_remove(&pc->link);
    free(pc);
  }
  if (bridge->seat_pointer_resource == resource) {
    bridge->seat_pointer_resource = NULL;
    bridge->seat_pointer_surface = NULL;
  }
}

static const struct wl_pointer_interface pointer_implementation = {
    .set_cursor = pointer_set_cursor,
    .release = pointer_release,
};

static void keyboard_release(struct wl_client *client,
                             struct wl_resource *resource) {
  LOG_TRACE("keyboard: released");
  wl_resource_destroy(resource);
}

static const struct wl_keyboard_interface keyboard_implementation = {
    .release = keyboard_release,
};

static void touch_release(struct wl_client *client,
                          struct wl_resource *resource) {
  (void)client;
  LOG_TRACE("touch: released");
  wl_resource_destroy(resource);
}

static const struct wl_touch_interface touch_implementation = {
    .release = touch_release,
};

static void keyboard_resource_destroy(struct wl_resource *resource) {
  struct keyboard_client *kc = wl_resource_get_user_data(resource);
  if (kc) {
    LOG_TRACE("keyboard: destroyed");
    wl_list_remove(&kc->link);
    free(kc);
  }
}

static void touch_resource_destroy(struct wl_resource *resource) {
  struct touch_client *tc = wl_resource_get_user_data(resource);
  if (tc) {
    LOG_TRACE("touch: destroyed");
    wl_list_remove(&tc->link);
    free(tc);
  }
}

static void seat_get_pointer(struct wl_client *client,
                             struct wl_resource *resource, uint32_t id) {
  LOG_INFO("seat_get_pointer called by client=%p id=%u", (void *)client, id);
  struct wl_resource *pointer = wl_resource_create(
      client, &wl_pointer_interface, wl_resource_get_version(resource), id);

  if (!pointer) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct pointer_client *pc = calloc(1, sizeof(*pc));
  if (!pc) {
    wl_resource_destroy(pointer);
    wl_resource_post_no_memory(resource);
    return;
  }
  pc->resource = pointer;
  pc->client = client;
  wl_list_insert(&bridge->pointer_clients, &pc->link);
  LOG_DEBUG("pointer: created");

  wl_resource_set_implementation(pointer, &pointer_implementation, pc,
                                 pointer_resource_destroy);
}

static void seat_get_keyboard(struct wl_client *client,
                              struct wl_resource *resource, uint32_t id) {
  struct wl_resource *keyboard = wl_resource_create(
      client, &wl_keyboard_interface, wl_resource_get_version(resource), id);

  if (!keyboard) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct keyboard_client *kc = calloc(1, sizeof(*kc));
  if (!kc) {
    wl_resource_destroy(keyboard);
    wl_resource_post_no_memory(resource);
    return;
  }
  kc->resource = keyboard;
  kc->client = client;
  wl_list_insert(&bridge->keyboard_clients, &kc->link);
  LOG_DEBUG("keyboard: created");

  wl_resource_set_implementation(keyboard, &keyboard_implementation, kc,
                                 keyboard_resource_destroy);

  if (wl_resource_get_version(keyboard) >=
      WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION) {
    wl_keyboard_send_repeat_info(keyboard, 25, 600);
  }

  init_keymap();

  if (!keymap_string || keymap_size == 0) {
    LOG_ERROR("No keymap available");
    return;
  }

  int fd = memfd_create("wayland-keymap", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) {
    LOG_ERROR("Failed to create keymap memfd");
    return;
  }

  if (ftruncate(fd, keymap_size) < 0) {
    LOG_ERROR("Failed to truncate keymap memfd");
    close(fd);
    return;
  }

  void *data =
      mmap(NULL, keymap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    LOG_ERROR("Failed to mmap keymap memfd");
    close(fd);
    return;
  }

  memcpy(data, keymap_string, keymap_size);
  munmap(data, keymap_size);

  wl_keyboard_send_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd,
                          keymap_size);
  close(fd);

  LOG_DEBUG("keyboard: sent keymap (%zu bytes)", keymap_size);
}

static void seat_get_touch(struct wl_client *client,
                           struct wl_resource *resource, uint32_t id) {
  struct wl_resource *touch = wl_resource_create(
      client, &wl_touch_interface, wl_resource_get_version(resource), id);

  if (!touch) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct touch_client *tc = calloc(1, sizeof(*tc));
  if (!tc) {
    wl_resource_destroy(touch);
    wl_resource_post_no_memory(resource);
    return;
  }
  tc->resource = touch;
  tc->client = client;
  wl_list_insert(&bridge->touch_clients, &tc->link);
  LOG_DEBUG("touch: created");

  wl_resource_set_implementation(touch, &touch_implementation, tc,
                                 touch_resource_destroy);
}

static void seat_release(struct wl_client *client,
                         struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static const struct wl_seat_interface seat_implementation = {
    .get_pointer = seat_get_pointer,
    .get_keyboard = seat_get_keyboard,
    .get_touch = seat_get_touch,
    .release = seat_release,
};

static void seat_resource_destroy(struct wl_resource *resource) {
  struct seat_resource *sr = wl_resource_get_user_data(resource);
  if (sr) {
    wl_list_remove(&sr->link);
    free(sr);
  }
}

void bind_seat(struct wl_client *client, void *data, uint32_t version,
               uint32_t id) {
  struct wl_resource *resource =
      wl_resource_create(client, &wl_seat_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  struct seat_resource *sr = calloc(1, sizeof(*sr));
  if (!sr) {
    wl_resource_destroy(resource);
    wl_client_post_no_memory(client);
    return;
  }

  sr->resource = resource;
  wl_list_insert(&bridge->seat_resources, &sr->link);

  wl_resource_set_implementation(resource, &seat_implementation, sr,
                                 seat_resource_destroy);

  wl_seat_send_capabilities(resource, WL_SEAT_CAPABILITY_POINTER |
                                          WL_SEAT_CAPABILITY_KEYBOARD |
                                          WL_SEAT_CAPABILITY_TOUCH);

  if (version >= WL_SEAT_NAME_SINCE_VERSION) {
    wl_seat_send_name(resource, "seat0");
  }

  LOG_DEBUG("Seat bound");
}

static struct wl_resource *pointer_for_client(struct wl_client *client) {
  struct pointer_client *pc;
  int count = 0;
  wl_list_for_each(pc, &bridge->pointer_clients, link) {
    count++;
    if (pc->client == client)
      return pc->resource;
  }

  LOG_TRACE(
      "pointer_for_client: client=%p has not created a pointer resource yet",
      (void *)client);
  return NULL;
}

int bridge_for_each_pointer(struct bridge_surface *surface,
                            void (*callback)(struct wl_resource *pointer,
                                             void *data),
                            void *data) {
  if (!surface || !surface->resource)
    return 0;

  struct wl_client *client = wl_resource_get_client(surface->resource);
  struct pointer_client *pc;
  int count = 0;

  wl_list_for_each(pc, &bridge->pointer_clients, link) {
    if (pc->client == client) {
      callback(pc->resource, data);
      count++;
    }
  }
  return count;
}

static struct wl_resource *keyboard_for_client(struct wl_client *client) {
  struct keyboard_client *kc;
  int count = 0;
  wl_list_for_each(kc, &bridge->keyboard_clients, link) {
    count++;
    if (kc->client == client)
      return kc->resource;
  }

  if (count == 0) {
    LOG_WARN("keyboard_for_client: no keyboards registered");
  }
  return NULL;
}

int bridge_for_each_keyboard(struct bridge_surface *surface,
                             void (*callback)(struct wl_resource *keyboard,
                                              void *data),
                             void *data) {
  if (!surface || !surface->resource)
    return 0;

  struct wl_client *client = wl_resource_get_client(surface->resource);
  struct keyboard_client *kc;
  int count = 0;

  wl_list_for_each(kc, &bridge->keyboard_clients, link) {
    if (kc->client == client) {
      callback(kc->resource, data);
      count++;
    }
  }
  return count;
}

struct wl_resource *
bridge_get_pointer_for_surface(struct bridge_surface *surface) {
  if (!surface || !surface->resource) {
    LOG_WARN("bridge_get_pointer_for_surface: surface or resource is NULL");
    return NULL;
  }
  struct wl_client *client = wl_resource_get_client(surface->resource);
  struct wl_resource *ptr = pointer_for_client(client);
  if (!ptr) {
    LOG_TRACE("bridge_get_pointer_for_surface: no pointer for client");
  }
  return ptr;
}

struct wl_resource *
bridge_get_keyboard_for_surface(struct bridge_surface *surface) {
  if (!surface || !surface->resource) {
    LOG_WARN("bridge_get_keyboard_for_surface: surface or resource is NULL");
    return NULL;
  }
  struct wl_client *client = wl_resource_get_client(surface->resource);
  struct wl_resource *kbd = keyboard_for_client(client);
  if (!kbd) {
    LOG_TRACE("bridge_get_keyboard_for_surface: no keyboard for client");
  }
  return kbd;
}

static struct wl_resource *touch_for_client(struct wl_client *client) {
  struct touch_client *tc;
  wl_list_for_each(tc, &bridge->touch_clients, link) {
    if (tc->client == client)
      return tc->resource;
  }
  return NULL;
}

struct wl_resource *
bridge_get_touch_for_surface(struct bridge_surface *surface) {
  if (!surface || !surface->resource) {
    return NULL;
  }
  struct wl_client *client = wl_resource_get_client(surface->resource);
  return touch_for_client(client);
}
