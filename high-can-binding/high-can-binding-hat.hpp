#pragma once
#include <cstddef>
#include <systemd/sd-event.h>
extern "C"
{
    #define AFB_BINDING_VERSION 1
    #include <afb/afb-binding.h>
};

extern "C" struct afb_binding_interface;
extern "C" struct afb_service srvitf;
extern const struct afb_binding_interface *binder_interface;
    void subscribe(struct afb_req request);
    void unsubscribe(struct afb_req request);
    void get(struct afb_req request);
    void initHigh(afb_service service);
    int ticked(sd_event_source *source, unsigned long t, void *data);
