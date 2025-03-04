#include "eventqueue.hpp"
#include <exception>

size_t swins::EventQueue::count () {
    return events.size();
}

void swins::EventQueue::push_event (swins::Event event) {
    queue_lock.lock();
    events.push (event);
    queue_lock.unlock();
}

swins::Event swins::EventQueue::poll_event () {
    Event ret;
    queue_lock.lock();
    if (!events.size()) {
        throw std::logic_error ("Polling empty event queue");
    }
    ret = events.front();
    events.pop();
    queue_lock.unlock();
    return ret; 
}
