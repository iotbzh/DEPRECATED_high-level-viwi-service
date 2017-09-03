
static const char _afb_description_v2_high_viwi[] =
    "{\"openapi\":\"3.0.0\",\"$schema\":\"http:iot.bzh/download/openapi/schem"
    "a-3.0/default-schema.json\",\"info\":{\"description\":\"\",\"title\":\"h"
    "igh-level-viwi-service\",\"version\":\"4.0\",\"x-binding-c-generator\":{"
    "\"api\":\"high-viwi\",\"version\":2,\"prefix\":\"\",\"postfix\":\"\",\"s"
    "tart\":null,\"onevent\":\"onEvent\",\"init\":\"init_service\",\"scope\":"
    "\"\",\"private\":false}},\"servers\":[{\"url\":\"ws://{host}:{port}/api/"
    "monitor\",\"description\":\"High ViWi API connected to low level AGL ser"
    "vices\",\"variables\":{\"host\":{\"default\":\"localhost\"},\"port\":{\""
    "default\":\"1234\"}},\"x-afb-events\":[{\"$ref\":\"#/components/schemas/"
    "afb-event\"}]}],\"components\":{\"schemas\":{\"afb-reply\":{\"$ref\":\"#"
    "/components/schemas/afb-reply-v2\"},\"afb-event\":{\"$ref\":\"#/componen"
    "ts/schemas/afb-event-v2\"},\"afb-reply-v2\":{\"title\":\"Generic respons"
    "e.\",\"type\":\"object\",\"required\":[\"jtype\",\"request\"],\"properti"
    "es\":{\"jtype\":{\"type\":\"string\",\"const\":\"afb-reply\"},\"request\""
    ":{\"type\":\"object\",\"required\":[\"status\"],\"properties\":{\"status"
    "\":{\"type\":\"string\"},\"info\":{\"type\":\"string\"},\"token\":{\"typ"
    "e\":\"string\"},\"uuid\":{\"type\":\"string\"},\"reqid\":{\"type\":\"str"
    "ing\"}}},\"response\":{\"type\":\"object\"}}},\"afb-event-v2\":{\"type\""
    ":\"object\",\"required\":[\"jtype\",\"event\"],\"properties\":{\"jtype\""
    ":{\"type\":\"string\",\"const\":\"afb-event\"},\"event\":{\"type\":\"str"
    "ing\"},\"data\":{\"type\":\"object\"}}}},\"x-permissions\":{},\"response"
    "s\":{\"200\":{\"description\":\"A complex object array response\",\"cont"
    "ent\":{\"application/json\":{\"schema\":{\"$ref\":\"#/components/schemas"
    "/afb-reply\"}}}}}},\"paths\":{\"/subscribe\":{\"description\":\"Subscrib"
    "e to a ViWi object\",\"parameters\":[{\"in\":\"query\",\"name\":\"event\""
    ",\"required\":false,\"schema\":{\"type\":\"string\"}}],\"responses\":{\""
    "200\":{\"$ref\":\"#/components/responses/200\"}}},\"/unsubscribe\":{\"de"
    "scription\":\"Unsubscribe previously suscribed ViWi objects.\",\"paramet"
    "ers\":[{\"in\":\"query\",\"name\":\"event\",\"required\":false,\"schema\""
    ":{\"type\":\"string\"}}],\"responses\":{\"200\":{\"$ref\":\"#/components"
    "/responses/200\"}}},\"/get\":{\"description\":\"Get informations about a"
    " resource or element\",\"responses\":{\"200\":{\"$ref\":\"#/components/r"
    "esponses/200\"}}},\"/load\":{\"description\":\"Load config file in direc"
    "tory passed as argument\",\"parameters\":[{\"in\":\"query\",\"name\":\"p"
    "ath\",\"required\":true,\"schema\":{\"type\":\"string\"}}],\"responses\""
    ":{\"200\":{\"$ref\":\"#/components/responses/200\"}}}}}"
;

 void subscribe(struct afb_req req);
 void unsubscribe(struct afb_req req);
 void get(struct afb_req req);
 void load(struct afb_req req);

static const struct afb_verb_v2 _afb_verbs_v2_high_viwi[] = {
    {
        .verb = "subscribe",
        .callback = subscribe,
        .auth = NULL,
        .info = "Subscribe to a ViWi object",
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "unsubscribe",
        .callback = unsubscribe,
        .auth = NULL,
        .info = "Unsubscribe previously suscribed ViWi objects.",
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "get",
        .callback = get,
        .auth = NULL,
        .info = "Get informations about a resource or element",
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "load",
        .callback = load,
        .auth = NULL,
        .info = "Load config file in directory passed as argument",
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = NULL,
        .callback = NULL,
        .auth = NULL,
        .info = NULL,
        .session = 0
	}
};

const struct afb_binding_v2 afbBindingV2 = {
    .api = "high-viwi",
    .specification = _afb_description_v2_high_viwi,
    .info = "",
    .verbs = _afb_verbs_v2_high_viwi,
    .preinit = NULL,
    .init = init_service,
    .onevent = onEvent,
    .noconcurrency = 0
};

