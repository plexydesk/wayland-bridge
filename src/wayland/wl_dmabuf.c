/*
 * Copyright (C) 2024-2026 Siraj Razick
 * SPDX-License-Identifier: AGPL-3.0-only
 */



#define _GNU_SOURCE
#include "linux-dmabuf-v1-protocol.h"
#include "wayland_bridge.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <gbm.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <xf86drm.h>

#define MAX_DMABUF_PLANES 4

struct dmabuf_attributes {
  int32_t width;
  int32_t height;
  uint32_t format;
  uint32_t flags;
  int n_planes;
  int fd[MAX_DMABUF_PLANES];
  uint32_t offset[MAX_DMABUF_PLANES];
  uint32_t stride[MAX_DMABUF_PLANES];
  uint64_t modifier;
};

struct linux_dmabuf_buffer {
  struct wl_resource *buffer_resource;
  struct wl_resource *params_resource;
  struct dmabuf_attributes attributes;

  EGLImageKHR egl_image;
  GLuint texture;
};

struct dmabuf_feedback_format_table {
  int fd;
  size_t size;
  struct {
    uint32_t format;
    uint32_t padding;
    uint64_t modifier;
  } *data;
  size_t num_entries;
};

static int drm_fd = -1;
static struct gbm_device *gbm_device = NULL;
static EGLDisplay egl_display = EGL_NO_DISPLAY;
static EGLContext egl_context = EGL_NO_CONTEXT;
static bool dmabuf_initialized = false;
static bool dmabuf_supported = false;
static dev_t main_device = 0;

int bridge_dmabuf_get_drm_fd(void) { return drm_fd; }

static struct dmabuf_feedback_format_table format_table = {.fd = -1};

static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR_ptr = NULL;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR_ptr = NULL;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES_ptr =
    NULL;
static PFNEGLQUERYDMABUFFORMATSEXTPROC eglQueryDmaBufFormatsEXT_ptr = NULL;
static PFNEGLQUERYDMABUFMODIFIERSEXTPROC eglQueryDmaBufModifiersEXT_ptr = NULL;

static int create_anonymous_file(size_t size) {
  int fd = memfd_create("dmabuf-format-table", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0)
    return -1;

  if (ftruncate(fd, size) < 0) {
    close(fd);
    return -1;
  }

  return fd;
}

static bool create_headless_egl(void) {

  drm_fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
  if (drm_fd < 0) {
    drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (drm_fd < 0) {
      LOG_ERROR("DMA-BUF: Failed to open DRM device");
      return false;
    }
  }

  struct stat st;
  if (fstat(drm_fd, &st) == 0) {
    main_device = st.st_rdev;
    LOG_DEBUG("DMA-BUF: Main device = %u:%u", major(main_device),
              minor(main_device));
  }

  gbm_device = gbm_create_device(drm_fd);
  if (!gbm_device) {
    LOG_ERROR("DMA-BUF: Failed to create GBM device");
    close(drm_fd);
    drm_fd = -1;
    return false;
  }

  PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT =
      (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress(
          "eglGetPlatformDisplayEXT");

  if (eglGetPlatformDisplayEXT) {
    egl_display =
        eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gbm_device, NULL);
  } else {
    egl_display = eglGetDisplay((EGLNativeDisplayType)gbm_device);
  }

  if (egl_display == EGL_NO_DISPLAY) {
    LOG_ERROR("DMA-BUF: Failed to get EGL display");
    gbm_device_destroy(gbm_device);
    gbm_device = NULL;
    close(drm_fd);
    drm_fd = -1;
    return false;
  }

  EGLint major, minor;
  if (!eglInitialize(egl_display, &major, &minor)) {
    LOG_ERROR("DMA-BUF: Failed to initialize EGL");
    egl_display = EGL_NO_DISPLAY;
    gbm_device_destroy(gbm_device);
    gbm_device = NULL;
    close(drm_fd);
    drm_fd = -1;
    return false;
  }

  LOG_DEBUG("DMA-BUF: EGL %d.%d initialized", major, minor);

  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    LOG_ERROR("DMA-BUF: Failed to bind OpenGL ES API");
    eglTerminate(egl_display);
    egl_display = EGL_NO_DISPLAY;
    gbm_device_destroy(gbm_device);
    gbm_device = NULL;
    close(drm_fd);
    drm_fd = -1;
    return false;
  }

  const char *extensions = eglQueryString(egl_display, EGL_EXTENSIONS);
  bool has_surfaceless =
      extensions && strstr(extensions, "EGL_KHR_surfaceless_context");

  static const EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

  EGLConfig config = NULL;
  EGLint num_configs = 0;

  if (has_surfaceless) {
    static const EGLint cfg_attribs[] = {EGL_RENDERABLE_TYPE,
                                         EGL_OPENGL_ES2_BIT, EGL_NONE};
    eglChooseConfig(egl_display, cfg_attribs, &config, 1, &num_configs);
  }

  if (num_configs < 1) {
    static const EGLint cfg_attribs_window[] = {EGL_SURFACE_TYPE,
                                                EGL_WINDOW_BIT,
                                                EGL_RENDERABLE_TYPE,
                                                EGL_OPENGL_ES2_BIT,
                                                EGL_RED_SIZE,
                                                8,
                                                EGL_GREEN_SIZE,
                                                8,
                                                EGL_BLUE_SIZE,
                                                8,
                                                EGL_NONE};
    eglChooseConfig(egl_display, cfg_attribs_window, &config, 1, &num_configs);
  }

  if (num_configs < 1) {
    LOG_ERROR("DMA-BUF: No suitable EGL config found");
    eglTerminate(egl_display);
    egl_display = EGL_NO_DISPLAY;
    gbm_device_destroy(gbm_device);
    gbm_device = NULL;
    close(drm_fd);
    drm_fd = -1;
    return false;
  }

  egl_context =
      eglCreateContext(egl_display, config, EGL_NO_CONTEXT, ctx_attribs);
  if (egl_context == EGL_NO_CONTEXT) {
    LOG_ERROR("DMA-BUF: Failed to create EGL context");
    eglTerminate(egl_display);
    egl_display = EGL_NO_DISPLAY;
    gbm_device_destroy(gbm_device);
    gbm_device = NULL;
    close(drm_fd);
    drm_fd = -1;
    return false;
  }

  if (!eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                      egl_context)) {
    LOG_ERROR("DMA-BUF: Failed to make EGL context current");
    eglDestroyContext(egl_display, egl_context);
    egl_context = EGL_NO_CONTEXT;
    eglTerminate(egl_display);
    egl_display = EGL_NO_DISPLAY;
    gbm_device_destroy(gbm_device);
    gbm_device = NULL;
    close(drm_fd);
    drm_fd = -1;
    return false;
  }

  LOG_DEBUG("DMA-BUF: Headless EGL context created (surfaceless=%s)",
            has_surfaceless ? "yes" : "no");

  eglCreateImageKHR_ptr =
      (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  eglDestroyImageKHR_ptr =
      (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  glEGLImageTargetTexture2DOES_ptr =
      (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress(
          "glEGLImageTargetTexture2DOES");
  eglQueryDmaBufFormatsEXT_ptr =
      (PFNEGLQUERYDMABUFFORMATSEXTPROC)eglGetProcAddress(
          "eglQueryDmaBufFormatsEXT");
  eglQueryDmaBufModifiersEXT_ptr =
      (PFNEGLQUERYDMABUFMODIFIERSEXTPROC)eglGetProcAddress(
          "eglQueryDmaBufModifiersEXT");

  bool has_import =
      extensions && strstr(extensions, "EGL_EXT_image_dma_buf_import");
  bool has_modifiers =
      extensions &&
      strstr(extensions, "EGL_EXT_image_dma_buf_import_modifiers");

  LOG_DEBUG("DMA-BUF: EGL_EXT_image_dma_buf_import: %s",
            has_import ? "YES" : "NO");
  LOG_DEBUG("DMA-BUF: EGL_EXT_image_dma_buf_import_modifiers: %s",
            has_modifiers ? "YES" : "NO");

  if (!has_import || !eglCreateImageKHR_ptr) {
    LOG_ERROR("DMA-BUF: Required EGL extensions not available");
    return false;
  }

  return true;
}

static bool create_format_table(void) {
  if (!eglQueryDmaBufFormatsEXT_ptr) {
    LOG_WARN("DMA-BUF: eglQueryDmaBufFormatsEXT not available");
    return false;
  }

  EGLint num_formats = 0;
  if (!eglQueryDmaBufFormatsEXT_ptr(egl_display, 0, NULL, &num_formats) ||
      num_formats <= 0) {
    LOG_WARN("DMA-BUF: No DMA-BUF formats supported");
    return false;
  }

  EGLint *formats = calloc(num_formats, sizeof(EGLint));
  if (!formats)
    return false;

  eglQueryDmaBufFormatsEXT_ptr(egl_display, num_formats, formats, &num_formats);
  LOG_DEBUG("DMA-BUF: %d formats supported by EGL", num_formats);

  size_t total_pairs = 0;
  for (int i = 0; i < num_formats; i++) {
    total_pairs++;
    total_pairs++;

    if (eglQueryDmaBufModifiersEXT_ptr) {
      EGLint num_mods = 0;
      eglQueryDmaBufModifiersEXT_ptr(egl_display, formats[i], 0, NULL, NULL,
                                     &num_mods);
      total_pairs += num_mods;
    }
  }

  size_t table_size = total_pairs * sizeof(*format_table.data);
  format_table.fd = create_anonymous_file(table_size);
  if (format_table.fd < 0) {
    free(formats);
    LOG_ERROR("DMA-BUF: Failed to create format table file");
    return false;
  }

  format_table.data = mmap(NULL, table_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                           format_table.fd, 0);
  if (format_table.data == MAP_FAILED) {
    close(format_table.fd);
    format_table.fd = -1;
    free(formats);
    LOG_ERROR("DMA-BUF: Failed to mmap format table");
    return false;
  }

  format_table.size = table_size;

  size_t idx = 0;
  for (int i = 0; i < num_formats; i++) {

    format_table.data[idx].format = formats[i];
    format_table.data[idx].padding = 0;
    format_table.data[idx].modifier = DRM_FORMAT_MOD_LINEAR;
    idx++;

    // Viewport-scaled dmabufs (e.g. Firefox's WebRender subsurface) go through
    // the bridge's readback path, which cannot resolve Intel X/Y/Yf tiling.
    // For these formats, force clients to allocate linear dmabufs.
    bool is_readback_format =
        (formats[i] == 0x34325241 || formats[i] == 0x34325258);

    bool has_explicit = false;
    if (eglQueryDmaBufModifiersEXT_ptr) {
      EGLint num_mods = 0;
      eglQueryDmaBufModifiersEXT_ptr(egl_display, formats[i], 0, NULL, NULL,
                                     &num_mods);

      if (num_mods > 0) {
        EGLuint64KHR *modifiers = calloc(num_mods, sizeof(EGLuint64KHR));
        EGLBoolean *external_only = calloc(num_mods, sizeof(EGLBoolean));

        if (modifiers && external_only) {
          eglQueryDmaBufModifiersEXT_ptr(egl_display, formats[i], num_mods,
                                         modifiers, external_only, &num_mods);

          for (int j = 0; j < num_mods; j++) {

            if (modifiers[j] != DRM_FORMAT_MOD_INVALID &&
                modifiers[j] != DRM_FORMAT_MOD_LINEAR && !external_only[j]) {

              uint8_t vendor = (modifiers[j] >> 56) & 0xFF;
              uint64_t mod_type = modifiers[j] & 0x00FFFFFFFFFFFFFF;
              if (vendor == 0x01) {

                if (mod_type >= 4) {
                  LOG_TRACE("DMA-BUF: Skipping Intel CCS modifier 0x%llx "
                            "(multi-plane)",
                            (unsigned long long)modifiers[j]);
                  continue;
                }

                if (is_readback_format && mod_type >= 1 && mod_type <= 3) {
                  LOG_TRACE("DMA-BUF: Skipping Intel tiled modifier 0x%llx "
                            "for readback format 0x%08x",
                            (unsigned long long)modifiers[j], formats[i]);
                  continue;
                }
              }

              if (vendor == 0x02) {
                LOG_TRACE("DMA-BUF: Skipping AMD tiled modifier 0x%llx",
                          (unsigned long long)modifiers[j]);
                continue;
              }

              format_table.data[idx].format = formats[i];
              format_table.data[idx].padding = 0;
              format_table.data[idx].modifier = modifiers[j];
              idx++;
              has_explicit = true;
            }
          }
        }

        free(modifiers);
        free(external_only);
      }
    }

    if (!has_explicit) {
      format_table.data[idx].format = formats[i];
      format_table.data[idx].padding = 0;
      format_table.data[idx].modifier = DRM_FORMAT_MOD_INVALID;
      idx++;
    }
  }

  format_table.num_entries = idx;
  free(formats);

  LOG_DEBUG("DMA-BUF: Format table created with %zu entries",
            format_table.num_entries);

  for (size_t i = 0;
       i < (format_table.num_entries < 8 ? format_table.num_entries : 8); i++) {
    LOG_TRACE("  [%zu] format=0x%08x modifier=0x%016llx", i,
              format_table.data[i].format,
              (unsigned long long)format_table.data[i].modifier);
  }
  if (format_table.num_entries > 8) {
    LOG_TRACE("  ... and %zu more entries", format_table.num_entries - 8);
  }

  return true;
}

static void linux_dmabuf_buffer_destroy(struct linux_dmabuf_buffer *buffer) {
  if (!buffer)
    return;

  if (buffer->texture) {
    glDeleteTextures(1, &buffer->texture);
    buffer->texture = 0;
  }
  if (buffer->egl_image != EGL_NO_IMAGE_KHR && eglDestroyImageKHR_ptr &&
      egl_display != EGL_NO_DISPLAY) {
    eglDestroyImageKHR_ptr(egl_display, buffer->egl_image);
    buffer->egl_image = EGL_NO_IMAGE_KHR;
  }

  for (int i = 0; i < buffer->attributes.n_planes; i++) {
    if (buffer->attributes.fd[i] >= 0) {
      close(buffer->attributes.fd[i]);
      buffer->attributes.fd[i] = -1;
    }
  }

  free(buffer);
}

static void destroy_params(struct wl_resource *params_resource) {
  struct linux_dmabuf_buffer *buffer =
      wl_resource_get_user_data(params_resource);
  if (buffer) {
    linux_dmabuf_buffer_destroy(buffer);
  }
}

static void linux_dmabuf_wl_buffer_destroy(struct wl_client *client,
                                           struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static const struct wl_buffer_interface linux_dmabuf_buffer_impl = {
    .destroy = linux_dmabuf_wl_buffer_destroy};

static void destroy_linux_dmabuf_wl_buffer(struct wl_resource *resource) {
  struct linux_dmabuf_buffer *buffer = wl_resource_get_user_data(resource);
  linux_dmabuf_buffer_destroy(buffer);
}

static bool import_dmabuf(struct linux_dmabuf_buffer *buffer) {
  if (!dmabuf_supported) {
    return false;
  }

  if (buffer->attributes.n_planes < 1 || buffer->attributes.n_planes > 4) {
    LOG_WARN("DMA-BUF: Invalid plane count: %d", buffer->attributes.n_planes);
    return false;
  }

  if (buffer->attributes.width == 0 || buffer->attributes.height == 0) {
    LOG_WARN("DMA-BUF: Invalid dimensions: %dx%d", buffer->attributes.width,
             buffer->attributes.height);
    return false;
  }

  if (buffer->attributes.fd[0] < 0) {
    LOG_WARN("DMA-BUF: Invalid fd for plane 0");
    return false;
  }

  LOG_TRACE(
      "DMA-BUF: Validated buffer %dx%d format=0x%x modifier=0x%llx planes=%d",
      buffer->attributes.width, buffer->attributes.height,
      buffer->attributes.format,
      (unsigned long long)buffer->attributes.modifier,
      buffer->attributes.n_planes);

  if (eglCreateImageKHR_ptr && glEGLImageTargetTexture2DOES_ptr &&
      egl_display != EGL_NO_DISPLAY) {

    static const EGLint plane_fd_attr[] = {
        EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE1_FD_EXT,
        EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE3_FD_EXT};
    static const EGLint plane_offset_attr[] = {
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT,
        EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT};
    static const EGLint plane_pitch_attr[] = {
        EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT,
        EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT};
    static const EGLint plane_mod_lo_attr[] = {
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
        EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT};
    static const EGLint plane_mod_hi_attr[] = {
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
        EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT};

    EGLint attribs[64];
    int i = 0;
    attribs[i++] = EGL_WIDTH;
    attribs[i++] = (EGLint)buffer->attributes.width;
    attribs[i++] = EGL_HEIGHT;
    attribs[i++] = (EGLint)buffer->attributes.height;
    attribs[i++] = EGL_LINUX_DRM_FOURCC_EXT;
    attribs[i++] = (EGLint)buffer->attributes.format;

    for (int p = 0; p < buffer->attributes.n_planes; p++) {
      attribs[i++] = plane_fd_attr[p];
      attribs[i++] = buffer->attributes.fd[p];
      attribs[i++] = plane_offset_attr[p];
      attribs[i++] = (EGLint)buffer->attributes.offset[p];
      attribs[i++] = plane_pitch_attr[p];
      attribs[i++] = (EGLint)buffer->attributes.stride[p];
      if (buffer->attributes.modifier != DRM_FORMAT_MOD_INVALID) {
        attribs[i++] = plane_mod_lo_attr[p];
        attribs[i++] = (EGLint)(buffer->attributes.modifier & 0xFFFFFFFF);
        attribs[i++] = plane_mod_hi_attr[p];
        attribs[i++] = (EGLint)(buffer->attributes.modifier >> 32);
      }
    }
    attribs[i++] = EGL_NONE;

    buffer->egl_image = eglCreateImageKHR_ptr(egl_display, EGL_NO_CONTEXT,
                                              EGL_LINUX_DMA_BUF_EXT,
                                              (EGLClientBuffer)NULL, attribs);
    if (buffer->egl_image == EGL_NO_IMAGE_KHR) {
      LOG_ERROR("DMA-BUF: eglCreateImageKHR failed (egl_err=0x%x) "
                "for %dx%d fmt=0x%x mod=0x%llx planes=%d",
                eglGetError(), buffer->attributes.width,
                buffer->attributes.height, buffer->attributes.format,
                (unsigned long long)buffer->attributes.modifier,
                buffer->attributes.n_planes);
      return false;
    }

    glGenTextures(1, &buffer->texture);
    glBindTexture(GL_TEXTURE_2D, buffer->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEGLImageTargetTexture2DOES_ptr(GL_TEXTURE_2D, buffer->egl_image);
    glBindTexture(GL_TEXTURE_2D, 0);
    LOG_TRACE("DMA-BUF: Created EGL image + texture=%u for %dx%d planes=%d",
              buffer->texture, buffer->attributes.width,
              buffer->attributes.height, buffer->attributes.n_planes);
  }

  return true;
}

static void params_destroy(struct wl_client *client,
                           struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void params_add(struct wl_client *client,
                       struct wl_resource *params_resource, int32_t name_fd,
                       uint32_t plane_idx, uint32_t offset, uint32_t stride,
                       uint32_t modifier_hi, uint32_t modifier_lo) {
  struct linux_dmabuf_buffer *buffer =
      wl_resource_get_user_data(params_resource);

  if (!buffer) {
    wl_resource_post_error(params_resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
                           "params was already used to create a wl_buffer");
    close(name_fd);
    return;
  }

  if (plane_idx >= MAX_DMABUF_PLANES) {
    wl_resource_post_error(params_resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX,
                           "plane index %u is too high", plane_idx);
    close(name_fd);
    return;
  }

  if (buffer->attributes.fd[plane_idx] != -1) {
    wl_resource_post_error(
        params_resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET,
        "a dmabuf has already been added for plane %u", plane_idx);
    close(name_fd);
    return;
  }

  uint64_t modifier;
  if (wl_resource_get_version(params_resource) <
      ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION)
    modifier = DRM_FORMAT_MOD_INVALID;
  else
    modifier = ((uint64_t)modifier_hi << 32) | modifier_lo;

  if (buffer->attributes.n_planes > 0 &&
      buffer->attributes.modifier != modifier) {
    wl_resource_post_error(params_resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT,
                           "modifier mismatch between planes");
    close(name_fd);
    return;
  }

  buffer->attributes.modifier = modifier;
  buffer->attributes.fd[plane_idx] = name_fd;
  buffer->attributes.offset[plane_idx] = offset;
  buffer->attributes.stride[plane_idx] = stride;
  buffer->attributes.n_planes++;

  LOG_TRACE(
      "DMA-BUF: params_add plane=%u fd=%d offset=%u stride=%u modifier=0x%llx",
      plane_idx, name_fd, offset, stride, (unsigned long long)modifier);
}

static void params_create_common(struct wl_client *client,
                                 struct wl_resource *params_resource,
                                 uint32_t buffer_id, int32_t width,
                                 int32_t height, uint32_t format,
                                 uint32_t flags) {
  struct linux_dmabuf_buffer *buffer =
      wl_resource_get_user_data(params_resource);

  if (!buffer) {
    wl_resource_post_error(params_resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
                           "params was already used to create a wl_buffer");
    return;
  }

  wl_resource_set_user_data(params_resource, NULL);
  buffer->params_resource = NULL;

  if (buffer->attributes.n_planes < 1) {
    wl_resource_post_error(params_resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                           "no dmabuf has been added to the params");
    goto err_out;
  }

  for (int i = 0; i < buffer->attributes.n_planes; i++) {
    if (buffer->attributes.fd[i] == -1) {
      wl_resource_post_error(params_resource,
                             ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                             "no dmabuf has been added for plane %i", i);
      goto err_out;
    }
  }

  if (width < 1 || height < 1) {
    wl_resource_post_error(params_resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS,
                           "invalid width %d or height %d", width, height);
    goto err_out;
  }

  buffer->attributes.width = width;
  buffer->attributes.height = height;
  buffer->attributes.format = format;
  buffer->attributes.flags = flags;

  for (int i = 0; i < buffer->attributes.n_planes; i++) {
    if ((uint64_t)buffer->attributes.offset[i] + buffer->attributes.stride[i] >
        UINT32_MAX) {
      wl_resource_post_error(params_resource,
                             ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                             "size overflow for plane %i", i);
      goto err_out;
    }

    off_t size = lseek(buffer->attributes.fd[i], 0, SEEK_END);
    if (size != -1) {
      if (buffer->attributes.offset[i] >= (uint32_t)size) {
        wl_resource_post_error(
            params_resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
            "invalid offset %u for plane %i", buffer->attributes.offset[i], i);
        goto err_out;
      }
    }
  }

  LOG_DEBUG(
      "DMA-BUF: Creating buffer %dx%d format=0x%x modifier=0x%llx planes=%d",
      width, height, format, (unsigned long long)buffer->attributes.modifier,
      buffer->attributes.n_planes);

  if (!import_dmabuf(buffer)) {
    LOG_ERROR("DMA-BUF: Failed to import buffer");
    goto err_failed;
  }

  buffer->buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, buffer_id);
  if (!buffer->buffer_resource) {
    wl_resource_post_no_memory(params_resource);
    goto err_buffer;
  }

  wl_resource_set_implementation(buffer->buffer_resource,
                                 &linux_dmabuf_buffer_impl, buffer,
                                 destroy_linux_dmabuf_wl_buffer);

  LOG_TRACE("DMA-BUF: Created buffer_resource=%p with impl=%p",
            (void *)buffer->buffer_resource, (void *)&linux_dmabuf_buffer_impl);

  if (buffer_id == 0) {
    zwp_linux_buffer_params_v1_send_created(params_resource,
                                            buffer->buffer_resource);
  }

  return;

err_buffer:

err_failed:
  if (buffer_id == 0) {
    zwp_linux_buffer_params_v1_send_failed(params_resource);
  } else {
    wl_resource_post_error(params_resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
                           "importing the supplied dmabufs failed");
  }

err_out:
  linux_dmabuf_buffer_destroy(buffer);
}

static void params_create(struct wl_client *client,
                          struct wl_resource *params_resource, int32_t width,
                          int32_t height, uint32_t format, uint32_t flags) {
  params_create_common(client, params_resource, 0, width, height, format,
                       flags);
}

static void params_create_immed(struct wl_client *client,
                                struct wl_resource *params_resource,
                                uint32_t buffer_id, int32_t width,
                                int32_t height, uint32_t format,
                                uint32_t flags) {
  params_create_common(client, params_resource, buffer_id, width, height,
                       format, flags);
}

static const struct zwp_linux_buffer_params_v1_interface buffer_params_impl = {
    .destroy = params_destroy,
    .add = params_add,
    .create = params_create,
    .create_immed = params_create_immed,
};

static void dmabuf_destroy(struct wl_client *client,
                           struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void dmabuf_create_params(struct wl_client *client,
                                 struct wl_resource *resource,
                                 uint32_t params_id) {
  struct linux_dmabuf_buffer *buffer = calloc(1, sizeof(*buffer));
  if (!buffer) {
    wl_resource_post_no_memory(resource);
    return;
  }

  for (int i = 0; i < MAX_DMABUF_PLANES; i++) {
    buffer->attributes.fd[i] = -1;
  }
  buffer->egl_image = EGL_NO_IMAGE_KHR;
  buffer->texture = 0;

  struct wl_resource *params_resource =
      wl_resource_create(client, &zwp_linux_buffer_params_v1_interface,
                         wl_resource_get_version(resource), params_id);

  if (!params_resource) {
    free(buffer);
    wl_resource_post_no_memory(resource);
    return;
  }

  buffer->params_resource = params_resource;
  wl_resource_set_implementation(params_resource, &buffer_params_impl, buffer,
                                 destroy_params);

  LOG_TRACE("DMA-BUF: create_params id=%u", params_id);
}

static void feedback_destroy(struct wl_client *client,
                             struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static const struct zwp_linux_dmabuf_feedback_v1_interface feedback_impl = {
    .destroy = feedback_destroy,
};

static void send_feedback(struct wl_resource *feedback_resource) {
  if (format_table.fd < 0 || !format_table.data) {
    LOG_ERROR("DMA-BUF: Format table not initialized!");
    zwp_linux_dmabuf_feedback_v1_send_done(feedback_resource);
    return;
  }

  zwp_linux_dmabuf_feedback_v1_send_format_table(
      feedback_resource, format_table.fd, format_table.size);

  struct wl_array device_array;
  wl_array_init(&device_array);
  dev_t *dev = wl_array_add(&device_array, sizeof(dev_t));
  if (dev)
    *dev = main_device;
  zwp_linux_dmabuf_feedback_v1_send_main_device(feedback_resource,
                                                &device_array);
  wl_array_release(&device_array);

  wl_array_init(&device_array);
  dev = wl_array_add(&device_array, sizeof(dev_t));
  if (dev)
    *dev = main_device;
  zwp_linux_dmabuf_feedback_v1_send_tranche_target_device(feedback_resource,
                                                          &device_array);
  wl_array_release(&device_array);

  zwp_linux_dmabuf_feedback_v1_send_tranche_flags(feedback_resource, 0);

  struct wl_array indices;
  wl_array_init(&indices);
  for (size_t i = 0; i < format_table.num_entries; i++) {
    uint16_t *idx = wl_array_add(&indices, sizeof(uint16_t));
    if (idx)
      *idx = (uint16_t)i;
  }
  zwp_linux_dmabuf_feedback_v1_send_tranche_formats(feedback_resource,
                                                    &indices);
  wl_array_release(&indices);

  zwp_linux_dmabuf_feedback_v1_send_tranche_done(feedback_resource);
  zwp_linux_dmabuf_feedback_v1_send_done(feedback_resource);

  LOG_TRACE("DMA-BUF: Sent feedback with %zu formats (device=%u:%u)",
            format_table.num_entries, major(main_device), minor(main_device));
}

static void dmabuf_get_default_feedback(struct wl_client *client,
                                        struct wl_resource *resource,
                                        uint32_t id) {
  struct wl_resource *feedback =
      wl_resource_create(client, &zwp_linux_dmabuf_feedback_v1_interface,
                         wl_resource_get_version(resource), id);

  if (!feedback) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(feedback, &feedback_impl, NULL, NULL);

  LOG_TRACE("DMA-BUF: get_default_feedback");
  send_feedback(feedback);
}

static void dmabuf_get_surface_feedback(struct wl_client *client,
                                        struct wl_resource *resource,
                                        uint32_t id,
                                        struct wl_resource *surface) {

  struct wl_resource *feedback =
      wl_resource_create(client, &zwp_linux_dmabuf_feedback_v1_interface,
                         wl_resource_get_version(resource), id);

  if (!feedback) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(feedback, &feedback_impl, NULL, NULL);

  LOG_TRACE("DMA-BUF: get_surface_feedback");
  send_feedback(feedback);
}

static const struct zwp_linux_dmabuf_v1_interface dmabuf_impl = {
    .destroy = dmabuf_destroy,
    .create_params = dmabuf_create_params,
    .get_default_feedback = dmabuf_get_default_feedback,
    .get_surface_feedback = dmabuf_get_surface_feedback,
};

void bind_dmabuf(struct wl_client *client, void *data, uint32_t version,
                 uint32_t id) {
  struct wl_resource *resource =
      wl_resource_create(client, &zwp_linux_dmabuf_v1_interface, version, id);

  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &dmabuf_impl, NULL, NULL);

  if (version < 4 && format_table.data) {
    for (size_t i = 0; i < format_table.num_entries; i++) {
      uint32_t fmt = format_table.data[i].format;
      uint64_t mod = format_table.data[i].modifier;

      if (version >= ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION) {
        zwp_linux_dmabuf_v1_send_modifier(resource, fmt, (uint32_t)(mod >> 32),
                                          (uint32_t)(mod & 0xFFFFFFFF));
      } else if (mod == DRM_FORMAT_MOD_INVALID ||
                 mod == DRM_FORMAT_MOD_LINEAR) {
        zwp_linux_dmabuf_v1_send_format(resource, fmt);
      }
    }
  }

  LOG_DEBUG("DMA-BUF: Client bound (version=%u)", version);
}

bool bridge_dmabuf_init(void) {
  if (dmabuf_initialized)
    return dmabuf_supported;
  dmabuf_initialized = true;

  if (getenv("PLEXY_DISABLE_DMABUF")) {
    LOG_INFO("DMA-BUF: Disabled via PLEXY_DISABLE_DMABUF");
    return false;
  }

  if (!create_headless_egl()) {
    LOG_ERROR("DMA-BUF: Failed to create headless EGL");
    return false;
  }

  if (!create_format_table()) {
    LOG_ERROR("DMA-BUF: Failed to create format table");
    return false;
  }

  dmabuf_supported = true;
  LOG_INFO("DMA-BUF: Initialization successful");
  return true;
}

void bridge_dmabuf_cleanup(void) {
  if (format_table.data && format_table.data != MAP_FAILED) {
    munmap(format_table.data, format_table.size);
    format_table.data = NULL;
  }
  if (format_table.fd >= 0) {
    close(format_table.fd);
    format_table.fd = -1;
  }

  if (egl_context != EGL_NO_CONTEXT) {
    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(egl_display, egl_context);
    egl_context = EGL_NO_CONTEXT;
  }

  if (egl_display != EGL_NO_DISPLAY) {
    eglTerminate(egl_display);
    egl_display = EGL_NO_DISPLAY;
  }

  if (gbm_device) {
    gbm_device_destroy(gbm_device);
    gbm_device = NULL;
  }

  if (drm_fd >= 0) {
    close(drm_fd);
    drm_fd = -1;
  }

  dmabuf_initialized = false;
  dmabuf_supported = false;
}

bool bridge_is_dmabuf_buffer(struct wl_resource *buffer_resource) {
  if (!buffer_resource) {
    return false;
  }
  bool result = wl_resource_instance_of(buffer_resource, &wl_buffer_interface,
                                        &linux_dmabuf_buffer_impl);

  LOG_TRACE("bridge_is_dmabuf_buffer: resource=%p result=%d",
            (void *)buffer_resource, result);
  return result;
}

struct bridge_dmabuf_buffer *
bridge_dmabuf_buffer_from_resource(struct wl_resource *resource) {
  if (!bridge_is_dmabuf_buffer(resource))
    return NULL;
  return (struct bridge_dmabuf_buffer *)wl_resource_get_user_data(resource);
}

int bridge_dmabuf_get_fd(struct bridge_dmabuf_buffer *buffer) {
  struct linux_dmabuf_buffer *b = (struct linux_dmabuf_buffer *)buffer;
  return b ? b->attributes.fd[0] : -1;
}

uint32_t bridge_dmabuf_get_width(struct bridge_dmabuf_buffer *buffer) {
  struct linux_dmabuf_buffer *b = (struct linux_dmabuf_buffer *)buffer;
  return b ? b->attributes.width : 0;
}

uint32_t bridge_dmabuf_get_height(struct bridge_dmabuf_buffer *buffer) {
  struct linux_dmabuf_buffer *b = (struct linux_dmabuf_buffer *)buffer;
  return b ? b->attributes.height : 0;
}

uint32_t bridge_dmabuf_get_stride(struct bridge_dmabuf_buffer *buffer) {
  struct linux_dmabuf_buffer *b = (struct linux_dmabuf_buffer *)buffer;
  return b ? b->attributes.stride[0] : 0;
}

uint32_t bridge_dmabuf_get_format(struct bridge_dmabuf_buffer *buffer) {
  struct linux_dmabuf_buffer *b = (struct linux_dmabuf_buffer *)buffer;
  return b ? b->attributes.format : 0;
}

uint64_t bridge_dmabuf_get_modifier(struct bridge_dmabuf_buffer *buffer) {
  struct linux_dmabuf_buffer *b = (struct linux_dmabuf_buffer *)buffer;
  return b ? b->attributes.modifier : DRM_FORMAT_MOD_INVALID;
}

uint32_t bridge_dmabuf_get_texture(struct bridge_dmabuf_buffer *buffer) {
  struct linux_dmabuf_buffer *b = (struct linux_dmabuf_buffer *)buffer;
  return b ? b->texture : 0;
}
