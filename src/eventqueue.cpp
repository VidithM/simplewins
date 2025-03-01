#include "eventqueue.hpp"
#include <exception>

size_t simplewins::EventQueue::count () {
    return events.size();
}

void simplewins::EventQueue::push_event (simplewins::Event event) {
    queue_lock.lock();
    events.push (event);
    queue_lock.unlock();
}

simplewins::Event simplewins::EventQueue::poll_event () {
    Event ret;
    queue_lock.lock();
    if (!this->count()) {
        throw std::logic_error ("Polling empty event queue");
    }
    ret = events.front();
    events.pop();
    queue_lock.unlock();
    return ret; 
}
