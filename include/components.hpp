#pragma once

namespace swins {
    class box {
        public:
            int x, y, w, h;
            bool intersect (box &o);
    };    
    struct cursor : box {
        char color[3];
    };
    struct window : box {
        int rank;
        char color[3];
    };
}
