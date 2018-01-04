#include "high-viwi-binding-hat.hpp"
#include <cstddef>
/// Interface between the daemon and the binding

static int init_service();

static const struct afb_verb_v2 verbs[]=
{
    { .verb= "subscribe",	.callback= subscribe,	.auth = NULL, .info = "subscribe to an ViWi object", .session = 0 },
    { .verb= "unsubscribe",	.callback= unsubscribe,	.auth = NULL, .info = "unsubscribe to a ViWi object", .session = 0 },
    { .verb= "get",	        .callback= get,	.auth = NULL, .info = "Get informations about a resource or element", .session = 0 },
    { .verb= NULL, .callback=NULL, .auth = NULL, .info = NULL, .session = 0 }
};

const struct afb_binding_v2 afbBindingV2 = {
    .api = "high-viwi",
    .specification = "",
    .info = "High CAN ViWi API connected to low-can AGL service",
    .verbs = verbs,
    .preinit = NULL,
    .init = init_service,
    .onevent = onEvent,
    .noconcurrency = 1
};

/// @brief Initialize the binding.
///
/// @return Exit code, zero if success.
int init_service()
{
    AFB_NOTICE("high level binding is initializing");
    afb_daemon_require_api("low-can", 1);
    initHigh();
    AFB_NOTICE("high level binding is initialized and running");
    return 0;
}
