#include "simplewins.hpp"
#include "eventqueue.hpp"

#include <poll.h>
#include <pthread.h>
#include <libudev.h>
#include <libinput.h>

simplewins::EventQueue input_queue;
extern bool kill;

static pthread_t event_thread;
static udev *ud;
static libinput *li;
static libinput_event *li_event;


static void* handle_input(void *args) {
    int ret;
    int cnt = 0;
    while (1) {
        pollfd fd = {
            .fd = libinput_get_fd (li),
            .events = POLLIN,
            .revents = 0
        };
        poll (&fd, 1, 100); 
        printf ("poll event/timeout\n");
        if (kill) {
            break;
        }
        libinput_dispatch (li);
        li_event = libinput_get_event (li);
        if (li_event == NULL) {
            continue;
        }
        cnt++;
        printf ("got event %d\n", cnt);
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
