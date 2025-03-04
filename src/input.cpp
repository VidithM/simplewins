#include "simplewins.hpp"
#include "eventqueue.hpp"
#include "utils/timer.hpp"

#include <poll.h>
#include <pthread.h>
#include <libudev.h>
#include <libinput.h>

swins::EventQueue input_queue;
extern bool kill;
extern swins::utils::Timer timer;

static pthread_t event_thread;
static udev *ud;
static libinput *li;
static libinput_event *li_event;


static void* handle_input(void *args) {
    int ret;
    while (1) {
        pollfd fd = {
            .fd = libinput_get_fd (li),
            .events = POLLIN,
            .revents = 0
        };
        poll (&fd, 1, 1000); 
        if (kill) {
            break;
        }
        libinput_dispatch (li);
        li_event = libinput_get_event (li);
        if (li_event == NULL) {
            continue;
        }
        enum libinput_event_type event_type = libinput_event_get_type (li_event);
        switch (event_type) {
            case LIBINPUT_EVENT_POINTER_MOTION:
                {
                    libinput_event_pointer *pointer_event = libinput_event_get_pointer_event (li_event);
                    double dx = libinput_event_pointer_get_dx (pointer_event);
                    double dy = libinput_event_pointer_get_dy (pointer_event);
                    swins::Event motion = {swins::MOUSE_MOTION, std::vector<int> {(int) dx, (int) dy}};
                    input_queue.push_event (motion);
                }
                break;
            case LIBINPUT_EVENT_POINTER_BUTTON:
                break;
            default:
                break;
        }
        libinput_event_destroy (li_event);
    }
    return NULL;
}

/*
 * libinput setup code from libinput documentation (https://wayland.freedesktop.org/libinput/doc/latest/api)
 */

static int open_restricted (const char *path, int flags, void *user_data) {
    int fd = open (path, flags);
    return fd < 0 ? -errno : fd;
}
    
static void close_restricted (int fd, void *user_data) {
    close (fd);
}    

const static libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted
};

int setup_input() {
    int ret;

    ud = udev_new();
    if (ud == NULL) {
        return -1;
    }

    li = libinput_udev_create_context (&interface, NULL, ud); 
    if (li == NULL) {
        ret = -1;
        goto ud_unref;
    }

    ret = libinput_udev_assign_seat (li, "seat0");
    if (ret == -1) {
        goto done;
    }
    
    ret = pthread_create (&event_thread, NULL, handle_input, NULL);
    if (ret) {
        goto done;
    }
    return 0;
   
done:
    libinput_unref (li);
ud_unref:
    udev_unref (ud);

    return ret;
}

void teardown_input() {
    assert (li != NULL);
    assert (ud != NULL);
    pthread_join (event_thread, NULL);

    libinput_unref (li);
    udev_unref (ud);
}
