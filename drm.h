#ifndef DRM_H
#define DRM_H

#include <stdint.h>
#include <xf86drmMode.h>

/* Structure to hold DRM framebuffer information */
typedef struct {
    int fd;                     /* DRM device file descriptor */
    drmModeRes *res;            /* DRM resources */
    drmModeConnector *conn;     /* Chosen connector */
    drmModeEncoder *enc;        /* Encoder for the connector */
    drmModeCrtc *crtc;          /* Current CRTC settings */
    drmModeModeInfo mode;       /* Chosen display mode */
    uint32_t fb_id;             /* Framebuffer ID */
    struct drm_mode_create_dumb dumb; /* Dumb buffer creation info */
    void *map;                  /* Mapped pointer to dumb buffer memory */
} drm_fb_t;

/* Initialize DRM:
 * Opens dri_device (e.g. "/dev/dri/card0"), selects the first connected connector,
 * gets its preferred mode, creates a dumb buffer of full-screen size, and maps it.
 * Returns 1 on success, 0 on failure.
 */
int drm_fb_init(drm_fb_t *fb, const char *dri_device);

/* Set the CRTC to display our framebuffer.
 * Returns 1 on success, 0 on failure.
 */
int drm_fb_set(drm_fb_t *fb);

/* Clean up all DRM framebuffer resources.
 * Returns 1 on success.
 */
int drm_fb_cleanup(drm_fb_t *fb);

#endif // DRM_H
