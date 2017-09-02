#pragma once

#include <systemd/sd-event.h>

#ifdef __cplusplus
	#include <cstddef>
extern "C"
{
#endif
	#define AFB_BINDING_VERSION 2
	#include <afb/afb-binding.h>
#ifdef __cplusplus
};
#endif

	void onEvent(const char *event, struct json_object *object);
	int init_service();
	int ticked(sd_event_source *source, uint64_t t, void *data);
