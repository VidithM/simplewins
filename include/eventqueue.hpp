#pragma once

#include <mutex>
#include <vector>
#include <queue>

namespace simplewins {
    enum EventType {
        MOUSE_MOTION,
        MOUSE_LMB_DOWN,
        MOUSE_LMB_UP        
    };
    struct Event {
        enum EventType type;
        std::vector<int> args;
    };
    class EventQueue {
        private:
            std::mutex queue_lock;
            std::queue<Event> events;      

        public:
            EventQueue() = default;
            size_t count ();
            void push_event (Event event);
            Event poll_event ();
    };
}
