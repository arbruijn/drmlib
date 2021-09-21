#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "drmlib.h"

char msg[256];

char *drmlib_get_msg() {
	return msg;
}

static void set_msg(const char *s, ...) {
  va_list vp;
  va_start(vp, s);
  vsnprintf(msg, sizeof(msg), s, vp);
  va_end(vp);
}

static struct {
	int device;
	drmModeCrtc *crtc;
	uint32_t connector_id;
	drmModeModeInfo mode_info;
	struct gbm_device *gbm_device;
	struct gbm_surface *gbm_surface;
	int first;
	struct gbm_bo *bo;
} cur;

static drmModeConnector *find_connector(drmModeRes *resources) {
	int i;
	for (i = 0; i < resources->count_connectors; i++) {
		drmModeConnector *connector;
		if (!(connector = drmModeGetConnector(cur.device, resources->connectors[i])))
			continue;
		if (connector->connection == DRM_MODE_CONNECTED)
			return connector;
		drmModeFreeConnector(connector);
	}
	return NULL;
}
static int open_path(const char *path) {
	drmModeRes *resources = NULL;
	drmModeConnector *connector = NULL;
	drmModeEncoder *encoder = NULL;

	memset(&cur, 0, sizeof(cur));
	if ((cur.device = open(path, O_RDWR)) == -1) {
		set_msg("open %s failed: %s", path, strerror(errno));
		return 0;
	}
	if (!(resources = drmModeGetResources(cur.device))) {
		set_msg("drmModeGetResources failed: %s", strerror(errno));
		goto err;
	}
	if (!(connector = find_connector(resources))) {
		set_msg("no connected connector found");
		goto err;
	}
	cur.connector_id = connector->connector_id;
	cur.mode_info = connector->modes[0];
	if (!connector->encoder_id) {
		set_msg("no current encoder\n");
		goto err;
	}
	if (!(encoder = drmModeGetEncoder(cur.device, connector->encoder_id))) {
		set_msg("drmModeGetEncoder %d failed: %s\n", connector->encoder_id, strerror(errno));
		goto err;
	}
	if (!(cur.crtc = drmModeGetCrtc(cur.device, encoder->crtc_id))) {
		set_msg("drmModeGetCrtc %d failed: %s", encoder->crtc_id, strerror(errno));
		goto err;
	}
	drmModeFreeEncoder(encoder);
	drmModeFreeConnector(connector);
	drmModeFreeResources(resources);
	if (!(cur.gbm_device = gbm_create_device(cur.device))) {
		set_msg("gbm_create_device failed");
		goto err;
	}
	if (!(cur.gbm_surface = gbm_surface_create(cur.gbm_device,
		cur.mode_info.hdisplay, cur.mode_info.vdisplay,
		GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT|GBM_BO_USE_RENDERING))) {
		set_msg("gbm_surface_create failed");
		goto err;
	}
	cur.first = 1;
	return 1;
err:
	if (encoder)
		drmModeFreeEncoder(encoder);
	if (connector)
		drmModeFreeConnector(connector);
	if (resources)
		drmModeFreeResources(resources);
	close(cur.device);
	memset(&cur, 0, sizeof(cur));
	return 0;
}

int drmlib_open() {
	if (open_path("/dev/dri/card1"))
		return 1;
	if (open_path("/dev/dri/card0"))
		return 1;
	return 0;
}

void *drmlib_get_display() {
	return cur.gbm_device;
}

void *drmlib_get_surface() {
	return cur.gbm_surface;
}

int drmlib_find_config(EGLDisplay egl_display, EGLConfig *configs, int count) {
	int i;
	for (i = 0; i < count; i++) {
		EGLint id;
		if (!eglGetConfigAttrib(egl_display, configs[i], EGL_NATIVE_VISUAL_ID, &id))
			continue;
		if (id == GBM_FORMAT_XRGB8888)
			return i;
	}
}

void drmlib_restore() {
	drmModeSetCrtc(cur.device, cur.crtc->crtc_id, cur.crtc->buffer_id, cur.crtc->x, cur.crtc->y,
		&cur.connector_id, 1, &cur.crtc->mode);
}

void drmlib_close() {
	drmModeFreeCrtc(cur.crtc);
	gbm_surface_destroy(cur.gbm_surface);
	gbm_device_destroy(cur.gbm_device);
	close(cur.device);
	memset(&cur, 0, sizeof(cur));
}

static void drm_fb_destroy_callback(struct gbm_bo *bo, void *data) {
        int drm_fd = gbm_device_get_fd(gbm_bo_get_device(bo));
        uint32_t fb_id = (uint32_t)(uintptr_t)data;

        if (fb_id)
                drmModeRmFB(drm_fd, fb_id);
}


void drmlib_swap() {
	struct gbm_bo *bo = gbm_surface_lock_front_buffer(cur.gbm_surface);
	uint32_t fb_id = (uint32_t)(uintptr_t)gbm_bo_get_user_data(bo);
	if (!fb_id) {
		uint32_t handle = gbm_bo_get_handle(bo).u32;
		uint32_t pitch = gbm_bo_get_stride(bo);
		drmModeAddFB(cur.device, cur.mode_info.hdisplay, cur.mode_info.vdisplay, 24, 32, pitch, handle, &fb_id);
		gbm_bo_set_user_data(bo, (void *)(uintptr_t)fb_id, drm_fb_destroy_callback);
	}

	if (cur.first) {
		drmModeSetCrtc(cur.device, cur.crtc->crtc_id, fb_id, 0, 0, &cur.connector_id, 1, &cur.mode_info);
		cur.first = 0;
	}
	if (cur.bo)
		gbm_surface_release_buffer(cur.gbm_surface, cur.bo);
	cur.bo = bo;
}
