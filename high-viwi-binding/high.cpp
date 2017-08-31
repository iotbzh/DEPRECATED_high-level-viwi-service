/**
TYPICAL COMMANDS:
high-can start
high-can get {"name":"/car/doors/"}
high-can get {"name":"/car/doors/6078d4-23b89b-8faaa9-8bd0f3"}
high-can get {"name":"/car/doors/","fields":["isDoorOpen"]}
high-can subscribe {"name":"/car/doors/6078d4-23b89b-8faaa9-8bd0f3"} note: fields parameter on subscribe is not implemented yet
high-can unsubscribe {"name":"/car/doors/6078d4-23b89b-8faaa9-8bd0f3"}
high-can subscribe {"name":"/car/doors/"}
high-can subscribe {"name":"/car/doors/6078d4-23b89b-8faaa9-8bd0f3","interval":5000}
high-can subscribe {"name":"/car/doors/","interval":5000}
high-can unsubscribe {"name":"/car/doors/","interval":5000}
*/

#include <json-c/json.h>
#include <time.h>
#include <algorithm>
#include <sstream>
#include <iterator>

#include "filescan-utils.h"
#include "wrap-json.h"
#include "high.hpp"
#include "high-viwi-binding.hpp"

/// @brief Split a std::string in several string, based on a delimeter
///
/// @param[in] string: the string to be splitted, delim: the delimeter to use for splitting.
///
/// @return std::vector<std::string> : a vector containing each individual string after the split.
using namespace std;
template<typename Out>
void split(const std::string &s, char delim, Out result)
{
	std::stringstream ss;
	ss.str(s);
	std::string item;
	while (std::getline(ss, item, delim))
	{
		*(result++) = item;
	}
}
std::vector<std::string> split(const std::string &s, char delim)
{
	std::vector<std::string> elems;
	split(s, delim, std::back_inserter(elems));
	return elems;
}

/// @brief Main high binding class: maintains resources status, subcriptions and timers
High::High()
{
}

/// @brief Simply fill registeredObjects from loaded configuration.
///
/// @param[in] uri - ViWi xObject uri
/// @param[in] properties - ViWi xObject properties retrieved
void High::registerObjects(const std::string& uri, const std::map<std::string, Property>& properties)
{
	registeredObjects[uri] = properties;
}

/// @brief Handle definition JSON object parsing from provided
/// configuration.
///
/// @param[in] definitions - pointer to the json_object that hold definitions
///  JSON object to be parsed.
std::map<std::string, std::map<std::string, Property>> High::loadDefinitions(json_object* definitions) const
{
	std::map<std::string, std::map<std::string, Property>> properties;

	int arraylen1 = json_object_array_length(definitions);
	for(int n = 0; n < arraylen1; ++n)
	{
		json_object* obj, *jvalue, *jproperties;
		obj = json_object_array_get_idx(definitions, n);
		json_object_object_get_ex(obj, "name", &jvalue);
		const std::string name = json_object_get_string(jvalue);
		if(! wrap_json_unpack(obj, "{s:o}", "properties", &jproperties))
		{
			std::map<std::string, Property> props;
			json_object_object_foreach(jproperties, key, val)
			{
				Property p;
				json_object_object_get_ex(val, "type", &jvalue);
				p.type = json_object_get_string(jvalue);
				json_object_object_get_ex(val, "description", &jvalue);
				p.description = json_object_get_string(jvalue);
				props[key] = p;
			}
			properties[name] = props;
		}
	}

	return properties;
}

/// @brief Handle resources JSON object parsing from provided
/// configuration.
///
/// @param[in] resources - pointer to the json_object that hold
///  resources, previously defined, JSON object to be parsed.
void High::loadResources(json_object* resources, const std::map<std::string, std::map<std::string, Property>>& properties)
{
	json_object* jarray1;
	json_object_object_get_ex(resources, "resources", &jarray1);
	int arraylen1 = json_object_array_length(jarray1);
	std::map<std::string, int> toSubscribe;
	for(int n = 0; n < arraylen1; ++n)
	{
		json_object* obj, *jvalue, *jarray2;
		obj = json_object_array_get_idx(jarray1, n);
		json_object_object_get_ex(obj, "name", &jvalue);
		const std::string name = json_object_get_string(jvalue);
		json_object_object_get_ex(obj, "values", &jarray2);
		const int arraylen2 = json_object_array_length(jarray2);
		for(int i = 0; i < arraylen2; ++i)
		{
			const std::string id = generateId();
			const std::string uri = name + id;
			jvalue = json_object_array_get_idx(jarray2, i);
			if(properties.find(name) == properties.end())
			{
				AFB_WARNING("Unable to find name %s in properties", name.c_str());
				continue;
			}
			const std::map<std::string, Property> props = properties[name];
			std::map<std::string, Property> localProps; //note that local props can have less members than defined.
			localProps["id"] = props.at("id");
			localProps["name"] = props.at("name");
			localProps["uri"] = props.at("uri");
			localProps["id"].value_string = std::string(id);
			localProps["uri"].value_string = std::string(uri);
			json_object_object_foreach(jvalue, key, val)
			{
				const std::string value = json_object_get_string(val);
				if(props.find(key) == props.end())
				{
					AFB_WARNING("Unable to find key %s in properties", value.c_str());
					continue;
				}
				Property prop = props.at(key);
				if(startsWith(value, "${"))
				{
					const std::string canMessage = value.substr(2, value.size() - 1);
					const std::vector<std::string> params = split(canMessage, ',');
					if(params.size() != 2)
					{
						AFB_WARNING("Invalid CAN message definition %s", value.c_str());
						continue;
					}
					prop.lowMessageName = params.at(0);
					prop.interval = stoi(params.at(1));
					if(toSubscribe.find(prop.lowMessageName) != toSubscribe.end())
					{
						if(toSubscribe.at(prop.lowMessageName) > prop.interval)
							toSubscribe[prop.lowMessageName] = prop.interval;
					} else {
						toSubscribe[prop.lowMessageName] = prop.interval;
					}
					switch (prop.type)
					{
						case "string":
							prop.value_string = std::string("nul");
							break;
						case "boolean":
							prop.value_bool = false;
							break;
						case "double":
							prop.value_double = 0.0;
							break;
						case "int":
							prop.value_int = 0;
							break;
						default:
							AFB_ERROR("ERROR 2! unexpected type in parseConfig %s %s", prop.description.c_str(), prop.type.c_str());
							break;
					}
				} else {
					prop.value_string= std::string(value);
				}
				localProps[key] = prop;
			}

			registerObjects(uri, localProps);
}

/// @brief Reads the json configuration and generates accordingly the resources container. An UID is generated for each resource.
///		Makes necessary subscriptions to low-level, eventually with a frequency.
///
/// @param[in] confd - path to configuration directory which holds the binding configuration to load
///
void High::parseConfigAndSubscribe(const std::string& confd)
{
	char* filename;
	char* fullpath;
	std::vector<std::string> conf_files_path;

	// Grab all config files with 'viwi' in their names in the path provided
	struct json_object* conf_filesJ = ScanForConfig(confd.c_str(), CTL_SCAN_FLAT, "viwi", "json");
	if (!conf_filesJ || json_object_array_length(conf_filesJ) == 0)
	{
		AFB_ERROR("No JSON config files found in %s", confd.c_str());
		return;
	}

	for(int i=0; i < json_object_array_length(conf_filesJ); i++)
	{
		json_object *entryJ=json_object_array_get_idx(conf_filesJ, i);

		int err = wrap_json_unpack (entryJ, "{s:s, s:s !}", "fullpath",  &fullpath,"filename", &filename);
		if (err) {
			AFB_ERROR ("OOOPs invalid config file path = %s", json_object_get_string(entryJ));
			return;
		}
		std::string filepath = fullpath;
		filepath += filename;
		conf_files_path.push_back(filepath);
	}

	json_object *jarray1, *jarray2;
	int i = 0;
	std::map<std::string, std::map<std::string, Property>> properties;
	while(properties.empty() || i < conf_files_path.size())
	{
		json_object *config = json_object_from_file(conf_files_path[i]);
		if(! wrap_json_unpack(config, "{s:o}", "definition", &jarray1))
		{
			properties = loadDefinitions(jarray1);
			conf_files_path.erase(i);
		}
		i++;
	}

	// Search for resources JSON node to load them if
	// definition has been found.
	if(! properties.empty())
	{
		for(const std::string& filepath: conf_files_path)
		{
			if(! wrap_json_unpack(config, "{s:o}", "resources", &jarray2))
			{
				loadResources(jarray2, properties);
			}
		}
	}
			for(const auto &p : localProps)
			{
				if(p.second.lowMessageName.size() > 0)
				{
					std::set<std::string> objectList;
					if(lowMessagesToObjects.find(p.second.lowMessageName) != lowMessagesToObjects.end())
						objectList = lowMessagesToObjects.at(p.second.lowMessageName);
					objectList.insert(uri);
					lowMessagesToObjects[p.second.lowMessageName] = objectList;
				}
			}
		}
	}
	for(const auto &p : toSubscribe)
	{
		json_object *jobj = json_object_new_object();
		json_object_object_add(jobj,"event", json_object_new_string(p.first.c_str()));
		if(p.second > 0)
		{
			json_object *filter = json_object_new_object();
			json_object_object_add(filter, "frequency", json_object_new_double(1000.0 / (double)p.second));
			json_object_object_add(jobj, "filter", filter);
		}
		json_object *dummy;
		const std::string js = json_object_get_string(jobj);
		if(afb_service_call_sync("low-can", "subscribe", jobj, &dummy) < 0)
			AFB_ERROR("high-can subscription to low-can FAILED %s", js.c_str());
		else
			AFB_NOTICE("high-can subscribed to low-can %s", js.c_str());
		json_object_put(dummy);
	}
	json_object_put(config);
	AFB_NOTICE("configuration loaded");
}

/// @brief Create and start a systemD timer. Only one timer is created per frequency.
///
/// @param[in] t: interval in ms.
///
void High::startTimer(const int &t)
{
	if(timers.find(t) != timers.end())
		return;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	sd_event_add_time(afb_daemon_get_event_loop(), NULL, CLOCK_MONOTONIC, (ts.tv_sec + 1) * 1000000, 0, &ticked, new int(t));
}
High::~High()
{
	timers.clear();
}

/// @brief callback called after subscription to low-level binding.
///
void High::callBackFromSubscribe(void *handle, int iserror, json_object *result)
{
	AFB_NOTICE("high level callBackFromSubscribe method called %s", json_object_get_string(result));
}

/// @brief Entry point for all timer events. Treats all requests linked to the specific timer frequency.
///		Restarts the timer, or cancels it if no requests are anymore linked to it.
///
/// @param[in] source: systemD timer, now: tick timestamp, interv: specific timer interval in ms.
///
void High::tick(sd_event_source *source, const long &now, void *interv)
{
	const int interval = *(int*)interv;
	AFB_NOTICE("tick! %d %ld", interval, now);
	bool hasEvents = false;
	if(timedEvents.find(interval) != timedEvents.end())
	{
		std::vector<TimedEvent> evts  = timedEvents[interval];
		for(int i = (int)evts.size() - 1; i >= 0; --i)
		{
			const TimedEvent e = evts.at(i);
			std::map<std::string, json_object*> jsons;
			for(const auto &pp : registeredObjects)
			{
				if(startsWith(pp.first, e.name))
				{
					jsons[pp.first] = generateJson(pp.first);
				}
			}
			json_object *j = json_object_new_object();
			if(jsons.size() == 1)
			{
				j = jsons[0];
			} else if(jsons.size() > 1)
			{
				for(const auto &pp : jsons)
					json_object_object_add(j, pp.first.c_str(), pp.second);
			}
			const int nbSubscribers = afb_event_push(e.event, j);
			if(nbSubscribers == 0)
			{
				afb_event_drop(e.event);
				evts.erase(evts.begin() + i);
				timedEvents[interval] = evts;
			}
			//AFB_NOTICE("%s event pushed to %d subscribers", e.eventName.c_str(), nbSubscribers);
		}
		if(evts.size() > 0)
			hasEvents = true;
	}
	if(hasEvents)
	{
		sd_event_source_set_time(source, now + interval * 1000);
		sd_event_source_set_enabled(source, SD_EVENT_ON);
	} else {
		//AFB_NOTICE("timer removed %d", interval);
		delete (int*)interv;
		if(timers.find(interval) != timers.end())
		{
			timers.erase(interval);
		}
		sd_event_source_unref(source);
	}
}

/// @brief Entry point for low-binding events. Updates all resources linked to this event and eventually
///		sends back events to subscribers, if any.
///
/// @param[in] message: json low-level message.
///
void High::treatMessage(json_object *message)
{
	json_object *nameJson, *jvalue;
	json_object_object_get_ex(message, "name", &nameJson);
	json_object_object_get_ex(message, "value", &jvalue);
	const std::string messageName(json_object_get_string(nameJson));
	if(lowMessagesToObjects.find(messageName) == lowMessagesToObjects.end())
	{
		AFB_ERROR("message not linked to any object %s", json_object_get_string(message));
		return;
	}
//	AFB_NOTICE("message received %s", json_object_get_string(message));
	const std::set<std::string> objects = lowMessagesToObjects.at(messageName);
	std::vector<std::string> candidateMessages;
	for(const std::string &uri : objects)
	{
		std::map<std::string, Property> properties = registeredObjects.at(uri);
		std::string foundProperty;
		for(const auto &p : properties)
		{
			if(p.second.lowMessageName != messageName)
				continue;
			foundProperty = p.first;
			candidateMessages.push_back(uri);
			break;
		}

		if(foundProperty.size() > 0)
		{
			Property property = properties.at(foundProperty);
			if(property.type == "boolean")
				property.value_bool = json_object_get_boolean(jvalue);
			else if(property.type == "string")
				property.value_string = std::string(json_object_get_string(jvalue));
			else if(property.type == "double")
				property.value_double = json_object_get_double(jvalue);
			else if(property.type == "int")
				property.value_int = json_object_get_int(jvalue);
			else
				AFB_ERROR("ERROR 3! unexpected type %s %s", property.description.c_str(), property.type.c_str());
			properties[foundProperty] = property;
			registeredObjects[uri] = properties;
		}
	}
/** at that point all objects have been updated. Now lets see if we should also send back messages to our subscribers. */
	for(const std::string &m : candidateMessages)
	{
		for(const auto &p : events)
		{
			if(startsWith(m, p.first))
			{
				std::map<std::string, json_object*> jsons;
				for(const auto &pp : registeredObjects)
				{
					if(startsWith(pp.first, p.first))
					{
						jsons[pp.first] = generateJson(pp.first);
					}
				}
				json_object *j = json_object_new_object();
				if(jsons.size() == 1)
				{
					j = jsons[0];
				} else if(jsons.size() > 1)
				{
					for(const auto &pp : jsons)
						json_object_object_add(j, pp.first.c_str(), pp.second);
				}
				const int nbSubscribers = afb_event_push(p.second, j);
				if(nbSubscribers == 0)
				{
					afb_event_drop(p.second);
					events.erase(p.first);
				}
			}
		}
	}
}

/// @brief Generate json message for a resource, in ViWi format. Based on resource definition extracted from json
///		configuration file. If vector "fields" is not empty, will included only properties present in the vector.
///
/// @param[in] messageObject: resource's name, fields: list of properties to be included (NULL = all).
///
/// @return jsonObject containing the resource status for this resource's name.
json_object *High::generateJson(const std::string &messageObject, std::vector<std::string> *fields)
{
	json_object *json = json_object_new_object();
	const std::map<std::string, Property> props = registeredObjects.at(messageObject);
	for(const auto &p : props)
	{
		if(fields && fields->size() > 0 && p.first != "id" && p.first != "uri" && p.first != "name")
		{
			if(std::find(fields->begin(), fields->end(), p.first) == fields->end())
				continue;
		}
		if(p.second.type == "string")
		{
			const std::string value = p.second.value_string;
			json_object_object_add(json, p.first.c_str(), json_object_new_string(value.c_str()));
		} else if(p.second.type == "boolean")
		{
			const bool value = p.second.value_bool;
			json_object_object_add(json, p.first.c_str(), json_object_new_boolean(value));
		} else if(p.second.type == "double")
		{
			const double value = p.second.value_double;
			json_object_object_add(json, p.first.c_str(), json_object_new_double(value));
		} else if(p.second.type == "int")
		{
			const int value = p.second.value_int;
			json_object_object_add(json, p.first.c_str(), json_object_new_int(value));
		} else {
			AFB_ERROR("ERROR 1! unexpected type %s %s %s", p.first.c_str(), p.second.description.c_str(), p.second.type.c_str());
		}
	}
	return json;
}

/// @brief Generates a random UID
///
/// @return string containing the generated UID.
std::string High::generateId() const
{
	char id[50];
	sprintf(id, "%x-%x-%x-%x", (rand()%(int)1e7 + 1), (rand()%(int)1e7 + 1), (rand()%(int)1e7 + 1), (rand()%(int)1e7 + 1));
	return std::string(id);
}

/// @brief Entry point for subscribing to a resource. Can optionnally include a time interval in ms.
///
/// @param[in] request: afb-request containing a json request, for instance {"name":"/car/demoboard/", "interval":1000}
///
/// @return true if subscribed succeeded, false otherwise.
bool High::subscribe(afb_req request)
{
	/** /car/doors/3901a278-ba17-44d6-9aef-f7ca67c04840 */
	bool ok = false;
	json_object *args = afb_req_json(request);
	json_object *nameJson = NULL;
	json_object *intervalJson = NULL;
	if(!json_object_object_get_ex(args, "name", &nameJson))
		return false;
	json_object_object_get_ex(args, "interval", &intervalJson);
	int ms = -1;
	if(intervalJson)
		ms = json_object_get_int(intervalJson);
	std::string message(json_object_get_string(nameJson));
	if(message.size() == 0)
		return ok;
	for(const auto &p : registeredObjects)
	{
		if(startsWith(p.first, message))
		{
			afb_event event;
			if(ms <= 0)
			{
				if(events.find(message) != events.end())
				{
					event = events.at(message);
				} else {
					event = afb_daemon_make_event(p.first.c_str());
					events[message] = event;
				}
				if (afb_event_is_valid(event) && afb_req_subscribe(request, event) == 0)
				{
					ok = true;
				}
			} else {
				std::vector<TimedEvent> evts;
				if(timedEvents.find(ms) != timedEvents.end())
					evts = timedEvents.at(ms);
				afb_event afbEvent;
				bool found = false;
				for(const auto & e : evts)
				{
					if(e.name == message)
					{
						afbEvent = e.event;
						found = true;
						break;
					}
				}
				if(!found)
				{
					char ext[20];
					sprintf(ext, "_%d", ms);
					std::string messageName = message + std::string(ext);
					//AFB_NOTICE("subscribe with interval %s", messageName.c_str());
					afbEvent = afb_daemon_make_event(messageName.c_str());
					if (!afb_event_is_valid(afbEvent))
					{
						AFB_ERROR("unable to create event");
						return false;
					}
					TimedEvent e;
					e.name = message;
					e.eventName = messageName;
					e.event = afbEvent;
					e.interval = ms;
					evts.push_back(e);
					timedEvents[ms] = evts;
				}
				if(afb_req_subscribe(request, afbEvent) == 0)
				{
					ok = true;
				} else {
					if(!found)
					{
						evts.erase(evts.end() - 1);
						timedEvents[ms] = evts;
					}
				}
				if(timedEvents.size() == 0)
				{
					timers.clear();
				} else if(ok)
				{
					startTimer(ms);
				}
			}
			break;
		}
	}
	return ok;
}

/// @brief Entry point for unsubscribing to a resource. Can optionnally include a time interval in ms.
///
/// @param[in] request: afb-request containing a json request, for instance {"name":"/car/demoboard/", "interval":1000}
///
/// @return true if unsubscription succeeded, false otherwise.
bool High::unsubscribe(afb_req request)
{
	json_object *args = afb_req_json(request);
	json_object *nameJson = NULL;
	json_object *intervalJson = NULL;
	if(!json_object_object_get_ex(args, "name", &nameJson))
		return false;
	json_object_object_get_ex(args, "interval", &intervalJson);
	int ms = -1;
	if(intervalJson)
		ms = json_object_get_int(intervalJson);
	std::string message(json_object_get_string(nameJson));
	if(message.size() == 0)
		return false;
	if(ms <= 0)
	{
		if(events.find(message) != events.end())
		{
			if(afb_req_unsubscribe(request, events.at(message)) == 0)
				return true;
		}
	} else {
		if(timedEvents.find(ms) == timedEvents.end())
			return false;
		const auto evts = timedEvents.at(ms);
		afb_event afbEvent;
		bool found = false;
		for(const auto & e : evts)
		{
			if(e.name == message)
			{
				afbEvent = e.event;
				found = true;
				break;
			}
		}
		if(!found)
			return false;
		if(afb_req_unsubscribe(request, afbEvent) == 0)
			return true;
	}
	return false;
}

/// @brief entry point for get requests. Accepts an optional list of properties to be included.
///
/// @param[in] request: afb-request containing a json request, for instance {"name":"/car/demoboard/", "fields":["vehicleSpeed"]},
///			**json: a pointer to a json object to be used for the reply.
///
/// @return true if get succeeded, false otherwise, and **json object generated with the reply.
bool High::get(afb_req request, json_object **json)
{
	json_object *args = afb_req_json(request);
	json_object *nameJson;
	json_object *fieldsJson;
	if(!json_object_object_get_ex(args, "name", &nameJson))
			return false;
	bool hasFields = json_object_object_get_ex(args, "fields", &fieldsJson);
	std::vector<std::string> fields;
	if(hasFields)
	{
		int arraylen = json_object_array_length(fieldsJson);
		json_object* jvalue;
		for(int i = 0; i < arraylen; ++i)
		{
		  jvalue = json_object_array_get_idx(fieldsJson, i);
		  fields.push_back(json_object_get_string(jvalue));
		}
	}
	const std::string name(json_object_get_string(nameJson));
	std::map<std::string, json_object*> jsons;
	for(const auto &p : registeredObjects)
	{
		if(startsWith(p.first, name))
			jsons[p.first] = generateJson(p.first, &fields);
	}
	if(jsons.size() == 0)
	{
		return false;
	}
	json_object *j = json_object_new_object();
	for(const auto &p : jsons)
	{
		json_object_object_add(j, p.first.c_str(), p.second);
	}
	*json = j;
	return true;
}

/// @brief Sub-routine (static) to check whether a string starts with another string
///
/// @param[in] s: string to scan, val: string to start with.
///
/// @return true if s starts with val, false otherwise.
bool High::startsWith(const std::string &s, const std::string &val)
{
	if(val.size() > s.size())
		return false;
	return s.substr(0, val.size()) == val;
}
