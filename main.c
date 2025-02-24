#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <sys/ioctl.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

const char *dri_device = "/dev/dri/card0";


#define ARRAY_SIZE(arr) sizeof(arr) / sizeof(arr[0])

struct type_name {
    unsigned int type;
    const char *name;
};

static const char *util_lookup_type_name(unsigned int type,
                     const struct type_name *table,
                     unsigned int count)
{
    unsigned int i;

    for (i = 0; i < count; i++)
        if (table[i].type == type)
            return table[i].name;
    
    return NULL;
}

static const struct type_name connector_type_names[] = {
    { DRM_MODE_CONNECTOR_Unknown, "unknown" },
    { DRM_MODE_CONNECTOR_VGA, "VGA" },
    { DRM_MODE_CONNECTOR_DVII, "DVI-I" },
    { DRM_MODE_CONNECTOR_DVID, "DVI-D" },
    { DRM_MODE_CONNECTOR_DVIA, "DVI-A" },
    { DRM_MODE_CONNECTOR_Composite, "composite" },
    { DRM_MODE_CONNECTOR_SVIDEO, "s-video" },
    { DRM_MODE_CONNECTOR_LVDS, "LVDS" },
    { DRM_MODE_CONNECTOR_Component, "component" },
    { DRM_MODE_CONNECTOR_9PinDIN, "9-pin DIN" },
    { DRM_MODE_CONNECTOR_DisplayPort, "DP" },
    { DRM_MODE_CONNECTOR_HDMIA, "HDMI-A" },
    { DRM_MODE_CONNECTOR_HDMIB, "HDMI-B" },
    { DRM_MODE_CONNECTOR_TV, "TV" },
    { DRM_MODE_CONNECTOR_eDP, "eDP" },
    { DRM_MODE_CONNECTOR_VIRTUAL, "Virtual" },
    { DRM_MODE_CONNECTOR_DSI, "DSI" },
    { DRM_MODE_CONNECTOR_DPI, "DPI" },
};

const char *util_lookup_connector_type_name(unsigned int type)
{
    return util_lookup_type_name(type, connector_type_names,
                     ARRAY_SIZE(connector_type_names));
}

void set_color (int *buf, uint64_t sz, int r, int g, int b, int a) {
    memset (buf, 0, sizeof(buf));
    sz /= sizeof(int);
    for (uint64_t i = 0; i < sz; i++) {
        buf[i] ^= (a << 24);
        buf[i] ^= (r << 16);
        buf[i] ^= (g << 8);
        buf[i] ^= b; 
    }
}

int main() {
    int fd, ret;
    drmModeResPtr resources = NULL;
    drmModeConnectorPtr connector = NULL;
    drmModeEncoderPtr encoder = NULL;
    drmModeCrtcPtr crtc = NULL;
    drmModeModeInfoPtr resolution = NULL;

    struct drm_mode_create_dumb create_dumb_args;
    struct drm_mode_map_dumb map_dumb_args;
    struct drm_mode_destroy_dumb destroy_dumb_args;

    int buf_handle, buf_id;
    uint64_t buf_size;
    uint64_t buf_mmap_offset; 
    int *buf_mmap = NULL;

    fd = open (dri_device, O_RDWR);

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
    set_color (buf_mmap, buf_size, 150, 75, 0, 255);
    ret = drmModeSetCrtc (fd, crtc->crtc_id, buf_id, 0, 0, &connector->connector_id, 1, resolution); 
    if (ret) {
        printf ("drmModeSetCrtc failed w/ ret: %d\n", ret);
        ret = -1;
        goto dumb_unmap;
    }
    ret = 0;
    while (1) {}

dumb_unmap:
    munmap (buf_mmap, buf_size); 
free_dumb:
    destroy_dumb_args.handle = buf_handle; 
    ioctl (fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb_args); 
done:
    drmModeFreeResources (resources);  
    drmModeFreeConnector (connector);
    drmModeFreeEncoder (encoder);
    drmModeFreeCrtc (crtc);
    return ret;
    
}
