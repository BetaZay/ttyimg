#include "drm.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/* Initialize DRM, choose a connector/CRTC, and create a dumb buffer
   sized to the full-screen resolution. */
int drm_fb_init(drm_fb_t *fb, const char *dri_device)
{
    memset(fb, 0, sizeof(*fb));

    fb->fd = open(dri_device, O_RDWR);
    if (fb->fd < 0) {
        fprintf(stderr, "Could not open DRM device %s\n", dri_device);
        return 0;
    }

    fb->res = drmModeGetResources(fb->fd);
    if (!fb->res) {
        fprintf(stderr, "Could not get DRM resources\n");
        close(fb->fd);
        return 0;
    }

    /* Select the first connected connector that has modes */
    for (int i = 0; i < fb->res->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(fb->fd, fb->res->connectors[i]);
        if (!conn)
            continue;
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            fb->conn = conn;
            break;
        }
        drmModeFreeConnector(conn);
    }
    if (!fb->conn) {
        fprintf(stderr, "No connected connector found\n");
        drmModeFreeResources(fb->res);
        close(fb->fd);
        return 0;
    }

    /* Choose the preferred mode, or fall back to the first mode */
    int found = 0;
    for (int i = 0; i < fb->conn->count_modes; i++) {
        if (fb->conn->modes[i].type & DRM_MODE_TYPE_PREFERRED) {
            fb->mode = fb->conn->modes[i];
            found = 1;
            break;
        }
    }
    if (!found)
        fb->mode = fb->conn->modes[0];

    /* Get the encoder */
    if (fb->conn->encoder_id)
        fb->enc = drmModeGetEncoder(fb->fd, fb->conn->encoder_id);
    if (!fb->enc) {
        fprintf(stderr, "Could not get encoder\n");
        drmModeFreeConnector(fb->conn);
        drmModeFreeResources(fb->res);
        close(fb->fd);
        return 0;
    }

    /* Get the current CRTC */
    fb->crtc = drmModeGetCrtc(fb->fd, fb->enc->crtc_id);
    if (!fb->crtc) {
        fprintf(stderr, "Could not get CRTC\n");
        drmModeFreeEncoder(fb->enc);
        drmModeFreeConnector(fb->conn);
        drmModeFreeResources(fb->res);
        close(fb->fd);
        return 0;
    }

    /* Create a dumb buffer with dimensions equal to the full screen */
    fb->dumb.width = fb->mode.hdisplay;
    fb->dumb.height = fb->mode.vdisplay;
    fb->dumb.bpp = 32;
    if (ioctl(fb->fd, DRM_IOCTL_MODE_CREATE_DUMB, &fb->dumb) < 0) {
        fprintf(stderr, "Could not create dumb buffer (err=%d)\n", errno);
        drmModeFreeCrtc(fb->crtc);
        drmModeFreeEncoder(fb->enc);
        drmModeFreeConnector(fb->conn);
        drmModeFreeResources(fb->res);
        close(fb->fd);
        return 0;
    }

    /* Add the dumb buffer as a framebuffer.
       For XRGB8888, many examples use depth=24 and bpp=32. If this fails,
       try using depth=32. */
    if (drmModeAddFB(fb->fd, fb->dumb.width, fb->dumb.height, 24, 32,
                     fb->dumb.pitch, fb->dumb.handle, &fb->fb_id) < 0) {
        fprintf(stderr, "Could not add framebuffer (err=%d)\n", errno);
        struct drm_mode_destroy_dumb destroy = {0};
        destroy.handle = fb->dumb.handle;
        ioctl(fb->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        drmModeFreeCrtc(fb->crtc);
        drmModeFreeEncoder(fb->enc);
        drmModeFreeConnector(fb->conn);
        drmModeFreeResources(fb->res);
        close(fb->fd);
        return 0;
    }

    /* Map the dumb buffer so we can write pixel data into it */
    struct drm_mode_map_dumb map = {0};
    map.handle = fb->dumb.handle;
    if (ioctl(fb->fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
        fprintf(stderr, "DRM_IOCTL_MODE_MAP_DUMB failed (err=%d)\n", errno);
        drmModeRmFB(fb->fd, fb->fb_id);
        struct drm_mode_destroy_dumb destroy = {0};
        destroy.handle = fb->dumb.handle;
        ioctl(fb->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        drmModeFreeCrtc(fb->crtc);
        drmModeFreeEncoder(fb->enc);
        drmModeFreeConnector(fb->conn);
        drmModeFreeResources(fb->res);
        close(fb->fd);
        return 0;
    }
    fb->map = mmap(0, fb->dumb.size, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, map.offset);
    if (fb->map == MAP_FAILED) {
        fprintf(stderr, "mmap failed (err=%d)\n", errno);
        drmModeRmFB(fb->fd, fb->fb_id);
        struct drm_mode_destroy_dumb destroy = {0};
        destroy.handle = fb->dumb.handle;
        ioctl(fb->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        drmModeFreeCrtc(fb->crtc);
        drmModeFreeEncoder(fb->enc);
        drmModeFreeConnector(fb->conn);
        drmModeFreeResources(fb->res);
        close(fb->fd);
        return 0;
    }

    return 1;
}

/* Set the CRTC to display our framebuffer */
int drm_fb_set(drm_fb_t *fb)
{
    /* Regain DRM master (if dropped) */
    drmSetMaster(fb->fd);
    if (drmModeSetCrtc(fb->fd, fb->crtc->crtc_id, fb->fb_id,
                       0, 0, &fb->conn->connector_id, 1, &fb->mode) < 0) {
        fprintf(stderr, "Could not set CRTC (err=%d)\n", errno);
        return 0;
    }
    drmDropMaster(fb->fd);
    return 1;
}

/* Clean up DRM framebuffer resources */
int drm_fb_cleanup(drm_fb_t *fb)
{
    if (fb->map)
        munmap(fb->map, fb->dumb.size);
    
    struct drm_mode_destroy_dumb destroy = {0};
    destroy.handle = fb->dumb.handle;
    ioctl(fb->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    
    if (fb->crtc)
        drmModeFreeCrtc(fb->crtc);
    if (fb->enc)
        drmModeFreeEncoder(fb->enc);
    if (fb->conn)
        drmModeFreeConnector(fb->conn);
    if (fb->res)
        drmModeFreeResources(fb->res);
    if (fb->fd >= 0)
        close(fb->fd);
    
    return 1;
}
