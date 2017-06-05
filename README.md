# High level binding

CAN binder, based upon ViWi definition.

This binding is intended to act between the low-level binding and clients. It collects resources as defined in a json configuration file, and
implements subscribe/unsubscribe/get verbs for the clients.

# build
```bash
mkdir build;cd build; cmake ..; make
```
# launching
natively under linux you can launch afb-daemon with the low-level and high-level bindings with a command like:

```bash
afb-daemon --rootdir=<path_to_low_binding>/CAN-binder/build/package --ldpaths=<path_to_low_binding>/CAN-binder/build/package/lib:<path_to_high_binding>/build/high-can-binding --port=1234 --tracereq=common --token=1 --verbose --verbose --verbose
```

#json configuration file
json configuration file (high.json) must be placed in the directory where you will launch afb-dameon
The json configuration file consists in 2 sections:

## definitions section
This section describes each resources defined in the high-level binding. Each resource is composed with different properties having a name, a type and a description.
Type can be boolean, double, string, or int. Properties "id", "uri" and "name" are compulsory.
For instance:
```json
{
	"name": "/car/demoboard/",
	"properties": {
		"id": {
			"type": "string",
			"description": "identifier"
		},
		"uri": {
			"type": "string",
			"description": "object uri"
		},
		"name": {
			"type": "string",
			"description": "name"
		},
		"unit": {
			"type": "string",
			"description": "units"
		},
		"speed": {
			"type": "double",
			"description": "vehicle centerpoint speed as shown by the instrument cluster"
		},
		"rpm": {
			"type": "double",
			"description": "engine rotations per minute"
		},
		"level": {
			"type": "double",
			"description": "level of tankage"
		},
		"load": {
			"type": "double",
			"description": "engine load"
		}
	}
}
```
## resources section
This section defines which values should be assigned to resource's properties as defined in the definitions section.
The link to the definitions section is made through the name of the resource.
Some values are static, some are linked to low-level requests.
In case a value is linked to a low-level request, the value will start with "${" and end with "}". In that case the value will consist in the name of the low-level signal, followed
with the frequency of the signal in ms. -1 in the frequency means that high level binding should subscribe to low level binding for all changes, without specifying a frequency.
For instance:
```json
{
	"name": "/car/demoboard/",
	"values": [{
		"name": "vehicleSpeed",
		"unit": "km/h",
		"speed": "${diagnostic_messages.vehicle.speed,1000}"
	}, {
		"name": "engineSpeed",
		"unit": "rpm",
		"rpm": "${diagnostic_messages.engine.speed,1000}"
	}, {
		"name": "fuelLevel",
		"unit": "litre",
		"level": "${diagnostic_messages.fuel.level,1000}"
	}, {
		"name": "engineLoad",
		"unit": "Nm",
		"load": "${diagnostic_messages.engine.load,1000}"
	}]
}
```
# Running and testing
You can use afb-client-demo to test high level binding.
For instance, once daemon has been launched with the 2 bindings:
```bash
afb-client-demo ws://localhost:1234/api?token=1
```
You can then use commands such as:
```bash
high-can subscribe {"name":"/car/doors/","interval":10000}
high-can unsubscribe {"name":"/car/doors/","interval":10000}
high-can get {"name":"/car/demoboard/"}
high-can get {"name":"/car/demoboard/","fields":["fuelLevel","engineLoad"]}
```
You can also inject some data in CAN bus using canplayer (example of data can be find in low-level binding example directory)
```bash
canplayer -I highwaycomplete
```
