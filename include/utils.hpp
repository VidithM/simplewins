#pragma once

#include <xf86drm.h>
#include <xf86drmMode.h>

const char *util_lookup_type_name(unsigned int type,
    const struct type_name *table,
    unsigned int count);

const char *util_lookup_connector_type_name(unsigned int type);