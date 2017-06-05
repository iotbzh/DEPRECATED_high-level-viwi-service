#include "high-can-binding-hat.hpp"
#include <cstddef>
/// Interface between the daemon and the binding
const struct afb_binding_interface *binder_interface;
extern "C"
{
    #include <afb/afb-service-itf.h>
    struct afb_service srvitf;
};
static const struct afb_verb_desc_v1 verbs[]=
{
    { .name= "subscribe",	.session= AFB_SESSION_NONE, .callback= subscribe,	.info= "subscribe to notification of CAN bus messages." },
    { .name= "unsubscribe",	.session= AFB_SESSION_NONE, .callback= unsubscribe,	.info= "unsubscribe a previous subscription." },
    { .name= "get",	        .session= AFB_SESSION_NONE, .callback= get,     	.info= "high can get viwi request." },
};

static const struct afb_binding binding_desc {
    AFB_BINDING_VERSION_1,
    {
        "High level CAN bus service",
        "high-can",
        verbs
    }
};

const struct afb_binding *afbBindingV1Register (const struct afb_binding_interface *itf)
{
    binder_interface = itf;
    NOTICE(binder_interface, "high level afbBindingV1Register");

    return &binding_desc;
}
/// @brief Initialize the binding.
///
/// @param[in] service Structure which represent the Application Framework Binder.
///
/// @return Exit code, zero if success.
int afbBindingV1ServiceInit(struct afb_service service)
{
    srvitf = service;
    //NOTICE(binder_interface, "before afb_daemon_require_api");
    //afb_daemon_require_api(binder_interface->daemon, "low-can", 1);
    NOTICE(binder_interface, "high level binding is initializing");
    initHigh(service);
    NOTICE(binder_interface, "high level binding is initialized and running");
    return 0;
}
