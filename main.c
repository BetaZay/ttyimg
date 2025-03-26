#include "drm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include <drm_fourcc.h>
#include <xf86drmMode.h>

/* stb_image for loading images (text overlay) */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* Helper to clamp an integer value between 0 and 255 */
static inline uint8_t clamp(int val) {
    if(val < 0) return 0;
    if(val > 255) return 255;
    return (uint8_t)val;
}

/* Get current time in milliseconds */
unsigned long get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000UL + tv.tv_usec / 1000;
}

int main(void)
{
    /* --- 1. Setup webcam capture via V4L2 --- */
    const char *video_device = "/dev/video0";
    int video_fd = open(video_device, O_RDWR);
    if (video_fd < 0) {
        perror("open video device");
        return EXIT_FAILURE;
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    /* Capture at 640x360 */
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 360;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(video_fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("VIDIOC_S_FMT");
        close(video_fd);
        return EXIT_FAILURE;
    }
    int frame_w = fmt.fmt.pix.width;
    int frame_h = fmt.fmt.pix.height;

    /* Request one buffer for streaming */
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(video_fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("VIDIOC_REQBUFS");
        close(video_fd);
        return EXIT_FAILURE;
    }

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (ioctl(video_fd, VIDIOC_QUERYBUF, &buf) < 0) {
        perror("VIDIOC_QUERYBUF");
        close(video_fd);
        return EXIT_FAILURE;
    }

    void *video_buffer = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                              MAP_SHARED, video_fd, buf.m.offset);
    if (video_buffer == MAP_FAILED) {
        perror("mmap video_buffer");
        close(video_fd);
        return EXIT_FAILURE;
    }

    if (ioctl(video_fd, VIDIOC_QBUF, &buf) < 0) {
        perror("VIDIOC_QBUF");
        munmap(video_buffer, buf.length);
        close(video_fd);
        return EXIT_FAILURE;
    }
    int type = buf.type;
    if (ioctl(video_fd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
        munmap(video_buffer, buf.length);
        close(video_fd);
        return EXIT_FAILURE;
    }

    /* --- 2. Load the pre-made text image for overlay ---
         This image (e.g. "text.png") should contain your text (e.g. "testing webcam...")
         with a transparent background if desired.
    */
    int text_w, text_h, text_channels;
    const char *text_filename = "txt.png";  // Adjust filename as needed
    unsigned char *text_data = stbi_load(text_filename, &text_w, &text_h, &text_channels, 4);
    if (!text_data) {
        fprintf(stderr, "Failed to load text overlay image %s\n", text_filename);
        // Not fatal; you could choose to continue without text overlay.
    }
    // Define a scaling factor for the text image (e.g. 0.8 to scale it down a bit)
    float text_scale = 0.8f;
    int overlay_text_w = text_data ? (int)(text_w * text_scale) : 0;
    int overlay_text_h = text_data ? (int)(text_h * text_scale) : 0;

    /* --- 3. Initialize DRM for full-screen primary framebuffer --- */
    const char *dri_device = "/dev/dri/card0";
    drm_fb_t fb;
    if (!drm_fb_init(&fb, dri_device)) {
        fprintf(stderr, "Failed to initialize DRM\n");
        if (text_data) stbi_image_free(text_data);
        munmap(video_buffer, buf.length);
        close(video_fd);
        return EXIT_FAILURE;
    }
    int screen_w = fb.dumb.width;
    int screen_h = fb.dumb.height;
    uint32_t *fb_pixels = (uint32_t *)fb.map;
    int fb_stride = fb.dumb.pitch / 4;

    /* --- 4. Live feed loop ---
         We'll capture and update frames continuously for 2.5 seconds.
         For each frame:
           - Dequeue frame from webcam.
           - Convert YUYV frame to XRGB8888.
           - Scale it to half the screen resolution.
           - Then, composite the video feed and the text image together,
             centering the composite block vertically and horizontally.
           - Re-queue the buffer.
    */
    unsigned long start_time = get_time_ms();
    unsigned long duration = 2500; // 2.5 seconds
    while (get_time_ms() - start_time < duration) {
        /* Dequeue a frame */
        if (ioctl(video_fd, VIDIOC_DQBUF, &buf) < 0) {
            perror("VIDIOC_DQBUF");
            break;
        }

        /* Allocate buffer for converted frame */
        uint32_t *conv_frame = malloc(frame_w * frame_h * sizeof(uint32_t));
        if (!conv_frame) {
            perror("malloc conv_frame");
            break;
        }
        /* Convert YUYV to XRGB8888.
           Each pair of pixels uses 4 bytes: Y0 U Y1 V.
        */
        unsigned char *src = video_buffer;
        for (int y = 0; y < frame_h; y++) {
            for (int x = 0; x < frame_w; x += 2) {
                int index = (y * frame_w + x) * 2;
                unsigned char Y0 = src[index];
                unsigned char U  = src[index + 1];
                unsigned char Y1 = src[index + 2];
                unsigned char V  = src[index + 3];

                int C0 = Y0 - 16;
                int C1 = Y1 - 16;
                int D = U - 128;
                int E = V - 128;
                int R0 = (298 * C0 + 409 * E + 128) >> 8;
                int G0 = (298 * C0 - 100 * D - 208 * E + 128) >> 8;
                int B0 = (298 * C0 + 516 * D + 128) >> 8;
                int R1 = (298 * C1 + 409 * E + 128) >> 8;
                int G1 = (298 * C1 - 100 * D - 208 * E + 128) >> 8;
                int B1 = (298 * C1 + 516 * D + 128) >> 8;
                conv_frame[y * frame_w + x]     = (clamp(R0) << 16) | (clamp(G0) << 8) | clamp(B0);
                conv_frame[y * frame_w + x + 1] = (clamp(R1) << 16) | (clamp(G1) << 8) | clamp(B1);
            }
        }

        /* Re-queue the buffer for the next frame */
        if (ioctl(video_fd, VIDIOC_QBUF, &buf) < 0) {
            perror("VIDIOC_QBUF");
            free(conv_frame);
            break;
        }

        /* --- Composite layout computation ---
             We scale the video feed to half the screen.
        */
        float target_video_w = screen_w / 2.0f;
        float target_video_h = screen_h / 2.0f;
        float scale_x = target_video_w / frame_w;
        float scale_y = target_video_h / frame_h;
        float video_scale = fmin(scale_x, scale_y);
        int video_scaled_w = frame_w * video_scale;
        int video_scaled_h = frame_h * video_scale;

        /* For the text image, we already computed overlay_text_w and overlay_text_h.
           Define a vertical spacing between video and text.
        */
        int spacing = 10;
        int composite_height = video_scaled_h + (text_data ? (spacing + overlay_text_h) : 0);
        int composite_width = video_scaled_w;
        if (text_data && overlay_text_w > composite_width)
            composite_width = overlay_text_w;

        /* Compute top-left of composite block centered on screen */
        int comp_x = (screen_w - composite_width) / 2;
        int comp_y = (screen_h - composite_height) / 2;
        /* Video feed position: centered within composite block */
        int video_dest_x = comp_x + (composite_width - video_scaled_w) / 2;
        int video_dest_y = comp_y;
        /* Text position: below video feed with spacing, centered horizontally */
        int text_dest_x = comp_x + (composite_width - (text_data ? overlay_text_w : 0)) / 2;
        int text_dest_y = comp_y + video_scaled_h + spacing;

        /* Clear the full screen to black */
        memset(fb_pixels, 0x00, fb.dumb.size);
        /* Nearest-neighbor scaling: copy conv_frame into fb.map for video feed */
        for (int dy = 0; dy < video_scaled_h; dy++) {
            for (int dx = 0; dx < video_scaled_w; dx++) {
                int sx = (int)(dx / video_scale);
                int sy = (int)(dy / video_scale);
                if (sx >= frame_w) sx = frame_w - 1;
                if (sy >= frame_h) sy = frame_h - 1;
                uint32_t pixel = conv_frame[sy * frame_w + sx];
                int fb_x = video_dest_x + dx;
                int fb_y = video_dest_y + dy;
                fb_pixels[fb_y * fb_stride + fb_x] = pixel;
            }
        }
        free(conv_frame);

        /* Blit the pre-made text overlay image, if available.
           We scale it using nearest-neighbor (simple approach) and draw it at the computed text_dest_x/text_dest_y.
        */
        if (text_data) {
            for (int y = 0; y < overlay_text_h; y++) {
                for (int x = 0; x < overlay_text_w; x++) {
                    /* Map the scaled coordinate back to the original text image */
                    int orig_x = (int)(x / text_scale);
                    int orig_y = (int)(y / text_scale);
                    if (orig_x >= text_w) orig_x = text_w - 1;
                    if (orig_y >= text_h) orig_y = text_h - 1;
                    int idx = (orig_y * text_w + orig_x) * 4;
                    uint32_t pixel = (text_data[idx] << 16) | (text_data[idx+1] << 8) | text_data[idx+2];
                    int fb_x = text_dest_x + x;
                    int fb_y = text_dest_y + y;
                    if (fb_x < screen_w && fb_y < screen_h)
                        fb_pixels[fb_y * fb_stride + fb_x] = pixel;
                }
            }
        }

        /* Update the display (primary plane is already set) */
        drm_fb_set(&fb);
    }

    /* --- 5. Cleanup --- */
    ioctl(video_fd, VIDIOC_STREAMOFF, &type);
    munmap(video_buffer, buf.length);
    close(video_fd);
    if (text_data) stbi_image_free(text_data);
    drm_fb_cleanup(&fb);
    return EXIT_SUCCESS;
}
