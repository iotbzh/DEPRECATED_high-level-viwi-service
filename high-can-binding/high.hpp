#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <json-c/json.h>
#include <systemd/sd-event.h>
extern "C"
{
    #include <afb/afb-binding.h>
};

struct TimedEvent {
    int interval;
    afb_event event;
    std::string name;
    std::string eventName;
};
struct Property {
    /**
     * alternatively, instead of a value per type, we could use boost::any, or in c++17 variant.
     */
    std::string type;
    std::string description;
    std::string lowMessageName;
    int interval;
    bool value_bool;
    std::string value_string;
    double value_double;
    int value_int;
};

class High
{
public:
    High();
    void treatMessage(json_object *message);
    bool subscribe(afb_req request);
    bool unsubscribe(afb_req request);
    bool get(afb_req request, json_object **json);
    void tick(sd_event_source *source, const long &now, void *interv);
    void startTimer(const int &t);
    ~High();
    void parseConfigAndSubscribe(afb_service service);
    static bool startsWith(const std::string &s, const std::string &val);
    static void callBackFromSubscribe(void *handle, int iserror, json_object *result);
private:
    std::map<std::string, afb_event> events;
    std::map<int, std::vector<TimedEvent>> timedEvents;
    std::map<std::string, std::map<std::string, Property>> registeredObjects;
    std::map<std::string, std::set<std::string>> lowMessagesToObjects;
    std::set<int> timers;
    std::string generateId() const;
    json_object *generateJson(const std::string &messageObject, std::vector<std::__cxx11::string> *fields = NULL);
};
