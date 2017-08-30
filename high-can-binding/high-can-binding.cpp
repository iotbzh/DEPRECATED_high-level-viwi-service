/*
 * Copyright (C) 2015, 2016 "IoT.bzh"
 * Author "Romain Forlot" <romain.forlot@iot.bzh>
 * Author "Loic Collignon" <loic.collignon@iot.bzh>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "high-can-binding-hat.hpp"
#include <json-c/json.h>
#include "high.hpp"
High high;

/// @brief callback for receiving message from low binding. Treatment itself is made in High class.
void onEvent(const char *event, json_object *object)
{
    high.treatMessage(object);
}
/// @brief entry point for client subscription request. Treatment itself is made in High class.
void subscribe(afb_req request)
{
    if(high.subscribe(request))
        afb_req_success(request, NULL, NULL);
    else
        afb_req_fail(request, "error", NULL);
}

/// @brief entry point for client un-subscription request. Treatment itself is made in High class.
void unsubscribe(afb_req request)
{
    if(high.unsubscribe(request))
        afb_req_success(request, NULL, NULL);
    else
        afb_req_fail(request, "error", NULL);
}

/// @brief verb that loads JSON configuration (old high.json file now)
void load(afb_req request)
{
    json_object* args = afb_req_json(request);
    const char* confd;

    wrap_json_unpack(args, "{s:s}", "path", &confd);
    high.parseConfigAndSubscribe(confd);
}

/// @brief entry point for get requests. Treatment itself is made in High class.
void get(afb_req request)
{
    json_object *jobj;
    if(high.get(request, &jobj))
    {
        afb_req_success(request, jobj, NULL);
    } else {
        afb_req_fail(request, "error", NULL);
    }
}

/// @brief entry point for systemD timers. Treatment itself is made in High class.
/// @param[in] source: systemD timer, t: time of tick, data: interval (ms).
int ticked(sd_event_source *source, uint64_t t, void* data)
{
    high.tick(source, t, data);
    return 0;
}

/// @brief Initialize the binding.
///
/// @param[in] service Structure which represent the Application Framework Binder.
void initHigh()
{
}


