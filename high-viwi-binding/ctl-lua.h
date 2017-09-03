
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <json-c/json.h>

#include "high-viwi-binding.hpp"

#ifndef CONTROL_DOSCRIPT_PRE
#define CONTROL_DOSCRIPT_PRE "doscript"
#endif

#ifndef CONTROL_CONFIG_PRE
#define CONTROL_CONFIG_PRE "onload"
#endif

#ifndef CONTROL_LUA_EVENT
#define CONTROL_LUA_EVENT "luaevt"
#endif

typedef int (*timerCallbackT)(void *context);

typedef struct TimerHandleS {
    int count;
    int delay;
    const char*label;
    void *context;
    timerCallbackT callback;
    sd_event_source *evtSource;
} TimerHandleT;

int TimerEvtInit (void);
afb_event TimerEvtGet(void);
void TimerEvtStart(TimerHandleT *timerHandle, timerCallbackT callback, void *context);
void TimerEvtStop(TimerHandleT *timerHandle);

typedef enum {
    CTL_MODE_NONE=0,
    CTL_MODE_API,
    CTL_MODE_CB,
    CTL_MODE_LUA,
} CtlRequestModeT;

typedef enum {
    CTL_SOURCE_CLOSE=-1,
    CTL_SOURCE_UNKNOWN=0,
    CTL_SOURCE_ONLOAD=1,
    CTL_SOURCE_OPEN=2,
    CTL_SOURCE_EVENT=3,
} DispatchSourceT;

typedef struct DispatchActionS{
    const char *info;
    const char* label;
    CtlRequestModeT mode;
    const char* api;
    const char* call;
    json_object *argsJ;
    int timeout;
    int (*actionCB)(DispatchSourceT source, const char*label,  json_object *argsJ, json_object *queryJ, void *context);
} DispatchActionT;

typedef enum {
    LUA_DOCALL,
    LUA_DOSTRING,
    LUA_DOSCRIPT,
} LuaDoActionT;

typedef int (*Lua2cFunctionT)(char *funcname, json_object *argsJ, void*context);
