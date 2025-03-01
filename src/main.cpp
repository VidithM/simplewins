#include "simplewins.hpp"
#include "eventqueue.hpp"
#include "input.hpp"

#include <sys/mman.h>
#include <sys/ioctl.h>

const std::string dri_device = "/dev/dri/card0";

extern simplewins::EventQueue events;

static int fd;
static int *buf_mmap = NULL;
static uint32_t buf_id;
static uint64_t buf_size, buf_bpp, buf_pitch;
static int xres, yres;

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
    set_color (150, 75, 0, 255, 30, 40, xres / 4, yres / 4);
    set_color (0, 75, 100, 255, 530, 540, xres / 4, yres / 4);
    set_color (255, 255, 255, 255, 1920, 1080, 10, 10);
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

    memset (buf_mmap, 0, buf_size);
    printf ("%d x %d\n", xres, yres);

    setup_input();

    while (1) {
        ret = render();
        if (ret) {
            printf ("drmModeSetCrtc failed w/ ret: %d\n", ret);
            ret = -1;
            goto dumb_unmap;
        }
    }

    ret = 0;
    
dumb_unmap:
    teardown_input();
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
