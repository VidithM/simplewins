#include "simplewins.hpp"
#include "components.hpp"

#if 0
class box {
    public:
        int x, y, w, h;
        bool intersect (box &o);
};    
#endif 

bool swins::box::intersect (swins::box &o) {
    int mn_x = std::min(x, o.x);
    int mx_x = std::max(x + w - 1, o.x + o.w - 1);
    int mn_y = std::min(y, o.y);
    int mx_y = std::max(y + h - 1, o.y + o.h - 1);
    
    int tot_w = w + o.w;
    int tot_h = h + o.h;
    
    return ((mx_x - mn_x + 1 < tot_w) && (mx_y - mn_y + 1 < tot_h));
}
