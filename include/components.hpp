#pragma once

namespace swins {
    struct cursor {
        int x, y;
        char color[3];
    };
    struct window {
        int x, y, w, h;
        int rank;
    };
}