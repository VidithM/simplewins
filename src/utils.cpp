#include "utils.hpp"

/*
 * The following are used internally within libdrm and are not exposed to users; they are
 * used here only for debug  
 */
#define ARRAY_SIZE(arr) sizeof(arr) / sizeof(arr[0])

struct type_name {
    unsigned int type;
    const char *name;
};

const char *util_lookup_type_name(unsigned int type,
                     const struct type_name *table,
                     unsigned int count)
{
    unsigned int i;

    for (i = 0; i < count; i++)
        if (table[i].type == type)
            return table[i].name;
    
    return NULL;
}

const struct type_name connector_type_names[] = {
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