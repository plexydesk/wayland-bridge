/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "wayland_bridge.h"
#include "xwm.h"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <drm_fourcc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <wayland-server-protocol.h>

struct bridge_region_resource {
  struct bridge_region_state state;
};

static void region_state_clear(struct bridge_region_state *state) {
  if (!state)
    return;
  free(state->ops);
  state->ops = NULL;
  state->count = 0;
  state->capacity = 0;
}

static bool region_state_ensure_capacity(struct bridge_region_state *state,
                                         size_t needed) {
  if (state->capacity >= needed) {
    return true;
  }
  size_t new_capacity = state->capacity ? state->capacity : 8;
  while (new_capacity < needed) {
    new_capacity *= 2;
  }
  struct bridge_region_op *ops =
      realloc(state->ops, new_capacity * sizeof(*ops));
  if (!ops) {
    return false;
  }
  state->ops = ops;
  state->capacity = new_capacity;
  return true;
}

static bool region_state_append(struct bridge_region_state *state, bool add,
                                int32_t x, int32_t y, int32_t width,
                                int32_t height) {
  if (!state || width <= 0 || height <= 0) {
    return true;
  }
  if (!region_state_ensure_capacity(state, state->count + 1)) {
    return false;
  }
  struct bridge_region_op *op = &state->ops[state->count++];
  op->x = x;
  op->y = y;
  op->width = width;
  op->height = height;
  op->add = add;
  return true;
}

static bool region_state_copy(struct bridge_region_state *dst,
                              const struct bridge_region_state *src) {
  if (!dst || !src)
    return false;
  if (src->count == 0) {
    region_state_clear(dst);
    return true;
  }
  struct bridge_region_op *ops = malloc(src->count * sizeof(*ops));
  if (!ops) {
    return false;
  }
  memcpy(ops, src->ops, src->count * sizeof(*ops));
  free(dst->ops);
  dst->ops = ops;
  dst->count = src->count;
  dst->capacity = src->count;
  return true;
}

static bool point_in_rect(const struct bridge_region_op *op, int32_t x,
                          int32_t y) {
  if (!op || op->width <= 0 || op->height <= 0)
    return false;
  int64_t right = (int64_t)op->x + (int64_t)op->width;
  int64_t bottom = (int64_t)op->y + (int64_t)op->height;
  return x >= op->x && y >= op->y && (int64_t)x < right && (int64_t)y < bottom;
}

static bool surface_opaque_region_contains(const struct bridge_surface *surface,
                                           int32_t x, int32_t y) {
  if (!surface || !surface->opaque_region_set) {
    return false;
  }

  bool inside = false;
  for (size_t i = 0; i < surface->opaque_region.count; i++) {
    const struct bridge_region_op *op = &surface->opaque_region.ops[i];
    if (point_in_rect(op, x, y)) {
      inside = op->add;
    }
  }
  return inside;
}

static uint32_t buffer_extent_to_surface_extent(double extent,
                                                uint32_t buffer_scale) {
  if (extent <= 0.0) {
    return 0;
  }
  if (buffer_scale <= 1) {
    return (uint32_t)(extent + 0.5);
  }

  uint32_t logical = (uint32_t)(extent / (double)buffer_scale + 0.5);
  return logical > 0 ? logical : 1;
}

static bool surface_opaque_region_contains_buffer_point(
    const struct bridge_surface *surface, uint32_t buffer_x,
    uint32_t buffer_y) {
  int32_t surface_x = (int32_t)buffer_x;
  int32_t surface_y = (int32_t)buffer_y;

  if (bridge_surface_uses_window_geometry_crop(surface) &&
      !surface->viewport_dst_set) {
    surface_x += surface->window_geometry.x;
    surface_y += surface->window_geometry.y;
  } else if (surface && !surface->is_x11 &&
             !bridge_surface_uses_logical_content_buffer(surface) &&
             !surface->viewport_dst_set && surface->buffer_scale > 1) {
    int32_t scale = (int32_t)surface->buffer_scale;
    surface_x /= scale;
    surface_y /= scale;
  }

  return surface_opaque_region_contains(surface, surface_x, surface_y);
}

bool bridge_surface_input_region_contains(const struct bridge_surface *surface,
                                          int32_t x, int32_t y) {
  if (!surface) {
    return false;
  }

  if (surface->xdg_toplevel && !surface->xdg_popup && !surface->is_subsurface) {
    return true;
  }
  if (!surface->input_region_set) {
    return true;
  }
  bool inside = false;
  for (size_t i = 0; i < surface->input_region.count; i++) {
    const struct bridge_region_op *op = &surface->input_region.ops[i];
    if (point_in_rect(op, x, y)) {
      inside = op->add;
    }
  }
  return inside;
}

void bridge_sync_input_region_to_plexy(struct bridge_surface *surface) {
  if (!surface || !surface->plexy_window) {
    return;
  }

  if (surface->xdg_toplevel && !surface->xdg_popup && !surface->is_subsurface) {
    plexy_window_set_input_region_mode(surface->plexy_window, false);
    return;
  }

  if (!surface->input_region_set) {
    plexy_window_set_input_region_mode(surface->plexy_window, false);
    return;
  }

  plexy_window_set_input_region_mode(surface->plexy_window, true);
  const float scale = bridge_surface_scale_factor(surface);
  for (size_t i = 0; i < surface->input_region.count; i++) {
    const struct bridge_region_op *op = &surface->input_region.ops[i];
    int32_t phys_x = (int32_t)((float)op->x * scale);
    int32_t phys_y = (int32_t)((float)op->y * scale);
    uint32_t phys_w = (uint32_t)((float)op->width * scale + 0.5f);
    uint32_t phys_h = (uint32_t)((float)op->height * scale + 0.5f);
    plexy_window_input_region_op(surface->plexy_window, phys_x, phys_y, phys_w,
                                 phys_h, op->add);
  }
}

static uint32_t wl_format_to_argb(uint32_t wl_format, uint32_t pixel) {
  switch (wl_format) {
  case WL_SHM_FORMAT_ARGB8888:
    return pixel;
  case WL_SHM_FORMAT_XRGB8888:
    return pixel | 0xFF000000u;
  case WL_SHM_FORMAT_ABGR8888: {
    uint8_t a = (pixel >> 24) & 0xFF;
    uint8_t b = (pixel >> 16) & 0xFF;
    uint8_t g = (pixel >> 8) & 0xFF;
    uint8_t r = pixel & 0xFF;
    return (a << 24) | (r << 16) | (g << 8) | b;
  }
  case WL_SHM_FORMAT_XBGR8888: {
    uint8_t b = (pixel >> 16) & 0xFF;
    uint8_t g = (pixel >> 8) & 0xFF;
    uint8_t r = pixel & 0xFF;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
  }
  case WL_SHM_FORMAT_RGBA8888: {
    uint8_t r = (pixel >> 24) & 0xFF;
    uint8_t g = (pixel >> 16) & 0xFF;
    uint8_t b = (pixel >> 8) & 0xFF;
    uint8_t a = pixel & 0xFF;
    return (a << 24) | (r << 16) | (g << 8) | b;
  }
  case WL_SHM_FORMAT_RGBX8888: {
    uint8_t r = (pixel >> 24) & 0xFF;
    uint8_t g = (pixel >> 16) & 0xFF;
    uint8_t b = (pixel >> 8) & 0xFF;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
  }
  case WL_SHM_FORMAT_BGRA8888: {
    uint8_t b = (pixel >> 24) & 0xFF;
    uint8_t g = (pixel >> 16) & 0xFF;
    uint8_t r = (pixel >> 8) & 0xFF;
    uint8_t a = pixel & 0xFF;
    return (a << 24) | (r << 16) | (g << 8) | b;
  }
  case WL_SHM_FORMAT_BGRX8888: {
    uint8_t b = (pixel >> 24) & 0xFF;
    uint8_t g = (pixel >> 16) & 0xFF;
    uint8_t r = (pixel >> 8) & 0xFF;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
  }
  case WL_SHM_FORMAT_RGB565: {
    uint16_t v = (uint16_t)pixel;
    uint8_t r = ((v >> 11) & 0x1F) * 255 / 31;
    uint8_t g = ((v >> 5) & 0x3F) * 255 / 63;
    uint8_t b = (v & 0x1F) * 255 / 31;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
  }
  case WL_SHM_FORMAT_BGR565: {
    uint16_t v = (uint16_t)pixel;
    uint8_t b = ((v >> 11) & 0x1F) * 255 / 31;
    uint8_t g = ((v >> 5) & 0x3F) * 255 / 63;
    uint8_t r = (v & 0x1F) * 255 / 31;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
  }
  default:
    return 0;
  }
}

static bool compute_viewport_mapping(const struct bridge_surface *surface,
                                     uint32_t src_width, uint32_t src_height,
                                     double *out_src_x, double *out_src_y,
                                     double *out_src_w, double *out_src_h,
                                     uint32_t *out_dst_w, uint32_t *out_dst_h) {
  if (!out_src_x || !out_src_y || !out_src_w || !out_src_h || !out_dst_w ||
      !out_dst_h) {
    return false;
  }

  double src_x = 0.0;
  double src_y = 0.0;
  double src_w = (double)src_width;
  double src_h = (double)src_height;

  if (surface && surface->viewport_src_set) {
    src_x = wl_fixed_to_double(surface->viewport_src_x);
    src_y = wl_fixed_to_double(surface->viewport_src_y);
    src_w = wl_fixed_to_double(surface->viewport_src_width);
    src_h = wl_fixed_to_double(surface->viewport_src_height);
  }

  if (src_x < 0.0) {
    src_w += src_x;
    src_x = 0.0;
  }
  if (src_y < 0.0) {
    src_h += src_y;
    src_y = 0.0;
  }

  if (src_x >= (double)src_width || src_y >= (double)src_height) {
    return false;
  }

  if (src_x + src_w > (double)src_width) {
    src_w = (double)src_width - src_x;
  }
  if (src_y + src_h > (double)src_height) {
    src_h = (double)src_height - src_y;
  }
  if (src_w <= 0.0 || src_h <= 0.0) {
    return false;
  }

  uint32_t dst_w = 0;
  uint32_t dst_h = 0;
  if (bridge_surface_uses_window_geometry_crop(surface) &&
      !surface->viewport_src_set && !surface->viewport_dst_set) {
    uint32_t scale = surface->buffer_scale > 0 ? surface->buffer_scale : 1;
    double geom_x = (double)surface->window_geometry.x * (double)scale;
    double geom_y = (double)surface->window_geometry.y * (double)scale;
    double geom_w = (double)surface->window_geometry.width * (double)scale;
    double geom_h = (double)surface->window_geometry.height * (double)scale;

    if (geom_x < 0.0) {
      geom_w += geom_x;
      geom_x = 0.0;
    }
    if (geom_y < 0.0) {
      geom_h += geom_y;
      geom_y = 0.0;
    }
    if (geom_x >= (double)src_width || geom_y >= (double)src_height ||
        geom_w <= 0.0 || geom_h <= 0.0) {
      return false;
    }
    if (geom_x + geom_w > (double)src_width) {
      geom_w = (double)src_width - geom_x;
    }
    if (geom_y + geom_h > (double)src_height) {
      geom_h = (double)src_height - geom_y;
    }
    if (geom_w <= 0.0 || geom_h <= 0.0) {
      return false;
    }

    src_x = geom_x;
    src_y = geom_y;
    src_w = geom_w;
    src_h = geom_h;
    dst_w = buffer_extent_to_surface_extent(src_w, scale);
    dst_h = buffer_extent_to_surface_extent(src_h, scale);
  } else if (surface && surface->viewport_dst_set) {
    dst_w = (uint32_t)surface->viewport_dst_width;
    dst_h = (uint32_t)surface->viewport_dst_height;
  } else if (bridge_surface_uses_logical_content_buffer(surface)) {
    uint32_t scale = surface->buffer_scale > 0 ? surface->buffer_scale : 1;
    dst_w = buffer_extent_to_surface_extent(src_w, scale);
    dst_h = buffer_extent_to_surface_extent(src_h, scale);
  } else {
    dst_w = (uint32_t)(src_w + 0.5);
    dst_h = (uint32_t)(src_h + 0.5);
  }
  if (dst_w == 0 || dst_h == 0) {
    return false;
  }

  *out_src_x = src_x;
  *out_src_y = src_y;
  *out_src_w = src_w;
  *out_src_h = src_h;
  *out_dst_w = dst_w;
  *out_dst_h = dst_h;
  return true;
}

static bool copy_shm_to_plexy(const struct bridge_shm_buffer *shm_buffer,
                              PlexyBuffer *pbuf,
                              const struct bridge_surface *surface,
                              float scale) {
  uint32_t src_width = bridge_shm_buffer_get_width(shm_buffer);
  uint32_t src_height = bridge_shm_buffer_get_height(shm_buffer);
  uint32_t stride = bridge_shm_buffer_get_stride(shm_buffer);
  uint32_t format = bridge_shm_buffer_get_format(shm_buffer);
  const uint8_t *src_base = bridge_shm_buffer_get_data(shm_buffer);

  if (!src_base || src_width == 0 || src_height == 0) {
    LOG_ERROR("copy_shm_to_plexy: FAILED - src_base=%p w=%u h=%u", src_base,
              src_width, src_height);
    return false;
  }

  uint32_t dst_width = plexy_buffer_get_width(pbuf);
  uint32_t dst_height = plexy_buffer_get_height(pbuf);
  uint32_t plexy_stride = plexy_buffer_get_stride(pbuf);
  uint8_t *dst_base = plexy_buffer_get_data(pbuf);

  if (!dst_base || plexy_stride == 0) {
    LOG_ERROR("copy_shm_to_plexy: FAILED - dst_base=%p plexy_stride=%u",
              dst_base, plexy_stride);
    return false;
  }

  if (!dst_base || plexy_stride == 0) {
    return false;
  }

  double src_x = 0.0, src_y = 0.0, src_w = (double)src_width,
         src_h = (double)src_height;
  uint32_t mapped_dst_w = dst_width, mapped_dst_h = dst_height;
  if (!compute_viewport_mapping(surface, src_width, src_height, &src_x, &src_y,
                                &src_w, &src_h, &mapped_dst_w, &mapped_dst_h)) {
    return false;
  }

  if (mapped_dst_w != dst_width || mapped_dst_h != dst_height) {
    return false;
  }

  bool has_viewport_transform =
      !(src_x == 0.0 && src_y == 0.0 && src_w == (double)src_width &&
        src_h == (double)src_height);

  if (!surface->opaque_region_set && !has_viewport_transform && scale == 1.0f &&
      src_width == dst_width && src_height == dst_height) {

    if (format == WL_SHM_FORMAT_ARGB8888 && stride == plexy_stride) {
      memcpy(dst_base, src_base, src_height * stride);
      return true;
    }

    if (format == WL_SHM_FORMAT_ARGB8888) {
      for (uint32_t y = 0; y < src_height; y++) {
        memcpy(dst_base + y * plexy_stride, src_base + y * stride,
               src_width * 4);
      }
      return true;
    }

    if (format == WL_SHM_FORMAT_XRGB8888) {
      for (uint32_t y = 0; y < src_height; y++) {
        const uint32_t *src_row = (const uint32_t *)(src_base + y * stride);
        uint32_t *dst_row = (uint32_t *)(dst_base + y * plexy_stride);
        for (uint32_t x = 0; x < src_width; x++) {
          dst_row[x] = src_row[x] | 0xFF000000u;
        }
      }
      return true;
    }
  }

  for (uint32_t dst_y = 0; dst_y < dst_height; dst_y++) {
    double fy = ((double)dst_y + 0.5) / (double)dst_height;
    uint32_t sample_y = (uint32_t)(src_y + fy * src_h);
    if (sample_y >= src_height)
      sample_y = src_height - 1;

    const uint8_t *src_row = src_base + sample_y * stride;
    uint32_t *dst_row = (uint32_t *)(dst_base + dst_y * plexy_stride);

    for (uint32_t dst_x = 0; dst_x < dst_width; dst_x++) {
      double fx = ((double)dst_x + 0.5) / (double)dst_width;
      uint32_t sample_x = (uint32_t)(src_x + fx * src_w);
      if (sample_x >= src_width)
        sample_x = src_width - 1;

      const uint32_t *src_pixel = (const uint32_t *)(src_row + sample_x * 4);
      uint32_t pixel = wl_format_to_argb(format, *src_pixel);
      if (surface->is_x11 &&
          surface_opaque_region_contains_buffer_point(surface, dst_x, dst_y)) {
        pixel |= 0xFF000000u;
      }
      dst_row[dst_x] = pixel;
    }
  }

  return true;
}

static bool dmabuf_format_has_alpha(uint32_t format) {
  switch (format) {
  case DRM_FORMAT_ARGB8888:
  case DRM_FORMAT_ABGR8888:
  case DRM_FORMAT_RGBA8888:
  case DRM_FORMAT_BGRA8888:
    return true;
  default:
    return false;
  }
}

static bool
copy_rgba_to_plexy_with_viewport(const uint8_t *rgba, uint32_t src_width,
                                 uint32_t src_height, PlexyBuffer *dst_buf,
                                 const struct bridge_surface *surface,
                                 bool src_has_alpha) {
  if (!rgba || !dst_buf || src_width == 0 || src_height == 0) {
    return false;
  }

  uint32_t dst_width = plexy_buffer_get_width(dst_buf);
  uint32_t dst_height = plexy_buffer_get_height(dst_buf);
  uint32_t dst_stride = plexy_buffer_get_stride(dst_buf);
  uint8_t *dst_base = plexy_buffer_get_data(dst_buf);
  if (!dst_base || dst_stride == 0 || dst_width == 0 || dst_height == 0) {
    return false;
  }

  double src_x = 0.0, src_y = 0.0, src_w = (double)src_width,
         src_h = (double)src_height;
  uint32_t mapped_dst_w = dst_width, mapped_dst_h = dst_height;
  if (!compute_viewport_mapping(surface, src_width, src_height, &src_x, &src_y,
                                &src_w, &src_h, &mapped_dst_w, &mapped_dst_h)) {
    return false;
  }
  if (mapped_dst_w != dst_width || mapped_dst_h != dst_height) {
    return false;
  }

  const bool bilinear_resample =
      (src_w != (double)dst_width || src_h != (double)dst_height);

  for (uint32_t y = 0; y < dst_height; y++) {
    uint32_t *dst_row = (uint32_t *)(dst_base + y * dst_stride);

    for (uint32_t x = 0; x < dst_width; x++) {
      uint8_t r = 0, g = 0, b = 0, a = 0;
      if (bilinear_resample) {
        double src_fx =
            src_x + (((double)x + 0.5) * src_w / (double)dst_width) - 0.5;
        double src_fy =
            src_y + (((double)y + 0.5) * src_h / (double)dst_height) - 0.5;

        if (src_fx < 0.0)
          src_fx = 0.0;
        if (src_fy < 0.0)
          src_fy = 0.0;
        if (src_fx > (double)(src_width - 1))
          src_fx = (double)(src_width - 1);
        if (src_fy > (double)(src_height - 1))
          src_fy = (double)(src_height - 1);

        uint32_t x0 = (uint32_t)src_fx;
        uint32_t y0 = (uint32_t)src_fy;
        uint32_t x1 = x0 + 1 < src_width ? x0 + 1 : x0;
        uint32_t y1 = y0 + 1 < src_height ? y0 + 1 : y0;
        uint32_t wx = (uint32_t)((src_fx - (double)x0) * 256.0 + 0.5);
        uint32_t wy = (uint32_t)((src_fy - (double)y0) * 256.0 + 0.5);
        if (wx > 256)
          wx = 256;
        if (wy > 256)
          wy = 256;

        const uint8_t *p00 = rgba + (((size_t)y0 * src_width + x0) * 4u);
        const uint8_t *p10 = rgba + (((size_t)y0 * src_width + x1) * 4u);
        const uint8_t *p01 = rgba + (((size_t)y1 * src_width + x0) * 4u);
        const uint8_t *p11 = rgba + (((size_t)y1 * src_width + x1) * 4u);

        uint32_t r0 =
            ((uint32_t)p00[0] * (256 - wx) + (uint32_t)p10[0] * wx + 128) >> 8;
        uint32_t g0 =
            ((uint32_t)p00[1] * (256 - wx) + (uint32_t)p10[1] * wx + 128) >> 8;
        uint32_t b0 =
            ((uint32_t)p00[2] * (256 - wx) + (uint32_t)p10[2] * wx + 128) >> 8;
        uint32_t a0 =
            ((uint32_t)p00[3] * (256 - wx) + (uint32_t)p10[3] * wx + 128) >> 8;

        uint32_t r1 =
            ((uint32_t)p01[0] * (256 - wx) + (uint32_t)p11[0] * wx + 128) >> 8;
        uint32_t g1 =
            ((uint32_t)p01[1] * (256 - wx) + (uint32_t)p11[1] * wx + 128) >> 8;
        uint32_t b1 =
            ((uint32_t)p01[2] * (256 - wx) + (uint32_t)p11[2] * wx + 128) >> 8;
        uint32_t a1 =
            ((uint32_t)p01[3] * (256 - wx) + (uint32_t)p11[3] * wx + 128) >> 8;

        r = (uint8_t)((r0 * (256 - wy) + r1 * wy + 128) >> 8);
        g = (uint8_t)((g0 * (256 - wy) + g1 * wy + 128) >> 8);
        b = (uint8_t)((b0 * (256 - wy) + b1 * wy + 128) >> 8);
        a = (uint8_t)((a0 * (256 - wy) + a1 * wy + 128) >> 8);
      } else {
        double src_fx = src_x + (((double)x + 0.5) * src_w / (double)dst_width);
        double src_fy =
            src_y + (((double)y + 0.5) * src_h / (double)dst_height);
        uint32_t sx = (uint32_t)src_fx;
        uint32_t sy = (uint32_t)src_fy;
        if (sx >= src_width)
          sx = src_width - 1;
        if (sy >= src_height)
          sy = src_height - 1;
        const uint8_t *src_px = rgba + (((size_t)sy * src_width + sx) * 4u);
        r = src_px[0];
        g = src_px[1];
        b = src_px[2];
        a = src_px[3];
      }

      if (surface->is_x11 &&
          surface_opaque_region_contains_buffer_point(surface, x, y)) {
        a = 0xFF;
      } else if (!src_has_alpha) {
        a = 0xFF;
      } else if (a > 0 && a < 255) {

        r = (uint8_t)(((uint32_t)r * 255u + (a / 2u)) / (uint32_t)a);
        g = (uint8_t)(((uint32_t)g * 255u + (a / 2u)) / (uint32_t)a);
        b = (uint8_t)(((uint32_t)b * 255u + (a / 2u)) / (uint32_t)a);
      } else if (a == 0) {
        r = g = b = 0;
      }
      dst_row[x] =
          ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
  }

  return true;
}

static void handle_previous_buffer_destroy(struct wl_listener *listener,
                                           void *data) {
  struct bridge_surface *surface;
  surface =
      wl_container_of(listener, surface, previous_buffer_destroy_listener);

  LOG_DEBUG("Previous buffer destroyed before release (surface=%p)",
            (void *)surface);
  surface->previous_buffer = NULL;
  wl_list_init(&surface->previous_buffer_destroy_listener.link);
}

static void handle_current_buffer_destroy(struct wl_listener *listener,
                                          void *data) {
  struct bridge_surface *surface;
  surface = wl_container_of(listener, surface, current_buffer_destroy_listener);

  LOG_DEBUG("Current buffer destroyed (surface=%p)", (void *)surface);
  surface->current_buffer = NULL;
  wl_list_init(&surface->current_buffer_destroy_listener.link);
}

static void handle_pending_buffer_destroy(struct wl_listener *listener,
                                          void *data) {
  struct bridge_surface *surface;
  surface = wl_container_of(listener, surface, pending_buffer_destroy_listener);

  LOG_DEBUG("Pending buffer destroyed (surface=%p)", (void *)surface);
  surface->pending_buffer = NULL;
  wl_list_init(&surface->pending_buffer_destroy_listener.link);
}

static void surface_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void surface_attach(struct wl_client *client,
                           struct wl_resource *resource,
                           struct wl_resource *buffer, int32_t x, int32_t y) {
  struct bridge_surface *surface = bridge_surface_from_resource(resource);
  if (!surface) {
    LOG_ERROR("surface_attach called with NULL surface");
    return;
  }

  bool is_dmabuf = bridge_is_dmabuf_buffer(buffer);

  if (surface->pending_buffer) {
    wl_list_remove(&surface->pending_buffer_destroy_listener.link);
    wl_list_init(&surface->pending_buffer_destroy_listener.link);
  }

  surface->pending_buffer = buffer;

  if (surface->pending_buffer) {
    surface->pending_buffer_destroy_listener.notify =
        handle_pending_buffer_destroy;
    wl_resource_add_destroy_listener(surface->pending_buffer,
                                     &surface->pending_buffer_destroy_listener);
  }

  surface->pending_dx = x;
  surface->pending_dy = y;
}

static void surface_damage(struct wl_client *client,
                           struct wl_resource *resource, int32_t x, int32_t y,
                           int32_t width, int32_t height) {
  struct bridge_surface *surface = bridge_surface_from_resource(resource);
  if (!surface)
    return;

  surface->has_damage = true;
  surface->damage.x = x;
  surface->damage.y = y;
  surface->damage.width = width;
  surface->damage.height = height;
}

static void frame_callback_destroy(struct wl_resource *resource) {
  wl_list_remove(wl_resource_get_link(resource));
}

static void surface_frame(struct wl_client *client,
                          struct wl_resource *resource, uint32_t callback) {
  struct bridge_surface *surface = bridge_surface_from_resource(resource);
  if (!surface)
    return;

  struct wl_resource *callback_resource =
      wl_resource_create(client, &wl_callback_interface, 1, callback);

  if (callback_resource) {
    wl_resource_set_implementation(callback_resource, NULL, NULL,
                                   frame_callback_destroy);
    wl_list_insert(&surface->frame_callbacks,
                   wl_resource_get_link(callback_resource));
  }
}

static void surface_set_opaque_region(struct wl_client *client,
                                      struct wl_resource *resource,
                                      struct wl_resource *region) {
  struct bridge_surface *surface = bridge_surface_from_resource(resource);
  if (!surface)
    return;

  if (!region) {
    surface->opaque_region_set = false;
    region_state_clear(&surface->opaque_region);
    return;
  }

  struct bridge_region_resource *region_res = wl_resource_get_user_data(region);
  if (!region_res) {
    surface->opaque_region_set = false;
    region_state_clear(&surface->opaque_region);
    return;
  }

  if (!region_state_copy(&surface->opaque_region, &region_res->state)) {
    wl_resource_post_no_memory(resource);
    return;
  }
  surface->opaque_region_set = true;
}

static void surface_set_input_region(struct wl_client *client,
                                     struct wl_resource *resource,
                                     struct wl_resource *region) {
  struct bridge_surface *surface = bridge_surface_from_resource(resource);
  if (!surface)
    return;

  if (!region) {
    surface->input_region_set = false;
    region_state_clear(&surface->input_region);
    bridge_sync_input_region_to_plexy(surface);
    return;
  }

  struct bridge_region_resource *region_res = wl_resource_get_user_data(region);
  if (!region_res) {
    surface->input_region_set = false;
    region_state_clear(&surface->input_region);
    bridge_sync_input_region_to_plexy(surface);
    return;
  }

  if (!region_state_copy(&surface->input_region, &region_res->state)) {
    wl_resource_post_no_memory(resource);
    return;
  }
  surface->input_region_set = true;
  bridge_sync_input_region_to_plexy(surface);
}

static void bridge_subsurface_logical_to_physical(
    struct bridge_surface *surface, int32_t logical_x, int32_t logical_y,
    uint32_t logical_w, uint32_t logical_h, int32_t *phys_x, int32_t *phys_y,
    uint32_t *phys_w, uint32_t *phys_h) {
  const float scale = surface ? bridge_surface_scale_factor(surface) : 1.0f;
  if (phys_x)
    *phys_x = (int32_t)((float)logical_x * scale + 0.5f);
  if (phys_y)
    *phys_y = (int32_t)((float)logical_y * scale + 0.5f);
  if (phys_w)
    *phys_w = (uint32_t)((float)logical_w * scale + 0.5f);
  if (phys_h)
    *phys_h = (uint32_t)((float)logical_h * scale + 0.5f);
}

static void bridge_update_subsurface_geometry(struct bridge_surface *surface,
                                              uint32_t target_width,
                                              uint32_t target_height) {
  if (!surface || !surface->is_subsurface || !surface->plexy_window)
    return;

  int32_t phys_x = 0, phys_y = 0;
  uint32_t phys_w = 0, phys_h = 0;
  bridge_subsurface_logical_to_physical(
      surface, surface->subsurface_x, surface->subsurface_y, target_width,
      target_height, &phys_x, &phys_y, &phys_w, &phys_h);

  const int32_t dw = (int32_t)phys_w - (int32_t)surface->plexy_window_width;
  const int32_t dh = (int32_t)phys_h - (int32_t)surface->plexy_window_height;
  if (abs(dw) <= 1 && abs(dh) <= 1)
    return;

  int ret = plexy_popup_update_geometry(surface->plexy_window, phys_x, phys_y,
                                        phys_w, phys_h, PLEXY_POPUP_FLAG_NONE);
  if (ret < 0) {
    LOG_WARN("Failed to update subsurface geometry win=%u to %ux%u",
             surface->plexy_window_id, phys_w, phys_h);
  } else {
    LOG_INFO("Updated subsurface geometry win=%u pos=(%d,%d) size=%ux%u "
             "(logical=%ux%u scale=%.2f)",
             surface->plexy_window_id, phys_x, phys_y, phys_w, phys_h,
             target_width, target_height, bridge_surface_scale_factor(surface));
    surface->plexy_window_width = phys_w;
    surface->plexy_window_height = phys_h;
  }
}

static void bridge_sync_toplevel_size_to_plexy(struct bridge_surface *surface,
                                               uint32_t content_w,
                                               uint32_t content_h) {
  if (!surface || !surface->xdg_toplevel || surface->xdg_popup ||
      surface->is_subsurface || !surface->plexy_window) {
    return;
  }
  if (!surface->geometry_set)
    return;

  const float scale = bridge_surface_scale_factor(surface);
  uint32_t geom_w =
      (uint32_t)((double)surface->window_geometry.width * (double)scale + 0.5);
  uint32_t geom_h =
      (uint32_t)((double)surface->window_geometry.height * (double)scale + 0.5);
  if (geom_w == 0 || geom_h == 0 || content_w == 0 || content_h == 0)
    return;

  int dw = (int)geom_w - (int)surface->last_configured_width;
  int dh = (int)geom_h - (int)surface->last_configured_height;
  if (abs(dw) <= 1 && abs(dh) <= 1)
    return;

  LOG_INFO("Toplevel win=%u: sync Plexy size to xdg geometry %ux%u "
           "(content %ux%u, last configured %ux%u)",
           surface->plexy_window_id, geom_w, geom_h, content_w, content_h,
           surface->last_configured_width, surface->last_configured_height);
  plexy_window_set_size(surface->plexy_window, geom_w, geom_h, content_w,
                        content_h);
}

static void surface_commit(struct wl_client *client,
                           struct wl_resource *resource) {
  struct bridge_surface *surface = bridge_surface_from_resource(resource);
  if (!surface) {
    LOG_ERROR("surface_commit called with NULL surface");
    return;
  }

  wl_signal_emit(&surface->commit_signal, surface);
  if (!bridge->running) {
    return;
  }

  if (!bridge_syncobj_process_commit(surface))
    return;

  if (surface->is_subsurface) {
    struct bridge_surface *parent = surface->subsurface_parent;
    if (!parent || !parent->plexy_window) {
      LOG_DEBUG("Subsurface commit: parent not ready");

      if (!wl_list_empty(&surface->frame_callbacks)) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        uint32_t time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        struct wl_resource *callback, *tmp;
        wl_resource_for_each_safe(callback, tmp, &surface->frame_callbacks) {
          wl_callback_send_done(callback, time);
          wl_resource_destroy(callback);
        }
      }
      return;
    }

    if (!surface->plexy_window && surface->pending_buffer) {
      struct bridge_shm_buffer *shm_buffer =
          bridge_shm_buffer_from_resource(surface->pending_buffer);
      struct bridge_dmabuf_buffer *dmabuf_buffer =
          bridge_dmabuf_buffer_from_resource(surface->pending_buffer);

      uint32_t width = 0, height = 0;
      if (shm_buffer) {
        width = bridge_shm_buffer_get_width(shm_buffer);
        height = bridge_shm_buffer_get_height(shm_buffer);
      } else if (dmabuf_buffer) {
        width = bridge_dmabuf_get_width(dmabuf_buffer);
        height = bridge_dmabuf_get_height(dmabuf_buffer);
      }

      uint32_t popup_w = width, popup_h = height;
      if (width > 0 && height > 0) {
        double popup_src_x = 0.0, popup_src_y = 0.0;
        double popup_src_w = (double)width, popup_src_h = (double)height;
        if (!compute_viewport_mapping(surface, width, height, &popup_src_x,
                                      &popup_src_y, &popup_src_w, &popup_src_h,
                                      &popup_w, &popup_h)) {
          popup_w = 0;
          popup_h = 0;
        }
      }

      if (popup_w > 0 && popup_h > 0) {
        int32_t phys_x = 0, phys_y = 0;
        uint32_t phys_w = 0, phys_h = 0;
        bridge_subsurface_logical_to_physical(
            surface, surface->subsurface_x, surface->subsurface_y, popup_w,
            popup_h, &phys_x, &phys_y, &phys_w, &phys_h);

        PlexyWindow *popup_window =
            plexy_create_popup(bridge->plexy_conn, parent->plexy_window, phys_x,
                               phys_y, phys_w, phys_h, PLEXY_POPUP_FLAG_NONE);
        if (popup_window) {
          bridge_surface_set_plexy_window(surface, popup_window);
          surface->plexy_window_width = phys_w;
          surface->plexy_window_height = phys_h;
          uint32_t parent_id = plexy_window_get_id(parent->plexy_window);
          bridge_sync_input_region_to_plexy(surface);
          LOG_DEBUG("Created subsurface window: id=%u parent=%u pos=(%d,%d) "
                    "size=%ux%u (logical=%ux%u scale=%.2f, buf=%ux%u)",
                    surface->plexy_window_id, parent_id, phys_x, phys_y, phys_w,
                    phys_h, popup_w, popup_h,
                    bridge_surface_scale_factor(surface), width, height);
        } else {
          LOG_ERROR("Failed to create subsurface window");
        }
      }
    }
  }

  if (!surface->plexy_window) {
    if (surface->pending_buffer) {
      LOG_DEBUG("surface_commit: no plexy_window yet but have pending buffer "
                "(surface=%p, is_sub=%d)",
                (void *)surface, surface->is_subsurface);
    }
    LOG_TRACE("surface_commit: no plexy_window yet");
    return;
  }

  bool buffer_updated = false;

  if (surface->pending_buffer) {
    struct bridge_shm_buffer *shm_buffer =
        bridge_shm_buffer_from_resource(surface->pending_buffer);
    struct bridge_dmabuf_buffer *dmabuf_buffer =
        bridge_dmabuf_buffer_from_resource(surface->pending_buffer);

    if (shm_buffer) {
      uint32_t width = bridge_shm_buffer_get_width(shm_buffer);
      uint32_t height = bridge_shm_buffer_get_height(shm_buffer);
      uint32_t format = bridge_shm_buffer_get_format(shm_buffer);
      double src_x = 0.0, src_y = 0.0, src_w = (double)width,
             src_h = (double)height;
      uint32_t target_width = width, target_height = height;
      if (!compute_viewport_mapping(surface, width, height, &src_x, &src_y,
                                    &src_w, &src_h, &target_width,
                                    &target_height)) {
        LOG_ERROR("Invalid viewport mapping for SHM buffer %ux%u", width,
                  height);
        surface->pending_buffer = NULL;
        return;
      }

      bridge_update_subsurface_geometry(surface, target_width, target_height);
      bridge_sync_toplevel_size_to_plexy(surface, target_width, target_height);

      bool need_new_buffers = false;

      if (!surface->front_plexy_buffer || !surface->back_plexy_buffer) {
        need_new_buffers = true;
      } else {
        uint32_t current_width =
            plexy_buffer_get_width(surface->back_plexy_buffer);
        uint32_t current_height =
            plexy_buffer_get_height(surface->back_plexy_buffer);
        if (current_width != target_width || current_height != target_height) {
          need_new_buffers = true;
        }
      }

      if (need_new_buffers) {
        LOG_DEBUG("Creating double buffers: %ux%u (src=%ux%u)", target_width,
                  target_height, width, height);

        if (surface->front_plexy_buffer) {
          plexy_destroy_buffer(surface->front_plexy_buffer);
        }
        if (surface->back_plexy_buffer) {
          plexy_destroy_buffer(surface->back_plexy_buffer);
        }

        surface->front_plexy_buffer =
            plexy_create_buffer(bridge->plexy_conn, target_width, target_height,
                                PLEXY_FORMAT_ARGB8888);
        surface->back_plexy_buffer =
            plexy_create_buffer(bridge->plexy_conn, target_width, target_height,
                                PLEXY_FORMAT_ARGB8888);

        if (!surface->front_plexy_buffer || !surface->back_plexy_buffer) {
          LOG_ERROR("Failed to create double buffers");
          surface->pending_buffer = NULL;
          return;
        }

        if (surface->is_x11 && surface->xwm_window) {
          struct xwm_window *xwm_win = (struct xwm_window *)surface->xwm_window;
          xwm_window_configure(xwm_win, xwm_win->x, xwm_win->y, target_width,
                               target_height);
          xwm_send_expose_event(xwm_win);
          LOG_DEBUG("Sent ConfigureNotify + Expose to X11 window: %ux%u",
                    target_width, target_height);
        }
      }

      if (!copy_shm_to_plexy(shm_buffer, surface->back_plexy_buffer, surface,
                             1.0f)) {
        LOG_ERROR("Failed to copy shm buffer to Plexy format (format=0x%x)",
                  format);
        surface->pending_buffer = NULL;
        return;
      }

      const char *role = surface->xdg_toplevel    ? "toplevel"
                         : surface->is_subsurface ? "subsurface"
                                                  : "other";
      LOG_INFO("SHM win=%u (%s): attach %ux%u format=0x%x -> %ux%u "
               "opaque_region=%s count=%zu",
               surface->plexy_window_id, role, width, height, format,
               target_width, target_height,
               surface->opaque_region_set ? "set" : "none",
               surface->opaque_region_set ? surface->opaque_region.count : 0);

      int ret = plexy_window_attach(surface->plexy_window,
                                    surface->back_plexy_buffer);
      if (ret < 0) {
        LOG_ERROR("Failed to attach buffer to window");
      } else {
        buffer_updated = true;
      }

      if (surface->current_buffer != surface->pending_buffer) {

        if (surface->previous_buffer) {
          wl_list_remove(&surface->previous_buffer_destroy_listener.link);
          bridge_syncobj_signal_release(surface);
          wl_buffer_send_release(surface->previous_buffer);
        }

        surface->previous_buffer = surface->current_buffer;

        if (surface->previous_buffer) {
          wl_list_remove(&surface->current_buffer_destroy_listener.link);
          wl_list_init(&surface->current_buffer_destroy_listener.link);

          surface->previous_buffer_destroy_listener.notify =
              handle_previous_buffer_destroy;
          wl_resource_add_destroy_listener(
              surface->previous_buffer,
              &surface->previous_buffer_destroy_listener);
        }

        surface->current_buffer = surface->pending_buffer;

        if (surface->current_buffer) {
          wl_list_remove(&surface->pending_buffer_destroy_listener.link);
          wl_list_init(&surface->pending_buffer_destroy_listener.link);

          surface->current_buffer_destroy_listener.notify =
              handle_current_buffer_destroy;
          wl_resource_add_destroy_listener(
              surface->current_buffer,
              &surface->current_buffer_destroy_listener);
        }
      }
    } else if (dmabuf_buffer) {

      uint32_t width = bridge_dmabuf_get_width(dmabuf_buffer);
      uint32_t height = bridge_dmabuf_get_height(dmabuf_buffer);
      int fd = bridge_dmabuf_get_fd(dmabuf_buffer);
      uint32_t stride = bridge_dmabuf_get_stride(dmabuf_buffer);
      uint32_t format = bridge_dmabuf_get_format(dmabuf_buffer);
      uint64_t modifier = bridge_dmabuf_get_modifier(dmabuf_buffer);
      uint32_t texture = bridge_dmabuf_get_texture(dmabuf_buffer);
      double src_x = 0.0, src_y = 0.0, src_w = (double)width,
             src_h = (double)height;
      uint32_t target_width = width, target_height = height;
      if (!compute_viewport_mapping(surface, width, height, &src_x, &src_y,
                                    &src_w, &src_h, &target_width,
                                    &target_height)) {
        LOG_ERROR("Invalid viewport mapping for DMA-BUF %ux%u", width, height);
        surface->pending_buffer = NULL;
        return;
      }

      bridge_update_subsurface_geometry(surface, target_width, target_height);
      bridge_sync_toplevel_size_to_plexy(surface, target_width, target_height);

      bool has_viewport_transform =
          !(src_x == 0.0 && src_y == 0.0 && src_w == (double)width &&
            src_h == (double)height && target_width == width &&
            target_height == height);

      bool need_new_buffers = false;
      bool use_zero_copy = (fd >= 0);

      if (use_zero_copy) {

        const uint32_t phys_buf_w = width;
        const uint32_t phys_buf_h = height;

        if (surface->front_plexy_buffer && surface->back_plexy_buffer) {
          uint32_t current_width =
              plexy_buffer_get_width(surface->front_plexy_buffer);
          uint32_t current_height =
              plexy_buffer_get_height(surface->front_plexy_buffer);
          if (current_width != phys_buf_w || current_height != phys_buf_h) {
            need_new_buffers = true;
          }
        } else {
          need_new_buffers = true;
        }

        if (need_new_buffers) {
          LOG_DEBUG(
              "Creating DMA-BUF zero-copy double buffers: %ux%u format=0x%x",
              phys_buf_w, phys_buf_h, format);

          if (surface->front_plexy_buffer) {
            plexy_destroy_buffer(surface->front_plexy_buffer);
            surface->front_plexy_buffer = NULL;
          }
          if (surface->back_plexy_buffer) {
            plexy_destroy_buffer(surface->back_plexy_buffer);
            surface->back_plexy_buffer = NULL;
          }

          surface->front_plexy_buffer = plexy_create_buffer_from_dmabuf(
              bridge->plexy_conn, fd, phys_buf_w, phys_buf_h, stride, format,
              modifier);
          surface->back_plexy_buffer = plexy_create_buffer_from_dmabuf(
              bridge->plexy_conn, fd, phys_buf_w, phys_buf_h, stride, format,
              modifier);

          if (!surface->front_plexy_buffer || !surface->back_plexy_buffer) {
            LOG_WARN("Zero-copy DMA-BUF double buffer creation failed, falling "
                     "back to readback");
            if (surface->front_plexy_buffer)
              plexy_destroy_buffer(surface->front_plexy_buffer);
            if (surface->back_plexy_buffer)
              plexy_destroy_buffer(surface->back_plexy_buffer);
            surface->front_plexy_buffer = NULL;
            surface->back_plexy_buffer = NULL;
            use_zero_copy = false;
          }
        } else if (surface->current_buffer != surface->pending_buffer) {

          if (surface->back_plexy_buffer) {
            int update_ret = plexy_buffer_update_dmabuf(
                surface->back_plexy_buffer, fd, stride);
            if (update_ret < 0) {
              LOG_WARN("DMA-BUF back buffer fd update failed, falling back to "
                       "readback");
              use_zero_copy = false;
            }
          }
        }

        if (use_zero_copy && surface->back_plexy_buffer) {

          int ret = plexy_window_attach(surface->plexy_window,
                                        surface->back_plexy_buffer);
          if (ret >= 0) {
            buffer_updated = true;
            LOG_DEBUG("DMA-BUF zero-copy attach success for win=%u",
                      surface->plexy_window_id);
          } else {
            LOG_WARN("DMA-BUF zero-copy attach failed, falling back");
            use_zero_copy = false;
          }
        }
      }

      if (!use_zero_copy) {

        if (!surface->front_plexy_buffer || !surface->back_plexy_buffer) {
          need_new_buffers = true;
        } else {
          uint32_t current_width =
              plexy_buffer_get_width(surface->back_plexy_buffer);
          uint32_t current_height =
              plexy_buffer_get_height(surface->back_plexy_buffer);
          if (current_width != target_width ||
              current_height != target_height) {
            need_new_buffers = true;
          }
        }

        if (need_new_buffers) {
          LOG_DEBUG(
              "Creating double buffers for DMA-BUF readback: %ux%u (src=%ux%u)",
              target_width, target_height, width, height);

          if (surface->front_plexy_buffer) {
            plexy_destroy_buffer(surface->front_plexy_buffer);
          }
          if (surface->back_plexy_buffer) {
            plexy_destroy_buffer(surface->back_plexy_buffer);
          }

          surface->front_plexy_buffer =
              plexy_create_buffer(bridge->plexy_conn, target_width,
                                  target_height, PLEXY_FORMAT_ARGB8888);
          surface->back_plexy_buffer =
              plexy_create_buffer(bridge->plexy_conn, target_width,
                                  target_height, PLEXY_FORMAT_ARGB8888);

          if (!surface->front_plexy_buffer || !surface->back_plexy_buffer) {
            LOG_ERROR("Failed to create double buffers for DMA-BUF");
            surface->pending_buffer = NULL;
            return;
          }
        }

        bool need_copy = (surface->current_buffer != surface->pending_buffer);

        if (width == 1 && height == 1 && has_viewport_transform) {
          LOG_INFO("DMA-BUF win=%u: GPU-only placeholder (1x1 buf, viewport "
                   "%ux%u) — "
                   "skipping readback; run app with MOZ_WEBRENDER=0 for SHM "
                   "rendering",
                   surface->plexy_window_id, target_width, target_height);
        } else if (need_copy && texture > 0) {
          LOG_DEBUG("DMA-BUF win=%u: readback path (texture=%u %dx%d -> %dx%d)",
                    surface->plexy_window_id, texture, width, height,
                    target_width, target_height);
          uint8_t *rgba = malloc((size_t)width * (size_t)height * 4);
          if (!rgba) {
            LOG_ERROR("DMA-BUF: failed to allocate readback buffer");
          } else {
            GLuint fbo;
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, texture, 0);

            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status == GL_FRAMEBUFFER_COMPLETE) {
              glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                           rgba);
              LOG_INFO("DMA-BUF win=%u: readback %ux%u format=0x%x -> %ux%u",
                       surface->plexy_window_id, width, height, format,
                       target_width, target_height);
              if (copy_rgba_to_plexy_with_viewport(
                      rgba, width, height, surface->back_plexy_buffer, surface,
                      dmabuf_format_has_alpha(format))) {
                int ret = plexy_window_attach(surface->plexy_window,
                                              surface->back_plexy_buffer);
                if (ret >= 0) {
                  buffer_updated = true;
                  LOG_DEBUG("DMA-BUF win=%u: readback attach success",
                            surface->plexy_window_id);
                } else {
                  LOG_ERROR("DMA-BUF win=%u: readback attach failed ret=%d",
                            surface->plexy_window_id, ret);
                }
              } else {
                LOG_ERROR("DMA-BUF: failed to copy readback with viewport");
              }
            } else {
              LOG_ERROR("DMA-BUF: FBO incomplete (status=0x%x)", status);
            }

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &fbo);
            free(rgba);
          }
        } else if (need_copy) {
          LOG_DEBUG("DMA-BUF win=%u: readback skipped (texture=%u)",
                    surface->plexy_window_id, texture);
        }
      }

      if (surface->current_buffer != surface->pending_buffer) {

        if (surface->previous_buffer) {
          wl_list_remove(&surface->previous_buffer_destroy_listener.link);
          bridge_syncobj_signal_release(surface);
          wl_buffer_send_release(surface->previous_buffer);
        }

        surface->previous_buffer = surface->current_buffer;

        if (surface->previous_buffer) {

          wl_list_remove(&surface->current_buffer_destroy_listener.link);
          wl_list_init(&surface->current_buffer_destroy_listener.link);

          surface->previous_buffer_destroy_listener.notify =
              handle_previous_buffer_destroy;
          wl_resource_add_destroy_listener(
              surface->previous_buffer,
              &surface->previous_buffer_destroy_listener);
        }

        surface->current_buffer = surface->pending_buffer;

        if (surface->current_buffer) {

          wl_list_remove(&surface->pending_buffer_destroy_listener.link);
          wl_list_init(&surface->pending_buffer_destroy_listener.link);

          surface->current_buffer_destroy_listener.notify =
              handle_current_buffer_destroy;
          wl_resource_add_destroy_listener(
              surface->current_buffer,
              &surface->current_buffer_destroy_listener);
        }
      }
    } else {
      LOG_ERROR("Pending buffer is not a shm or dmabuf buffer instance");
    }

    surface->pending_buffer = NULL;
  }

  if (!surface->current_buffer) {
    surface->has_damage = false;

    if (!wl_list_empty(&surface->frame_callbacks)) {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      uint32_t time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
      struct wl_resource *callback, *tmp;
      wl_resource_for_each_safe(callback, tmp, &surface->frame_callbacks) {
        wl_callback_send_done(callback, time);
        wl_resource_destroy(callback);
      }
    }
    return;
  }

  if (buffer_updated) {
    int commit_ret = 0;
    if (surface->has_damage) {
      commit_ret = plexy_window_commit_damage(
          surface->plexy_window, surface->damage.x, surface->damage.y,
          surface->damage.width, surface->damage.height, -1, -1, 0, 0);
      surface->has_damage = false;
    } else {
      commit_ret = plexy_window_commit(surface->plexy_window);
    }
    LOG_TRACE("Committed win=%u ret=%d", surface->plexy_window_id, commit_ret);

    if (surface->front_plexy_buffer && surface->back_plexy_buffer) {
      PlexyBuffer *temp = surface->front_plexy_buffer;
      surface->front_plexy_buffer = surface->back_plexy_buffer;
      surface->back_plexy_buffer = temp;
    }

    plexy_flush(bridge->plexy_conn);

    if (!surface->is_subsurface && !wl_list_empty(&surface->subsurfaces)) {
      bridge_subsurface_enforce_zorder(surface);
    }

  } else {

    surface->has_damage = false;

    if (!wl_list_empty(&surface->frame_callbacks)) {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      uint32_t time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
      struct wl_resource *callback, *tmp;
      wl_resource_for_each_safe(callback, tmp, &surface->frame_callbacks) {
        wl_callback_send_done(callback, time);
        wl_resource_destroy(callback);
      }
    }

    bridge_surface_send_presentation_feedback(surface);

    if (surface->previous_buffer) {
      wl_list_remove(&surface->previous_buffer_destroy_listener.link);
      wl_list_init(&surface->previous_buffer_destroy_listener.link);
      bridge_syncobj_signal_release(surface);
      wl_buffer_send_release(surface->previous_buffer);
      surface->previous_buffer = NULL;
    }
  }
}

static void surface_set_buffer_transform(struct wl_client *client,
                                         struct wl_resource *resource,
                                         int32_t transform) {}

static void surface_set_buffer_scale(struct wl_client *client,
                                     struct wl_resource *resource,
                                     int32_t scale) {
  struct bridge_surface *surface = bridge_surface_from_resource(resource);
  if (!surface)
    return;

  surface->buffer_scale = scale;

  if (surface->plexy_window) {
    plexy_window_set_buffer_scale(surface->plexy_window, scale);
  }
}

static void surface_damage_buffer(struct wl_client *client,
                                  struct wl_resource *resource, int32_t x,
                                  int32_t y, int32_t width, int32_t height) {

  surface_damage(client, resource, x, y, width, height);
}

static void surface_offset(struct wl_client *client,
                           struct wl_resource *resource, int32_t x, int32_t y) {

  struct bridge_surface *surface = bridge_surface_from_resource(resource);
  if (!surface)
    return;

  surface->pending_dx = x;
  surface->pending_dy = y;
}

static const struct wl_surface_interface surface_implementation = {
    .destroy = surface_destroy,
    .attach = surface_attach,
    .damage = surface_damage,
    .frame = surface_frame,
    .set_opaque_region = surface_set_opaque_region,
    .set_input_region = surface_set_input_region,
    .commit = surface_commit,
    .set_buffer_transform = surface_set_buffer_transform,
    .set_buffer_scale = surface_set_buffer_scale,
    .damage_buffer = surface_damage_buffer,
    .offset = surface_offset,
};

static void surface_client_destroy(struct wl_listener *listener, void *data) {
  struct bridge_surface *surface =
      wl_container_of(listener, surface, client_destroy_listener);

  surface->resource = NULL;
}

static void surface_resource_destroy(struct wl_resource *resource) {
  struct bridge_surface *surface = bridge_surface_from_resource(resource);
  if (surface) {
    wl_list_remove(&surface->client_destroy_listener.link);
    bridge_surface_destroy(surface);
  }
}

static void compositor_create_surface(struct wl_client *client,
                                      struct wl_resource *resource,
                                      uint32_t id) {
  struct wl_resource *surface_resource = wl_resource_create(
      client, &wl_surface_interface, wl_resource_get_version(resource), id);

  if (!surface_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct bridge_surface *surface = bridge_surface_create(surface_resource);
  if (!surface) {
    wl_resource_destroy(surface_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource_set_implementation(surface_resource, &surface_implementation,
                                 surface, surface_resource_destroy);

  surface->client_destroy_listener.notify = surface_client_destroy;
  wl_client_add_destroy_listener(client, &surface->client_destroy_listener);

  bridge_notify_surface_created(surface);
}

static void region_destroy(struct wl_client *client,
                           struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void region_resource_destroy(struct wl_resource *resource) {
  struct bridge_region_resource *region = wl_resource_get_user_data(resource);
  if (!region)
    return;
  region_state_clear(&region->state);
  free(region);
}

static void region_add(struct wl_client *client, struct wl_resource *resource,
                       int32_t x, int32_t y, int32_t width, int32_t height) {
  struct bridge_region_resource *region = wl_resource_get_user_data(resource);
  if (!region)
    return;
  if (!region_state_append(&region->state, true, x, y, width, height)) {
    wl_resource_post_no_memory(resource);
  }
}

static void region_subtract(struct wl_client *client,
                            struct wl_resource *resource, int32_t x, int32_t y,
                            int32_t width, int32_t height) {
  struct bridge_region_resource *region = wl_resource_get_user_data(resource);
  if (!region)
    return;
  if (!region_state_append(&region->state, false, x, y, width, height)) {
    wl_resource_post_no_memory(resource);
  }
}

static const struct wl_region_interface region_implementation = {
    .destroy = region_destroy,
    .add = region_add,
    .subtract = region_subtract,
};

static void compositor_create_region(struct wl_client *client,
                                     struct wl_resource *resource,
                                     uint32_t id) {
  struct wl_resource *region_resource = wl_resource_create(
      client, &wl_region_interface, wl_resource_get_version(resource), id);

  if (!region_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  struct bridge_region_resource *region = calloc(1, sizeof(*region));
  if (!region) {
    wl_resource_destroy(region_resource);
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource_set_implementation(region_resource, &region_implementation,
                                 region, region_resource_destroy);
}

static const struct wl_compositor_interface compositor_implementation = {
    .create_surface = compositor_create_surface,
    .create_region = compositor_create_region,
};

void bind_compositor(struct wl_client *client, void *data, uint32_t version,
                     uint32_t id) {
  struct wl_resource *resource =
      wl_resource_create(client, &wl_compositor_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &compositor_implementation, NULL,
                                 NULL);
}
