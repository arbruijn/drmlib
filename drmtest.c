#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include "drmlib.h"

static EGLint attributes[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 0,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_NONE
};

static const EGLint context_attribs[] = {
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE
};

static void draw(float progress) {
	glClearColor(1.0f-progress, progress, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
}

static int64_t get_time_ns(void) {
        struct timespec tv;
        clock_gettime(CLOCK_MONOTONIC, &tv);
        return tv.tv_nsec + tv.tv_sec * INT64_C(1000000000);
}

int main() {
	if (!drmlib_open()) {
		fprintf(stderr, "%s\n", drmlib_get_msg());
		return EXIT_FAILURE;
	}
	EGLDisplay display = eglGetDisplay(drmlib_get_display());
	eglInitialize(display, NULL ,NULL);
	const char *ver = eglQueryString(display, EGL_VERSION);
	printf("EGL_VERSION = %s\n", ver);
	eglBindAPI(EGL_OPENGL_API);
	int count;
	eglGetConfigs(display, NULL, 0, &count);
	EGLConfig *configs = malloc(count * sizeof *configs);
	int num_config;
	eglChooseConfig(display, attributes, configs, count, &num_config);
	int config_index = drmlib_find_config(display,configs,num_config);
	EGLContext context = eglCreateContext(display, configs[config_index], EGL_NO_CONTEXT, context_attribs);
	EGLSurface egl_surface = eglCreateWindowSurface(display, configs[config_index],
		(NativeWindowType)drmlib_get_surface(), NULL);
	free(configs);
	eglMakeCurrent(display, egl_surface, egl_surface, context);

	printf("OpenGL information:\n");
	printf("  version: \"%s\"\n", glGetString(GL_VERSION));
	printf("  shading language version: \"%s\"\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	printf("  vendor: \"%s\"\n", glGetString(GL_VENDOR));
	printf("  renderer: \"%s\"\n", glGetString(GL_RENDERER));
	//printf("  extensions: \"%s\"\n", glGetString(GL_EXTENSIONS));

	int i;
	int64_t t = get_time_ns();
	for (i = 0; get_time_ns() - t < INT64_C(1000000000); i++) {
		draw((i % 600) / 600.0f);
		eglSwapBuffers(display, egl_surface);
		drmlib_swap();
	}
	t = get_time_ns() - t;
	printf("%d frames, %f sec, %f fps\n", i, t / 1e9, (double)i / (double)t * 1e9);

	drmlib_restore();
	eglDestroySurface(display, egl_surface);
	eglDestroyContext(display, context);
	eglTerminate(display);
	drmlib_close();
}
