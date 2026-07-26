/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

struct bridge_surface;

void input_router_pointer_enter(struct bridge_surface *surface, int32_t x,
                                int32_t y);
void input_router_pointer_leave(struct bridge_surface *surface);
void input_router_pointer_motion(struct bridge_surface *surface, int32_t x,
                                 int32_t y);
void input_router_pointer_button(struct bridge_surface *surface,
                                 uint32_t button, bool pressed, int32_t x,
                                 int32_t y);
void input_router_pointer_axis(struct bridge_surface *surface, int32_t axis,
                               int32_t value, int32_t discrete);

void input_router_clear_focus(struct bridge_surface *surface);
