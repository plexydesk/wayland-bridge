/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#define _GNU_SOURCE
#include "text-input-v3-protocol.h"
#include "wayland_bridge.h"
#include "xx-input-method-v2-protocol.h"
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct text_input_client {
  struct wl_resource *resource;
  struct wl_client *client;
  struct wl_resource *entered_surface;
  bool enabled;
  uint32_t serial;

  char *surrounding_text;
  int32_t surrounding_cursor;
  int32_t surrounding_anchor;
  uint32_t text_change_cause;
  uint32_t content_hint;
  uint32_t content_purpose;
  int32_t cursor_x;
  int32_t cursor_y;
  int32_t cursor_width;
  int32_t cursor_height;

  struct wl_list link;
};

static struct text_input_client *
text_input_client_from_resource(struct wl_resource *resource) {
  return (struct text_input_client *)wl_resource_get_user_data(resource);
}

static bool client_matches_surface(struct text_input_client *tic,
                                   struct bridge_surface *surface) {
  if (!tic || !surface || !surface->resource)
    return false;
  return tic->client == wl_resource_get_client(surface->resource);
}

static void text_input_send_done(struct text_input_client *tic) {
  if (!tic || !tic->resource)
    return;
  tic->serial++;
  zwp_text_input_v3_send_done(tic->resource, tic->serial);
}

static void text_input_send_enter_if_needed(struct text_input_client *tic,
                                            struct bridge_surface *surface) {
  if (!tic || !surface || !surface->resource || !tic->enabled)
    return;
  if (tic->entered_surface == surface->resource)
    return;
  tic->entered_surface = surface->resource;
  zwp_text_input_v3_send_enter(tic->resource, surface->resource);
  text_input_send_done(tic);
}

static void text_input_send_leave_if_needed(struct text_input_client *tic,
                                            struct bridge_surface *surface) {
  if (!tic || !surface || !surface->resource)
    return;
  if (tic->entered_surface != surface->resource)
    return;
  zwp_text_input_v3_send_leave(tic->resource, surface->resource);
  tic->entered_surface = NULL;
  text_input_send_done(tic);
}

static char keycode_to_ascii(uint32_t keycode, bool shift) {
  if (keycode >= KEY_A && keycode <= KEY_Z) {
    char base = shift ? 'A' : 'a';
    return (char)(base + (char)(keycode - KEY_A));
  }
  if (keycode >= KEY_1 && keycode <= KEY_0) {
    static const char normal[] = {'1', '2', '3', '4', '5',
                                  '6', '7', '8', '9', '0'};
    static const char shifted[] = {'!', '@', '#', '$', '%',
                                   '^', '&', '*', '(', ')'};
    size_t idx = keycode - KEY_1;
    return shift ? shifted[idx] : normal[idx];
  }

  switch (keycode) {
  case KEY_SPACE:
    return ' ';
  case KEY_MINUS:
    return shift ? '_' : '-';
  case KEY_EQUAL:
    return shift ? '+' : '=';
  case KEY_LEFTBRACE:
    return shift ? '{' : '[';
  case KEY_RIGHTBRACE:
    return shift ? '}' : ']';
  case KEY_BACKSLASH:
    return shift ? '|' : '\\';
  case KEY_SEMICOLON:
    return shift ? ':' : ';';
  case KEY_APOSTROPHE:
    return shift ? '"' : '\'';
  case KEY_GRAVE:
    return shift ? '~' : '`';
  case KEY_COMMA:
    return shift ? '<' : ',';
  case KEY_DOT:
    return shift ? '>' : '.';
  case KEY_SLASH:
    return shift ? '?' : '/';
  case KEY_TAB:
    return '\t';
  case KEY_ENTER:
    return '\n';
  default:
    return 0;
  }
}

struct wl_resource *bridge_ime_get_focused_text_input(void) {
  if (!bridge || !bridge->keyboard_focus || !bridge->keyboard_focus->resource)
    return NULL;

  struct wl_client *focus_client =
      wl_resource_get_client(bridge->keyboard_focus->resource);

  struct text_input_client *tic;
  wl_list_for_each(tic, &bridge->text_input_clients, link) {
    if (tic->enabled && tic->client == focus_client)
      return tic->resource;
  }
  return NULL;
}

void bridge_ime_send_text_input_done(struct wl_resource *ti_resource) {
  struct text_input_client *tic = wl_resource_get_user_data(ti_resource);
  if (tic)
    text_input_send_done(tic);
}

void bridge_ime_forward_text_input_state(struct wl_resource *im) {
  struct wl_resource *ti = bridge_ime_get_focused_text_input();
  if (!ti)
    return;

  struct text_input_client *tic = wl_resource_get_user_data(ti);
  if (!tic)
    return;

  if (tic->surrounding_text)
    xx_input_method_v1_send_surrounding_text(im, tic->surrounding_text,
                                             (uint32_t)tic->surrounding_cursor,
                                             (uint32_t)tic->surrounding_anchor);
  xx_input_method_v1_send_text_change_cause(im, tic->text_change_cause);
  xx_input_method_v1_send_content_type(im, tic->content_hint,
                                       tic->content_purpose);
}

void bridge_text_input_try_commit_key(struct bridge_surface *surface,
                                      uint32_t keycode, uint32_t modifiers) {
  if (!bridge || !surface || !surface->resource)
    return;

  bool shift = (modifiers & PLEXY_MOD_SHIFT) != 0;
  char committed = keycode_to_ascii(keycode, shift);

  struct text_input_client *tic;
  wl_list_for_each(tic, &bridge->text_input_clients, link) {
    if (!client_matches_surface(tic, surface) || !tic->enabled) {
      continue;
    }

    if (tic->entered_surface != surface->resource) {
      text_input_send_enter_if_needed(tic, surface);
    }

    if (keycode == KEY_BACKSPACE) {
      zwp_text_input_v3_send_delete_surrounding_text(tic->resource, 1, 0);
      text_input_send_done(tic);
      continue;
    }

    if (committed != 0) {
      char text[2] = {committed, '\0'};
      zwp_text_input_v3_send_preedit_string(tic->resource, "", 0, 0);
      zwp_text_input_v3_send_commit_string(tic->resource, text);
      text_input_send_done(tic);
    }
  }
}

void bridge_text_input_notify_focus(struct bridge_surface *surface,
                                    bool focused) {
  if (!bridge || !surface || !surface->resource)
    return;

  struct text_input_client *tic;
  wl_list_for_each(tic, &bridge->text_input_clients, link) {
    if (!client_matches_surface(tic, surface)) {
      continue;
    }
    if (focused) {
      text_input_send_enter_if_needed(tic, surface);
    } else {
      text_input_send_leave_if_needed(tic, surface);
    }
  }
}

static void text_input_destroy(struct wl_client *client,
                               struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void text_input_enable(struct wl_client *client,
                              struct wl_resource *resource) {
  (void)client;
  struct text_input_client *tic = text_input_client_from_resource(resource);
  if (!tic)
    return;

  tic->enabled = true;

  if (bridge && bridge->keyboard_focus &&
      client_matches_surface(tic, bridge->keyboard_focus)) {
    text_input_send_enter_if_needed(tic, bridge->keyboard_focus);
  }

  bridge_ime_notify_text_input_state(true);
}

static void text_input_disable(struct wl_client *client,
                               struct wl_resource *resource) {
  (void)client;
  struct text_input_client *tic = text_input_client_from_resource(resource);
  if (!tic)
    return;

  bridge_ime_notify_text_input_state(false);

  if (tic->entered_surface) {
    zwp_text_input_v3_send_leave(resource, tic->entered_surface);
    tic->entered_surface = NULL;
  }
  tic->enabled = false;
  text_input_send_done(tic);
}

static void text_input_set_surrounding_text(struct wl_client *client,
                                            struct wl_resource *resource,
                                            const char *text, int32_t cursor,
                                            int32_t anchor) {
  (void)client;
  struct text_input_client *tic = text_input_client_from_resource(resource);
  if (!tic)
    return;

  free(tic->surrounding_text);
  tic->surrounding_text = text ? strdup(text) : NULL;
  tic->surrounding_cursor = cursor;
  tic->surrounding_anchor = anchor;
}

static void text_input_set_text_change_cause(struct wl_client *client,
                                             struct wl_resource *resource,
                                             uint32_t cause) {
  (void)client;
  struct text_input_client *tic = text_input_client_from_resource(resource);
  if (!tic)
    return;
  tic->text_change_cause = cause;
}

static void text_input_set_content_type(struct wl_client *client,
                                        struct wl_resource *resource,
                                        uint32_t hint, uint32_t purpose) {
  (void)client;
  struct text_input_client *tic = text_input_client_from_resource(resource);
  if (!tic)
    return;
  tic->content_hint = hint;
  tic->content_purpose = purpose;
}

static void text_input_set_cursor_rectangle(struct wl_client *client,
                                            struct wl_resource *resource,
                                            int32_t x, int32_t y, int32_t width,
                                            int32_t height) {
  (void)client;
  struct text_input_client *tic = text_input_client_from_resource(resource);
  if (!tic)
    return;
  tic->cursor_x = x;
  tic->cursor_y = y;
  tic->cursor_width = width;
  tic->cursor_height = height;
}

static void text_input_commit(struct wl_client *client,
                              struct wl_resource *resource) {
  (void)client;
  struct text_input_client *tic = text_input_client_from_resource(resource);
  if (!tic)
    return;
  text_input_send_done(tic);
}

static const struct zwp_text_input_v3_interface text_input_implementation = {
    .destroy = text_input_destroy,
    .enable = text_input_enable,
    .disable = text_input_disable,
    .set_surrounding_text = text_input_set_surrounding_text,
    .set_text_change_cause = text_input_set_text_change_cause,
    .set_content_type = text_input_set_content_type,
    .set_cursor_rectangle = text_input_set_cursor_rectangle,
    .commit = text_input_commit,
};

static void text_input_resource_destroy(struct wl_resource *resource) {
  struct text_input_client *tic = text_input_client_from_resource(resource);
  if (!tic)
    return;

  wl_list_remove(&tic->link);
  free(tic->surrounding_text);
  free(tic);
}

static void text_input_manager_destroy(struct wl_client *client,
                                       struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void text_input_manager_get_text_input(struct wl_client *client,
                                              struct wl_resource *resource,
                                              uint32_t id,
                                              struct wl_resource *seat) {
  (void)seat;
  struct wl_resource *text_input =
      wl_resource_create(client, &zwp_text_input_v3_interface,
                         wl_resource_get_version(resource), id);

  if (!text_input) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct text_input_client *tic = calloc(1, sizeof(*tic));
  if (!tic) {
    wl_resource_destroy(text_input);
    wl_resource_post_no_memory(resource);
    return;
  }

  tic->resource = text_input;
  tic->client = client;
  tic->enabled = false;
  wl_list_insert(&bridge->text_input_clients, &tic->link);

  wl_resource_set_implementation(text_input, &text_input_implementation, tic,
                                 text_input_resource_destroy);
}

static const struct zwp_text_input_manager_v3_interface
    text_input_manager_implementation = {
        .destroy = text_input_manager_destroy,
        .get_text_input = text_input_manager_get_text_input,
};

void bind_text_input_manager(struct wl_client *client, void *data,
                             uint32_t version, uint32_t id) {
  (void)data;
  uint32_t bind_version = version > 1 ? 1 : version;
  struct wl_resource *resource = wl_resource_create(
      client, &zwp_text_input_manager_v3_interface, bind_version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &text_input_manager_implementation,
                                 NULL, NULL);
}
