
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <json-c/json.h>

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
