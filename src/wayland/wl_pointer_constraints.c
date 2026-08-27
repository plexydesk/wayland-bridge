/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "pointer-constraints-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdint.h>
#include <stdlib.h>

enum constraint_type {
  CONSTRAINT_LOCK,
  CONSTRAINT_CONFINE,
};

struct bridge_region_resource_shadow {
  struct bridge_region_state state;
};

struct pointer_constraint {
  struct wl_resource *resource;
  struct bridge_surface *surface;
  struct wl_resource *pointer;
  struct wl_resource *region;
  enum constraint_type type;
  uint32_t lifetime;
  bool active;
  bool hint_set;
  int32_t hint_x;
  int32_t hint_y;
  struct wl_list link;
};

static struct wl_list constraints;
static bool constraints_initialized = false;
static bool raw_input_sync_pending = false;

static void ensure_constraints_init(void) {
  if (constraints_initialized) {
    return;
  }
  wl_list_init(&constraints);
  constraints_initialized = true;
}

static bool point_in_region_resource(struct wl_resource *region, int32_t x,
                                     int32_t y) {
  if (!region) {
    return true;
  }

  struct bridge_region_resource_shadow *region_res =
      wl_resource_get_user_data(region);
  if (!region_res) {
    return true;
  }

  bool inside = false;
  for (size_t i = 0; i < region_res->state.count; i++) {
    const struct bridge_region_op *op = &region_res->state.ops[i];
    if (op->width <= 0 || op->height <= 0) {
      continue;
    }

    int64_t right = (int64_t)op->x + (int64_t)op->width;
    int64_t bottom = (int64_t)op->y + (int64_t)op->height;
    if (x >= op->x && y >= op->y && (int64_t)x < right && (int64_t)y < bottom) {
      inside = op->add;
    }
  }

  return inside;
}

static void send_constraint_state(struct pointer_constraint *constraint,
                                  bool active) {
  if (!constraint || constraint->active == active) {
    return;
  }

  bool surface_locked_before = false;
  if (constraint->type == CONSTRAINT_LOCK) {
    surface_locked_before =
        bridge_pointer_constraints_is_locked(constraint->surface);
  }

  bridge_log("constraint %s: type=%s lifetime=%s surface=%p",
             active ? "ACTIVATE" : "DEACTIVATE",
             constraint->type == CONSTRAINT_LOCK ? "lock" : "confine",
             constraint->lifetime ==
                     ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT
                 ? "PERSISTENT"
                 : "ONESHOT",
             (void *)constraint->surface);

  if (constraint->type == CONSTRAINT_LOCK) {
    if (active) {
      zwp_locked_pointer_v1_send_locked(constraint->resource);
    } else {
      zwp_locked_pointer_v1_send_unlocked(constraint->resource);
    }
  } else {
    if (active) {
      zwp_confined_pointer_v1_send_confined(constraint->resource);
    } else {
      zwp_confined_pointer_v1_send_unconfined(constraint->resource);
    }
  }

  constraint->active = active;

  if (constraint->type == CONSTRAINT_LOCK) {
    bool surface_locked_after =
        bridge_pointer_constraints_is_locked(constraint->surface);
    if (surface_locked_before != surface_locked_after) {
      bridge_surface_set_pointer_lock_cursor_hidden(constraint->surface,
                                                    surface_locked_after);
    }
  }
}

static bool raw_input_should_be_enabled = false;

static void sync_raw_input_idle(void *data) {
  (void)data;
  raw_input_sync_pending = false;

  int count = 0;
  bool any_active = false;
  struct pointer_constraint *c;
  wl_list_for_each(c, &constraints, link) {
    count++;
    if (c->active) {
      any_active = true;
    }
  }
  bridge_log("raw_input_sync: %d constraints, any_active=%d -> %s (was %s)",
             count, any_active, any_active ? "ENABLE" : "DISABLE",
             raw_input_should_be_enabled ? "enabled" : "disabled");

  if (any_active && !raw_input_should_be_enabled) {
    bridge_enable_raw_input();
    raw_input_should_be_enabled = true;
  } else if (!any_active && raw_input_should_be_enabled) {
    bridge_disable_raw_input();
    raw_input_should_be_enabled = false;
  }
}

static void schedule_raw_input_sync(void) {
  if (raw_input_sync_pending)
    return;
  if (!bridge || !bridge->loop)
    return;
  struct wl_event_source *src =
      wl_event_loop_add_idle(bridge->loop, sync_raw_input_idle, NULL);
  if (src)
    raw_input_sync_pending = true;
}

static void pointer_constraint_resource_destroy(struct wl_resource *resource) {
  struct pointer_constraint *constraint = wl_resource_get_user_data(resource);
  if (!constraint) {
    return;
  }

  bridge_log("constraint DESTROY: type=%s lifetime=%s active=%d surface=%p",
             constraint->type == CONSTRAINT_LOCK ? "lock" : "confine",
             constraint->lifetime ==
                     ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT
                 ? "PERSISTENT"
                 : "ONESHOT",
             constraint->active, (void *)constraint->surface);

  bool was_active = constraint->active;
  wl_list_remove(&constraint->link);
  free(constraint);

  if (was_active)
    schedule_raw_input_sync();
}

static struct pointer_constraint *
find_constraint(struct bridge_surface *surface, struct wl_resource *pointer) {
  if (!constraints_initialized || !surface || !pointer) {
    return NULL;
  }

  struct pointer_constraint *c;
  wl_list_for_each(c, &constraints, link) {
    if (c->surface == surface && c->pointer == pointer) {
      return c;
    }
  }
  return NULL;
}

static void locked_pointer_destroy(struct wl_client *client,
                                   struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void
locked_pointer_set_cursor_position_hint(struct wl_client *client,
                                        struct wl_resource *resource,
                                        wl_fixed_t x, wl_fixed_t y) {
  (void)client;
  struct pointer_constraint *constraint = wl_resource_get_user_data(resource);
  if (!constraint) {
    return;
  }

  constraint->hint_set = true;
  constraint->hint_x = wl_fixed_to_int(x);
  constraint->hint_y = wl_fixed_to_int(y);
}

static void locked_pointer_set_region(struct wl_client *client,
                                      struct wl_resource *resource,
                                      struct wl_resource *region) {
  (void)client;
  struct pointer_constraint *constraint = wl_resource_get_user_data(resource);
  if (!constraint) {
    return;
  }
  constraint->region = region;
}

static const struct zwp_locked_pointer_v1_interface locked_pointer_impl = {
    .destroy = locked_pointer_destroy,
    .set_cursor_position_hint = locked_pointer_set_cursor_position_hint,
    .set_region = locked_pointer_set_region,
};

static void confined_pointer_destroy(struct wl_client *client,
                                     struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void confined_pointer_set_region(struct wl_client *client,
                                        struct wl_resource *resource,
                                        struct wl_resource *region) {
  (void)client;
  struct pointer_constraint *constraint = wl_resource_get_user_data(resource);
  if (!constraint) {
    return;
  }
  constraint->region = region;
}

static const struct zwp_confined_pointer_v1_interface confined_pointer_impl = {
    .destroy = confined_pointer_destroy,
    .set_region = confined_pointer_set_region,
};

static void pointer_constraints_destroy(struct wl_client *client,
                                        struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void create_constraint(struct wl_client *client,
                              struct wl_resource *resource, uint32_t id,
                              struct wl_resource *surface_resource,
                              struct wl_resource *pointer,
                              struct wl_resource *region, uint32_t lifetime,
                              enum constraint_type type) {
  if (!surface_resource || !pointer) {
    wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                           "pointer constraints requires surface and pointer");
    return;
  }

  if (lifetime != ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT &&
      lifetime != ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT) {
    wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_METHOD,
                           "invalid pointer constraints lifetime %u", lifetime);
    return;
  }

  struct bridge_surface *surface =
      bridge_surface_from_resource(surface_resource);
  if (!surface) {
    wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                           "invalid wl_surface for pointer constraints");
    return;
  }

  ensure_constraints_init();

  if (find_constraint(surface, pointer)) {
    wl_resource_post_error(resource,
                           ZWP_POINTER_CONSTRAINTS_V1_ERROR_ALREADY_CONSTRAINED,
                           "pointer already constrained for this surface");
    return;
  }

  const struct wl_interface *iface = (type == CONSTRAINT_LOCK)
                                         ? &zwp_locked_pointer_v1_interface
                                         : &zwp_confined_pointer_v1_interface;

  struct wl_resource *constraint_resource =
      wl_resource_create(client, iface, wl_resource_get_version(resource), id);
  if (!constraint_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct pointer_constraint *constraint = calloc(1, sizeof(*constraint));
  if (!constraint) {
    wl_resource_destroy(constraint_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  constraint->resource = constraint_resource;
  constraint->surface = surface;
  constraint->pointer = pointer;
  constraint->region = region;
  constraint->type = type;
  constraint->lifetime = lifetime;
  wl_list_insert(&constraints, &constraint->link);

  if (type == CONSTRAINT_LOCK) {
    wl_resource_set_implementation(constraint_resource, &locked_pointer_impl,
                                   constraint,
                                   pointer_constraint_resource_destroy);
  } else {
    wl_resource_set_implementation(constraint_resource, &confined_pointer_impl,
                                   constraint,
                                   pointer_constraint_resource_destroy);
  }

  if (bridge && bridge->pointer_focus == surface) {
    send_constraint_state(constraint, true);
    if (constraint->active)
      schedule_raw_input_sync();
  } else {
    bridge_log("constraint %s: surface=%p not focused (deferred activation)",
               type == CONSTRAINT_LOCK ? "LOCK" : "CONFINE", (void *)surface);
  }
}

static void pointer_constraints_lock_pointer(
    struct wl_client *client, struct wl_resource *resource, uint32_t id,
    struct wl_resource *surface, struct wl_resource *pointer,
    struct wl_resource *region, uint32_t lifetime) {
  create_constraint(client, resource, id, surface, pointer, region, lifetime,
                    CONSTRAINT_LOCK);
}

static void pointer_constraints_confine_pointer(
    struct wl_client *client, struct wl_resource *resource, uint32_t id,
    struct wl_resource *surface, struct wl_resource *pointer,
    struct wl_resource *region, uint32_t lifetime) {
  create_constraint(client, resource, id, surface, pointer, region, lifetime,
                    CONSTRAINT_CONFINE);
}

static const struct zwp_pointer_constraints_v1_interface
    pointer_constraints_impl = {
        .destroy = pointer_constraints_destroy,
        .lock_pointer = pointer_constraints_lock_pointer,
        .confine_pointer = pointer_constraints_confine_pointer,
};

void bridge_pointer_constraints_set_focus(struct bridge_surface *surface,
                                          bool focused) {
  if (!constraints_initialized || !surface) {
    return;
  }

  bool state_changed = false;
  struct pointer_constraint *c, *tmp;
  wl_list_for_each_safe(c, tmp, &constraints, link) {
    if (c->surface != surface) {
      continue;
    }

    if (!focused &&
        c->lifetime == ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT) {
      continue;
    }

    if (c->active != focused) {
      state_changed = true;
    }

    send_constraint_state(c, focused);

    if (!focused &&
        c->lifetime == ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT) {
      wl_resource_destroy(c->resource);
    }
  }
  if (state_changed) {
    LOG_DEBUG("constraints_set_focus: surface=%p focused=%d", (void *)surface,
              focused);
    schedule_raw_input_sync();
  }
}

bool bridge_pointer_constraints_has_active(struct bridge_surface *surface) {
  if (!constraints_initialized || !surface) {
    return false;
  }

  struct pointer_constraint *c;
  wl_list_for_each(c, &constraints, link) {
    if (c->surface == surface && c->active) {
      return true;
    }
  }

  return false;
}

bool bridge_pointer_constraints_is_locked(struct bridge_surface *surface) {
  if (!constraints_initialized || !surface) {
    return false;
  }

  struct pointer_constraint *c;
  wl_list_for_each(c, &constraints, link) {
    if (c->surface == surface && c->active && c->type == CONSTRAINT_LOCK) {
      return true;
    }
  }

  return false;
}

bool bridge_pointer_constraints_apply_motion(struct bridge_surface *surface,
                                             int32_t *x, int32_t *y) {
  if (!constraints_initialized || !surface || !x || !y) {
    return true;
  }

  bool constrained = false;
  struct pointer_constraint *c;
  wl_list_for_each(c, &constraints, link) {
    if (c->surface != surface || !c->active) {
      continue;
    }

    constrained = true;
    if (c->type == CONSTRAINT_LOCK) {
      if (surface->has_pointer_position) {
        *x = surface->pointer_x;
        *y = surface->pointer_y;
      }
      continue;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    if (surface->plexy_window) {
      plexy_window_get_size(surface->plexy_window, &width, &height);
    }

    if (width > 0) {
      if (*x < 0)
        *x = 0;
      if ((uint32_t)*x >= width)
        *x = (int32_t)(width - 1);
    }
    if (height > 0) {
      if (*y < 0)
        *y = 0;
      if ((uint32_t)*y >= height)
        *y = (int32_t)(height - 1);
    }

    if (c->region && !point_in_region_resource(c->region, *x, *y)) {
      if (surface->has_pointer_position &&
          point_in_region_resource(c->region, surface->pointer_x,
                                   surface->pointer_y)) {
        *x = surface->pointer_x;
        *y = surface->pointer_y;
      }
    }
  }

  (void)constrained;
  return true;
}

void bind_pointer_constraints(struct wl_client *client, void *data,
                              uint32_t version, uint32_t id) {
  (void)data;
  ensure_constraints_init();

  struct wl_resource *resource = wl_resource_create(
      client, &zwp_pointer_constraints_v1_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &pointer_constraints_impl, NULL,
                                 NULL);
}
