#include "simplewins.hpp"
#include "components.hpp"
#include "eventqueue.hpp"
#include "input.hpp"
#include "utils/libdrm_utils.hpp"
#include "utils/timer.hpp"

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sched.h>

const std::string dri_device = "/dev/dri/card0";

extern swins::EventQueue input_queue;
bool kill = false;
swins::utils::Timer timer;

static int fd;
static int *buf_mmap = NULL;
static uint32_t buf_id;
static uint64_t buf_size, buf_bpp, buf_pitch;
static int xres, yres;

static swins::cursor cursor;

static drmModeResPtr resources = NULL;
static drmModeConnectorPtr connector = NULL;
static drmModeEncoderPtr encoder = NULL;
static drmModeCrtcPtr crtc = NULL;
static drmModeModeInfoPtr resolution = NULL;

static void set_color (int r, int g, int b, int a,
    int x, int y, int w, int h) {
    int *buf = buf_mmap;

    int buf_w = xres;
    int buf_h = yres;
    
    assert (x >= 0 && x < buf_w);
    assert (y >= 0 && y < buf_h);
    assert (w > 0 && (x + w - 1 < buf_w));
    assert (h > 0 && (y + h - 1 < buf_h));

    int start = buf_w * y + x;
    int end = buf_w * (y + h - 1) + (x + w - 1); 

    for (int i = start; i <= end; i += buf_w) {
        for (int j = i; j < i + w; j++) {
            buf[j] = 0;
            buf[j] ^= (a << 24);
            buf[j] ^= (r << 16);
            buf[j] ^= (g << 8);
            buf[j] ^= b; 
        }
    } 
}

static int render () {
    int ret;
    if (!input_queue.count()) {
        // Nothing to do
        return 0;
    }
    bool redraw_windows = true;
    bool redraw_cursor = false;
    struct swins::cursor old_cursor = cursor;
    for (int i = 0; i < input_queue.count(); i++) {
        swins::Event next = input_queue.poll_event();
        switch (next.type) {
            case swins::MOUSE_MOTION:
                {
                    cursor.x += next.args[0];
                    cursor.y += next.args[1];

                    cursor.x = std::min(cursor.x, xres - cursor.w);
                    cursor.x = std::max(cursor.x, 0);
                    cursor.y = std::min(cursor.y, yres - cursor.h);
                    cursor.y = std::max(cursor.y, 0);
                    redraw_cursor = true;
                }
                break;
            default:
                printf ("Unrecoginzed event type\n");
                return -1;
        }
    }
    if (redraw_windows) {
        set_color (150, 75, 0, 255, 30, 40, xres / 4, yres / 4);
        set_color (0, 75, 100, 255, 530, 540, xres / 4, yres / 4);
    }

    // Cursor:
    if (redraw_cursor) {
        // Remove old cursor
        set_color (0, 0, 0, 0, old_cursor.x, old_cursor.y, cursor.w, cursor.h);
        set_color (cursor.color[0], cursor.color[1], cursor.color[2],
            255, cursor.x, cursor.y, cursor.w, cursor.h);
    }
    ret = drmModeSetCrtc (fd, crtc->crtc_id, buf_id, 0, 0, &connector->connector_id, 1, resolution);
    return ret;
}

/*
 * Most of the setup here is guided by this article: https://embear.ch/blog/drm-framebuffer
 */
int main() {
    int ret;
    struct drm_mode_create_dumb create_dumb_args;
    struct drm_mode_map_dumb map_dumb_args;
    struct drm_mode_destroy_dumb destroy_dumb_args;
    struct sched_param sched_prio;

    uint32_t buf_handle;
    uint64_t buf_mmap_offset; 

    fd = open (dri_device.c_str(), O_RDWR);
    if (fd == -1) {
        printf ("Could not open dri device\n");
        return -1;
    }

    resources = drmModeGetResources (fd);
    if (resources == NULL) {
        printf ("Could not get DRM resources\n");
        ret = -1;
        goto done;
    }

    for (int i = 0; i < resources->count_connectors; i++) {
        char name[32];

        connector = drmModeGetConnectorCurrent (fd, resources->connectors[i]);
        if (!connector)
            continue;

        snprintf (name, sizeof(name), "%s-%u", util_lookup_connector_type_name(connector->connector_type),
                connector->connector_type_id);

        if (connector->count_modes) {
            printf ("Active connector: %s; num_encoders: %d\n", name, connector->count_encoders);
            break;
        }    

        drmModeFreeConnector (connector);
        connector = NULL;
    }

    if (connector == NULL) {
        printf ("Could not find active connector\n");
        ret = -1;
        goto done;
    } 

    encoder = drmModeGetEncoder (fd, connector->encoder_id);
    if (encoder == NULL) {
        printf ("Could not get encoder w/ id: %d\n", connector->encoder_id);
        ret = -1;
        goto done;
    }

    crtc = drmModeGetCrtc (fd, encoder->crtc_id);
    if (crtc == NULL) {
        printf ("Could not get CRTC w/ id: %d\n", encoder->crtc_id); 
        ret = -1;
        goto done;
    }

    printf ("CRTC id: %d, w: %d, h: %d\n", crtc->crtc_id, crtc->width, crtc->height);

    for (int i = 0; i < connector->count_modes; i++) {
        resolution = &connector->modes[i];
        if (resolution->type & DRM_MODE_TYPE_PREFERRED) {
            break;
        }
    }

    create_dumb_args.height = resolution->vdisplay;
    create_dumb_args.width = resolution->hdisplay;
    create_dumb_args.bpp = 32;
    buf_bpp = 32;
    create_dumb_args.flags = 0;

    // Gives pitch, size, handle
    ret = ioctl (fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb_args);
    if (ret) {
        printf ("dumb buffer creation failed; err: %d\n", ret);
        ret = -1;
        goto done;
    }

    buf_handle = create_dumb_args.handle;
    buf_size = create_dumb_args.size;
    buf_pitch = create_dumb_args.pitch;
    printf ("DRM FB handle: 0x%x\n", buf_handle);

    ret = drmModeAddFB (fd, resolution->hdisplay, resolution->vdisplay, 24, 32, create_dumb_args.pitch, buf_handle, &buf_id); 
    if (ret) {
        printf ("Could not add FB to DRM");
        ret = -1;
        goto free_dumb;
    }

    printf ("DRM FB id: %d\n", buf_id);

    map_dumb_args.handle = buf_handle;
    map_dumb_args.pad = 0;

    ret = ioctl (fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb_args);

    if (ret) {
        printf ("Could not prepare dumb buf for mmap\n");
        ret = -1;
        goto free_dumb;
    } 
    
    buf_mmap_offset = map_dumb_args.offset;
    buf_mmap = (int *) mmap (NULL, buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf_mmap_offset); 

    if (buf_mmap == NULL) {
        printf ("Could not mmap dumb buf\n");
        ret = -1;
        goto free_dumb;
    }

    xres = buf_pitch / (buf_bpp / 8);
    yres = buf_size / buf_pitch;

    printf ("%d x %d\n", xres, yres);

    ret = setup_input();
    if (ret) {
        goto dumb_unmap;
    }

    cursor.x = xres / 2;
    cursor.y = xres / 2;
    cursor.w = 10; cursor.h = 10;
    cursor.color[0] = 0; cursor.color[1] = 100; cursor.color[2] = 100;
    
    while (1) {
        ret = render();
        if (ret) {
            printf ("drmModeSetCrtc failed w/ ret: %d\n", ret);
            ret = -1;
            goto input_thread_teardown;
        }
    }

    ret = 0;

input_thread_teardown:
    kill = true;
    teardown_input();
dumb_unmap:
   munmap (buf_mmap, buf_size); 
free_dumb:
    destroy_dumb_args.handle = buf_handle; 
    ioctl (fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb_args); 
done:
    close (fd);
    drmModeFreeResources (resources);  
    drmModeFreeConnector (connector);
    drmModeFreeEncoder (encoder);
    drmModeFreeCrtc (crtc);
    return ret;
    
}
