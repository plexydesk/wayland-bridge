/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */


#define _GNU_SOURCE
#include "linux-drm-syncobj-v1-protocol.h"
#include "wayland_bridge.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>

struct syncobj_timeline {
  struct wl_resource *resource;
  int fd;
  uint32_t drm_handle;
};

struct syncobj_surface {
  struct wl_resource *resource;
  struct bridge_surface *bridge_surface;

  uint32_t pending_acquire_handle;
  uint64_t pending_acquire_point;
  bool has_pending_acquire;

  uint32_t pending_release_handle;
  uint64_t pending_release_point;
  bool has_pending_release;
};

static void timeline_destroy(struct wl_client *client,
                             struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static const struct wp_linux_drm_syncobj_timeline_v1_interface timeline_impl = {
    .destroy = timeline_destroy,
};

static void timeline_resource_destroy(struct wl_resource *resource) {
  struct syncobj_timeline *tl = wl_resource_get_user_data(resource);
  if (tl) {
    int drm = bridge_dmabuf_get_drm_fd();
    if (drm >= 0 && tl->drm_handle)
      drmSyncobjDestroy(drm, tl->drm_handle);
    if (tl->fd >= 0)
      close(tl->fd);
    free(tl);
  }
}

static void syncobj_surface_destroy(struct wl_client *client,
                                    struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void syncobj_surface_set_acquire_point(struct wl_client *client,
                                              struct wl_resource *resource,
                                              struct wl_resource *timeline,
                                              uint32_t point_hi,
                                              uint32_t point_lo) {
  (void)client;
  struct syncobj_surface *ss = wl_resource_get_user_data(resource);
  if (!ss)
    return;
  struct syncobj_timeline *tl = wl_resource_get_user_data(timeline);
  if (!tl)
    return;
  ss->pending_acquire_handle = tl->drm_handle;
  ss->pending_acquire_point = ((uint64_t)point_hi << 32) | point_lo;
  ss->has_pending_acquire = true;
}

static void syncobj_surface_set_release_point(struct wl_client *client,
                                              struct wl_resource *resource,
                                              struct wl_resource *timeline,
                                              uint32_t point_hi,
                                              uint32_t point_lo) {
  (void)client;
  struct syncobj_surface *ss = wl_resource_get_user_data(resource);
  if (!ss)
    return;
  struct syncobj_timeline *tl = wl_resource_get_user_data(timeline);
  if (!tl)
    return;
  ss->pending_release_handle = tl->drm_handle;
  ss->pending_release_point = ((uint64_t)point_hi << 32) | point_lo;
  ss->has_pending_release = true;
}

static const struct wp_linux_drm_syncobj_surface_v1_interface
    syncobj_surface_impl = {
        .destroy = syncobj_surface_destroy,
        .set_acquire_point = syncobj_surface_set_acquire_point,
        .set_release_point = syncobj_surface_set_release_point,
};

static void syncobj_surface_resource_destroy(struct wl_resource *resource) {
  struct syncobj_surface *ss = wl_resource_get_user_data(resource);
  if (ss) {
    if (ss->bridge_surface)
      ss->bridge_surface->syncobj_surface = NULL;
    free(ss);
  }
}

static void manager_destroy(struct wl_client *client,
                            struct wl_resource *resource) {
  (void)client;
  wl_resource_destroy(resource);
}

static void manager_get_surface(struct wl_client *client,
                                struct wl_resource *resource, uint32_t id,
                                struct wl_resource *surface) {
  struct bridge_surface *bs = bridge_surface_from_resource(surface);
  if (!bs) {
    wl_resource_post_error(resource,
                           WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_SURFACE,
                           "wl_surface has no bridge_surface");
    return;
  }

  if (bs->syncobj_surface) {
    wl_resource_post_error(resource,
                           WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_SURFACE_EXISTS,
                           "syncobj_surface already exists for this surface");
    return;
  }

  struct wl_resource *ss_res =
      wl_resource_create(client, &wp_linux_drm_syncobj_surface_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!ss_res) {
    wl_client_post_no_memory(client);
    return;
  }

  struct syncobj_surface *ss = calloc(1, sizeof(*ss));
  if (!ss) {
    wl_resource_destroy(ss_res);
    wl_resource_post_no_memory(resource);
    return;
  }

  ss->resource = ss_res;
  ss->bridge_surface = bs;
  bs->syncobj_surface = ss;

  wl_resource_set_implementation(ss_res, &syncobj_surface_impl, ss,
                                 syncobj_surface_resource_destroy);
}

static void manager_import_timeline(struct wl_client *client,
                                    struct wl_resource *resource, uint32_t id,
                                    int32_t fd) {
  int drm = bridge_dmabuf_get_drm_fd();
  if (drm < 0) {
    close(fd);
    wl_resource_post_error(
        resource, WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_INVALID_TIMELINE,
        "DRM device not available for syncobj import");
    return;
  }

  uint32_t handle = 0;
  int ret = drmSyncobjFDToHandle(drm, fd, &handle);
  close(fd);

  if (ret != 0 || handle == 0) {
    wl_resource_post_error(
        resource, WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_INVALID_TIMELINE,
        "failed to import syncobj timeline fd");
    return;
  }

  struct wl_resource *tl_res =
      wl_resource_create(client, &wp_linux_drm_syncobj_timeline_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!tl_res) {
    drmSyncobjDestroy(drm, handle);
    wl_client_post_no_memory(client);
    return;
  }

  struct syncobj_timeline *tl = calloc(1, sizeof(*tl));
  if (!tl) {
    drmSyncobjDestroy(drm, handle);
    wl_resource_destroy(tl_res);
    wl_resource_post_no_memory(resource);
    return;
  }

  tl->resource = tl_res;
  tl->fd = -1;
  tl->drm_handle = handle;

  wl_resource_set_implementation(tl_res, &timeline_impl, tl,
                                 timeline_resource_destroy);
}

static const struct wp_linux_drm_syncobj_manager_v1_interface
    syncobj_manager_impl = {
        .destroy = manager_destroy,
        .get_surface = manager_get_surface,
        .import_timeline = manager_import_timeline,
};

void bind_drm_syncobj_manager(struct wl_client *client, void *data,
                              uint32_t version, uint32_t id) {
  (void)data;
  struct wl_resource *resource = wl_resource_create(
      client, &wp_linux_drm_syncobj_manager_v1_interface, version, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &syncobj_manager_impl, NULL, NULL);
}

bool bridge_syncobj_process_commit(struct bridge_surface *surface) {
  struct syncobj_surface *ss = surface->syncobj_surface;
  if (!ss)
    return true;

  bool has_buffer = (surface->pending_buffer != NULL);
  bool has_acquire = ss->has_pending_acquire;
  bool has_release = ss->has_pending_release;

  if (!has_buffer && !has_acquire && !has_release)
    return true;

  if (!has_buffer && (has_acquire || has_release)) {
    wl_resource_post_error(ss->resource,
                           WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_BUFFER,
                           "sync points set without buffer");
    return false;
  }

  if (has_buffer && has_acquire != has_release) {
    if (!has_acquire) {
      wl_resource_post_error(
          ss->resource, WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_ACQUIRE_POINT,
          "buffer attached without acquire point");
    } else {
      wl_resource_post_error(
          ss->resource, WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_RELEASE_POINT,
          "buffer attached without release point");
    }
    return false;
  }

  if (has_buffer && !has_acquire && !has_release)
    return true;

  if (ss->pending_acquire_handle == ss->pending_release_handle &&
      ss->pending_acquire_point >= ss->pending_release_point) {
    wl_resource_post_error(
        ss->resource, WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_CONFLICTING_POINTS,
        "acquire point must be before release point on same timeline");
    return false;
  }

  int drm = bridge_dmabuf_get_drm_fd();
  if (drm < 0)
    goto consume;

  {
    uint32_t acq_handle = ss->pending_acquire_handle;
    uint64_t acq_point = ss->pending_acquire_point;

    int ret =
        drmSyncobjTimelineWait(drm, &acq_handle, &acq_point, 1, 0,
                               DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT, NULL);
    if (ret != 0) {
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      int64_t abs_timeout =
          (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec + 16000000LL;
      ret =
          drmSyncobjTimelineWait(drm, &acq_handle, &acq_point, 1, abs_timeout,
                                 DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT, NULL);
      if (ret != 0)
        LOG_WARN("syncobj acquire wait timed out (16ms), proceeding anyway");
    }
  }

  surface->prev_buf_release_handle = surface->current_buf_release_handle;
  surface->prev_buf_release_point = surface->current_buf_release_point;
  surface->prev_buf_has_release = surface->current_buf_has_release;

  surface->current_buf_release_handle = ss->pending_release_handle;
  surface->current_buf_release_point = ss->pending_release_point;
  surface->current_buf_has_release = true;

consume:
  ss->has_pending_acquire = false;
  ss->has_pending_release = false;
  return true;
}

void bridge_syncobj_signal_release(struct bridge_surface *surface) {
  if (!surface->prev_buf_has_release)
    return;

  int drm = bridge_dmabuf_get_drm_fd();
  if (drm < 0)
    return;

  drmSyncobjTimelineSignal(drm, &surface->prev_buf_release_handle,
                           &surface->prev_buf_release_point, 1);
  surface->prev_buf_has_release = false;
}
