#ifndef DRMLIB_H_
#define DRMLIB_H_

char *drmlib_get_msg();
int drmlib_open();
void *drmlib_get_display();
void *drmlib_get_surface();
int drmlib_find_config(EGLDisplay egl_display, EGLConfig *configs, int count);
void drmlib_restore();
void drmlib_close();
void drmlib_swap();

#endif
