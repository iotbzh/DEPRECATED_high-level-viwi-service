#include "high-can-binding-hat.hpp"
#include <cstddef>
/// Interface between the daemon and the binding

static int init_service();

static const struct afb_verb_v2 verbs[]=
{
    { .verb= "subscribe",	.callback= subscribe,	.auth = NULL, .session = 0 },
    { .verb= "unsubscribe",	.callback= unsubscribe,	.auth = NULL, .session = 0 },
    { .verb= "get",	        .callback= get,	.auth = NULL, .session = 0 },
    { .verb= NULL, .callback=NULL, .auth = NULL, .session = 0 }
};

const struct afb_binding_v2 afbBindingV2 = {
    .api = "high-can",
    .specification = "",
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
    NOTICE("high level binding is initializing");
    afb_daemon_require_api("low-can", 1);
    initHigh();
    NOTICE("high level binding is initialized and running");
    return 0;
}
