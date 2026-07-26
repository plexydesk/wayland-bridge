/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#define _GNU_SOURCE
#include "wayland_bridge.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wayland-server-protocol.h>

struct bridge_shm_pool {
  int fd;
  size_t size;
  void *data;
  int ref_count;
};

struct bridge_shm_buffer {
  struct bridge_shm_pool *pool;
  int32_t offset;
  int32_t width;
  int32_t height;
  int32_t stride;
  uint32_t format;
  void *data;
};

struct bridge_shm_buffer *
bridge_shm_buffer_from_resource(struct wl_resource *resource);

static void pool_ref(struct bridge_shm_pool *pool) {
  if (pool) {
    pool->ref_count++;
  }
}

static void pool_unref(struct bridge_shm_pool *pool) {
  if (!pool)
    return;
  pool->ref_count--;
  if (pool->ref_count <= 0) {
    if (pool->data) {
      munmap(pool->data, pool->size);
    }
    if (pool->fd >= 0) {
      close(pool->fd);
    }
    free(pool);
  }
}

static bool validate_dimensions(int32_t width, int32_t height, int32_t stride) {
  return width > 0 && height > 0 && stride > 0;
}

static int32_t bytes_per_pixel_for_format(uint32_t format) {
  switch (format) {
  case WL_SHM_FORMAT_ARGB8888:
  case WL_SHM_FORMAT_XRGB8888:
  case WL_SHM_FORMAT_ABGR8888:
  case WL_SHM_FORMAT_XBGR8888:
  case WL_SHM_FORMAT_RGBA8888:
  case WL_SHM_FORMAT_RGBX8888:
  case WL_SHM_FORMAT_BGRA8888:
  case WL_SHM_FORMAT_BGRX8888:
    return 4;
  case WL_SHM_FORMAT_RGB565:
  case WL_SHM_FORMAT_BGR565:
    return 2;
  default:
    return 0;
  }
}

static bool validate_bounds(struct bridge_shm_pool *pool, int32_t offset,
                            int32_t width, int32_t height, int32_t stride,
                            uint32_t format) {
  int32_t bpp = bytes_per_pixel_for_format(format);
  if (bpp == 0)
    return false;

  int64_t last_row = (int64_t)offset + (int64_t)stride * (height - 1);
  int64_t required = last_row + (int64_t)bpp * width;
  if (offset < 0 || height <= 0 || width <= 0 || stride <= 0) {
    return false;
  }
  if (required < 0 || required > (int64_t)pool->size) {
    return false;
  }
  return true;
}

static void shm_buffer_destroy(struct wl_client *client,
                               struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static const struct wl_buffer_interface shm_buffer_interface = {
    .destroy = shm_buffer_destroy,
};

static void shm_buffer_resource_destroy(struct wl_resource *resource) {
  struct bridge_shm_buffer *buffer = wl_resource_get_user_data(resource);
  if (!buffer)
    return;

  pool_unref(buffer->pool);
  free(buffer);
}

static void shm_pool_resource_destroy(struct wl_resource *resource) {
  struct bridge_shm_pool *pool = wl_resource_get_user_data(resource);
  if (!pool)
    return;
  pool_unref(pool);
  wl_resource_set_user_data(resource, NULL);
}

static void shm_pool_create_buffer(struct wl_client *client,
                                   struct wl_resource *resource, uint32_t id,
                                   int32_t offset, int32_t width,
                                   int32_t height, int32_t stride,
                                   uint32_t format) {
  struct bridge_shm_pool *pool = wl_resource_get_user_data(resource);
  if (!pool) {
    LOG_ERROR("SHM: invalid shm pool");
    wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD,
                           "invalid shm pool");
    return;
  }

  if (!validate_dimensions(width, height, stride)) {
    wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_STRIDE,
                           "invalid dimensions: %dx%d stride=%d", width, height,
                           stride);
    return;
  }

  int32_t bpp = bytes_per_pixel_for_format(format);
  if (bpp == 0) {
    wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FORMAT,
                           "unsupported shm format: %u", format);
    return;
  }

  if (!validate_bounds(pool, offset, width, height, stride, format)) {
    wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_STRIDE,
                           "buffer out of bounds");
    return;
  }

  struct bridge_shm_buffer *shm_buffer = calloc(1, sizeof(*shm_buffer));
  if (!shm_buffer) {
    wl_resource_post_no_memory(resource);
    return;
  }

  shm_buffer->pool = pool;
  shm_buffer->offset = offset;
  shm_buffer->width = width;
  shm_buffer->height = height;
  shm_buffer->stride = stride;
  shm_buffer->format = format;
  shm_buffer->data = (char *)pool->data + offset;

  pool_ref(pool);

  struct wl_resource *buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, id);

  if (!buffer_resource) {
    pool_unref(pool);
    free(shm_buffer);
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource_set_implementation(buffer_resource, &shm_buffer_interface,
                                 shm_buffer, shm_buffer_resource_destroy);
}

static void shm_pool_destroy(struct wl_client *client,
                             struct wl_resource *resource) {
  struct bridge_shm_pool *pool = wl_resource_get_user_data(resource);
  pool_unref(pool);
  wl_resource_set_user_data(resource, NULL);
  wl_resource_destroy(resource);
}

static void shm_pool_resize(struct wl_client *client,
                            struct wl_resource *resource, int32_t size) {
  struct bridge_shm_pool *pool = wl_resource_get_user_data(resource);

  LOG_TRACE("SHM pool resize requested: %d bytes (current: %zu)", size,
            pool ? pool->size : 0);

  if (!pool || size <= 0) {
    LOG_ERROR("SHM resize: invalid pool or size");
    wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD,
                           "invalid resize size: %d", size);
    return;
  }

  if ((size_t)size == pool->size) {
    LOG_TRACE("SHM resize: size unchanged, skipping");
    return;
  }

  if ((size_t)size < pool->size) {
    LOG_WARN("SHM resize: trying to shrink pool");
    wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD,
                           "cannot shrink shm pool from %zu to %d", pool->size,
                           size);
    return;
  }

  if ((size_t)size == pool->size) {
    LOG_TRACE("SHM resize: size unchanged, skipping");
    return;
  }

  void *new_map = mremap(pool->data, pool->size, (size_t)size, MREMAP_MAYMOVE);
  if (new_map == MAP_FAILED) {
    LOG_ERROR("SHM resize: mremap failed (errno=%d)", errno);
    wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD,
                           "mremap failed during resize");
    return;
  }

  pool->data = new_map;
  pool->size = (size_t)size;
  LOG_DEBUG("SHM pool resized to %d bytes", size);
}

static const struct wl_shm_pool_interface shm_pool_implementation = {
    .create_buffer = shm_pool_create_buffer,
    .destroy = shm_pool_destroy,
    .resize = shm_pool_resize,
};

static void shm_create_pool(struct wl_client *client,
                            struct wl_resource *resource, uint32_t id,
                            int32_t fd, int32_t size) {
  if (fd < 0 || size <= 0) {
    wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD,
                           "invalid fd=%d size=%d", fd, size);
    if (fd >= 0)
      close(fd);
    return;
  }

  int seals = fcntl(fd, F_GET_SEALS);
  if (seals >= 0) {
    LOG_TRACE("SHM pool: client fd has seals=0x%x", seals);
  }

  void *map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED) {
    wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD,
                           "mmap failed for shm pool");
    close(fd);
    return;
  }

  struct bridge_shm_pool *pool = calloc(1, sizeof(*pool));
  if (!pool) {
    wl_resource_post_no_memory(resource);
    munmap(map, size);
    close(fd);
    return;
  }

  pool->fd = fd;
  pool->size = (size_t)size;
  pool->data = map;
  pool->ref_count = 1;

  struct wl_resource *pool_resource = wl_resource_create(
      client, &wl_shm_pool_interface, wl_resource_get_version(resource), id);

  if (!pool_resource) {
    wl_resource_post_no_memory(resource);
    pool_unref(pool);
    return;
  }

  wl_resource_set_implementation(pool_resource, &shm_pool_implementation, pool,
                                 shm_pool_resource_destroy);
}

static void shm_release(struct wl_client *client,
                        struct wl_resource *resource) {

  wl_resource_destroy(resource);
}

static const struct wl_shm_interface shm_implementation = {
    .create_pool = shm_create_pool,
    .release = shm_release,
};

static const uint32_t supported_formats[] = {
    WL_SHM_FORMAT_ARGB8888, WL_SHM_FORMAT_XRGB8888, WL_SHM_FORMAT_ABGR8888,
    WL_SHM_FORMAT_XBGR8888, WL_SHM_FORMAT_RGBA8888, WL_SHM_FORMAT_RGBX8888,
    WL_SHM_FORMAT_BGRA8888, WL_SHM_FORMAT_BGRX8888, WL_SHM_FORMAT_RGB565,
    WL_SHM_FORMAT_BGR565,
};

void bind_shm(struct wl_client *client, void *data, uint32_t version,
              uint32_t id) {
  struct wl_resource *resource =
      wl_resource_create(client, &wl_shm_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &shm_implementation, NULL, NULL);

  for (size_t i = 0;
       i < sizeof(supported_formats) / sizeof(supported_formats[0]); ++i) {
    wl_shm_send_format(resource, supported_formats[i]);
  }
}

struct bridge_shm_buffer *
bridge_shm_buffer_from_resource(struct wl_resource *resource) {
  if (!resource)
    return NULL;
  if (!wl_resource_instance_of(resource, &wl_buffer_interface,
                               &shm_buffer_interface)) {
    return NULL;
  }
  return wl_resource_get_user_data(resource);
}

uint32_t bridge_shm_buffer_get_width(const struct bridge_shm_buffer *buffer) {
  return buffer ? (uint32_t)buffer->width : 0;
}

uint32_t bridge_shm_buffer_get_height(const struct bridge_shm_buffer *buffer) {
  return buffer ? (uint32_t)buffer->height : 0;
}

uint32_t bridge_shm_buffer_get_stride(const struct bridge_shm_buffer *buffer) {
  return buffer ? (uint32_t)buffer->stride : 0;
}

uint32_t bridge_shm_buffer_get_format(const struct bridge_shm_buffer *buffer) {
  return buffer ? buffer->format : 0;
}

int32_t bridge_shm_buffer_get_offset(const struct bridge_shm_buffer *buffer) {
  return buffer ? buffer->offset : 0;
}

void *bridge_shm_buffer_get_pool_data(const struct bridge_shm_buffer *buffer) {
  return (buffer && buffer->pool) ? buffer->pool->data : NULL;
}

size_t bridge_shm_buffer_get_pool_size(const struct bridge_shm_buffer *buffer) {
  return (buffer && buffer->pool) ? buffer->pool->size : 0;
}

void *bridge_shm_buffer_get_data(const struct bridge_shm_buffer *buffer) {

  if (!buffer || !buffer->pool || !buffer->pool->data) {
    return NULL;
  }
  return (char *)buffer->pool->data + buffer->offset;
}
