/*
 * Copyright (C) 2016 "IoT.bzh"
 * Author Fulup Ar Foll <fulup@iot.bzh>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * ref:
 *  (manual) https://www.lua.org/manual/5.3/manual.html
 *  (lua->C) http://www.troubleshooters.com/codecorn/lua/lua_c_calls_lua.htm#_Anatomy_of_a_Lua_Call
 *  (lua/C Var) http://acamara.es/blog/2012/08/passing-variables-from-lua-5-2-to-c-and-vice-versa/
 *  (Lua/C Lib)https://john.nachtimwald.com/2014/07/12/wrapping-a-c-library-in-lua/
 *  (Lua/C Table) https://gist.github.com/SONIC3D/10388137
 *  (Lua/C Nested table) https://stackoverflow.com/questions/45699144/lua-nested-table-from-lua-to-c
 *  (Lua/C Wrapper) https://stackoverflow.com/questions/45699950/lua-passing-handle-to-function-created-with-newlib
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "ctl-lua.h"
#include "high-viwi-binding.hpp"
#include "wrap-json.h"

#define LUA_FIST_ARG 2  // when using luaL_newlib calllback receive libtable as 1st arg
#define LUA_MSG_MAX_LENGTH 512
#define JSON_ERROR (json_object*)-1

#define CTX_MAGIC 123456789
#define CTX_TOKEN "AFB_ctx"

#ifndef CONTROL_MAXPATH_LEN
#define CONTROL_MAXPATH_LEN 255
#endif

static afb_req NULL_AFBREQ = {};

static  lua_State* luaState;

typedef struct {
	char *name;
	int  count;
	afb_event event;
} LuaAfbEvent;
static LuaAfbEvent *luaDefaultEvt;

typedef struct {
	int ctxMagic;
	afb_req request;
	void *handle;
	char *info;
} LuaAfbContextT;

typedef struct {
  const char *callback;
  json_object *context;
  void *handle;
} LuaCallServiceT;

typedef enum {
	AFB_MSG_INFO,
	AFB_MSG_WARNING,
	AFB_MSG_NOTICE,
	AFB_MSG_DEBUG,
	AFB_MSG_ERROR,
} LuaAfbMessageT;

/*
 * Note(Fulup): I fail to use luaL_setmetatable and replaced it with a simple opaque
 * handle while waiting for someone smarter than me to find a better solution
 *  https://stackoverflow.com/questions/45596493/lua-using-lua-newuserdata-from-lua-pcall
 */

static LuaAfbContextT *LuaCtxCheck (lua_State *luaState, int index) {
  LuaAfbContextT *afbContext;
  //luaL_checktype(luaState, index, LUA_TUSERDATA);
  //afbContext = (LuaAfbContextT *)luaL_checkudata(luaState, index, CTX_TOKEN);
  luaL_checktype(luaState, index, LUA_TLIGHTUSERDATA);
  afbContext = (LuaAfbContextT *) lua_touserdata(luaState, index);
  if (afbContext == NULL && afbContext->ctxMagic != CTX_MAGIC) {
	  luaL_error(luaState, "Fail to retrieve user data context=%s", CTX_TOKEN);
	  AFB_ERROR ("afbContextCheck error retrieving afbContext");
	  return NULL;
  }
  return afbContext;
}

static LuaAfbContextT *LuaCtxPush (lua_State *luaState, afb_req request, void *handle, const char* info) {
  // LuaAfbContextT *afbContext = (LuaAfbContextT *)lua_newuserdata(luaState, sizeof(LuaAfbContextT));
  // luaL_setmetatable(luaState, CTX_TOKEN);
  LuaAfbContextT *afbContext = (LuaAfbContextT *)calloc(1, sizeof(LuaAfbContextT));
  lua_pushlightuserdata(luaState, afbContext);
  if (!afbContext) {
	  AFB_ERROR ("LuaCtxPush fail to allocate user data context");
	  return NULL;
  }
  afbContext->ctxMagic=CTX_MAGIC;
  afbContext->info=strdup(info);
  afbContext->request= request;
  afbContext->handle= handle;
  return afbContext;
}

static void LuaCtxFree (LuaAfbContextT *afbContext) {
	free(afbContext->info);
	free(afbContext);
}

// Push a json structure on the stack as a LUA table
static int LuaPushArgument (json_object *argsJ) {

	//AFB_NOTICE("LuaPushArgument argsJ=%s", json_object_get_string(argsJ));

	json_type jtype= json_object_get_type(argsJ);
	switch (jtype) {
		case json_type_object: {
			lua_newtable (luaState);
			json_object_object_foreach (argsJ, key, val) {
				int done = LuaPushArgument (val);
				if (done) {
					lua_setfield(luaState,-2, key);
				}
			}
			break;
		}
		case json_type_array: {
			int length= json_object_array_length(argsJ);
			lua_newtable (luaState);
			for (int idx=0; idx < length; idx ++) {
				json_object *val=json_object_array_get_idx(argsJ, idx);
				LuaPushArgument (val);
				lua_seti (luaState,-2, idx);
			}
			break;
		}
		case json_type_int:
			lua_pushinteger(luaState, json_object_get_int(argsJ));
			break;
		case json_type_string:
			lua_pushstring(luaState, json_object_get_string(argsJ));
			break;
		case json_type_boolean:
			lua_pushboolean(luaState, json_object_get_boolean(argsJ));
			break;
		case json_type_double:
			lua_pushnumber(luaState, json_object_get_double(argsJ));
			break;
		case json_type_null:
			AFB_WARNING("LuaPushArgument: NULL object type %s", json_object_get_string(argsJ));
			return 0;
			break;

		default:
			AFB_ERROR("LuaPushArgument: unsupported Json object type %s", json_object_get_string(argsJ));
			return 0;
	}
	return 1;
}

static  json_object *LuaPopOneArg (lua_State* luaState, int idx);

// Move a table from Internal Lua representation to Json one
// Numeric table are transformed in json array, string one in object
// Mix numeric/string key are not supported
static json_object *LuaTableToJson (lua_State* luaState, int index) {
	#define LUA_KEY_INDEX -2
	#define LUA_VALUE_INDEX -1

	int idx;
	int tableType;
	json_object *tableJ= NULL;

	lua_pushnil(luaState); // 1st key
	if (index < 0) index--;
	for (idx=1; lua_next(luaState, index) != 0; idx++) {

		// uses 'key' (at index -2) and 'value' (at index -1)
		if (lua_type(luaState,LUA_KEY_INDEX) == LUA_TSTRING) {

			if (!tableJ) {
				tableJ= json_object_new_object();
				tableType=LUA_TSTRING;
			} else if (tableType != LUA_TSTRING){
				AFB_ERROR("MIX Lua Table with key string/numeric not supported");
				return NULL;
			}

			const char *key= lua_tostring(luaState, LUA_KEY_INDEX);
			json_object *argJ= LuaPopOneArg(luaState, LUA_VALUE_INDEX);
			json_object_object_add(tableJ, key, argJ);

		} else {
			if (!tableJ) {
				tableJ= json_object_new_array();
				tableType=LUA_TNUMBER;
			} else if(tableType != LUA_TNUMBER) {
				AFB_ERROR("MIX Lua Table with key numeric/string not supported");
				return NULL;
			}

			json_object *argJ= LuaPopOneArg(luaState, LUA_VALUE_INDEX);
			json_object_array_add(tableJ, argJ);
		}


		lua_pop(luaState, 1); // removes 'value'; keeps 'key' for next iteration
	}

	// Query is empty free empty json object
	if (idx == 1) {
		json_object_put(tableJ);
		return NULL;
	}
	return tableJ;
}

static  json_object *LuaPopOneArg (lua_State* luaState, int idx) {
	json_object *value=NULL;

	int luaType = lua_type(luaState, idx);
	switch(luaType)  {
		case LUA_TNUMBER: {
			lua_Number number= lua_tonumber(luaState, idx);;
			int nombre = (int)number; // evil trick to determine wether n fits in an integer. (stolen from ltcl.c)
			if (number == nombre) {
				value= json_object_new_int((int)number);
			} else {
				value= json_object_new_double(number);
			}
			break;
		}
		case LUA_TBOOLEAN:
			value=  json_object_new_boolean(lua_toboolean(luaState, idx));
			break;
		case LUA_TSTRING:
		   value=  json_object_new_string(lua_tostring(luaState, idx));
			break;
		case LUA_TTABLE:
			value= LuaTableToJson(luaState, idx);
			break;
		case LUA_TNIL:
			value=json_object_new_string("nil") ;
			break;
		case LUA_TUSERDATA:
			value=json_object_new_int64((int64_t)lua_touserdata(luaState, idx)); // store userdata as int !!!
			break;

		default:
			AFB_NOTICE ("LuaPopOneArg: script returned Unknown/Unsupported idx=%d type:%d/%s", idx, luaType, lua_typename(luaState, luaType));
			value=NULL;
	}

	return value;
}

static json_object *LuaPopArgs (lua_State* luaState, int start) {
	json_object *responseJ;

	int stop = lua_gettop(luaState);
	if(stop-start <0) return NULL;

	// start at 2 because we are using a function array lib
	if (start == stop) {
		responseJ=LuaPopOneArg (luaState, start);
	} else {
		// loop on remaining return arguments
		responseJ= json_object_new_array();
		for (int idx=start; idx <= stop; idx++) {
			json_object *argJ=LuaPopOneArg (luaState, idx);
			if (!argJ) goto OnErrorExit;
			json_object_array_add(responseJ, argJ);
	   }
	}

	return responseJ;

  OnErrorExit:
	return NULL;
}


static int LuaFormatMessage(lua_State* luaState, LuaAfbMessageT action) {
	char *message;

	json_object *responseJ= LuaPopArgs(luaState, LUA_FIST_ARG);

	if (!responseJ) {
		luaL_error(luaState,"LuaFormatMessage empty message");
		goto OnErrorExit;
	}

	// if we have only on argument just return the value.
	if (json_object_get_type(responseJ)!=json_type_array || json_object_array_length(responseJ) <2) {
		message= (char*)json_object_get_string(responseJ);
		goto PrintMessage;
	}

	// extract format and push all other parameter on the stack
	message = alloca (LUA_MSG_MAX_LENGTH);
	const char *format = json_object_get_string(json_object_array_get_idx(responseJ, 0));

	int arrayIdx=1;
	int targetIdx=0;

	for (int idx=0; format[idx] !='\0'; idx++) {

		if (format[idx]=='%' && format[idx] !='\0') {
			json_object *slotJ= json_object_array_get_idx(responseJ, arrayIdx);
			//if (slotJ) AFB_NOTICE("**** idx=%d slotJ=%s", arrayIdx, json_object_get_string(slotJ));


			switch (format[++idx]) {
				case 'd':
					if (slotJ) targetIdx += snprintf (&message[targetIdx], LUA_MSG_MAX_LENGTH-targetIdx,"%d", json_object_get_int(slotJ));
					else  targetIdx += snprintf (&message[targetIdx], LUA_MSG_MAX_LENGTH-targetIdx,"nil");
					arrayIdx++;
					break;
				case 'f':
					if (slotJ) targetIdx += snprintf (&message[targetIdx], LUA_MSG_MAX_LENGTH-targetIdx,"%f", json_object_get_double(slotJ));
					else  targetIdx += snprintf (&message[targetIdx], LUA_MSG_MAX_LENGTH-targetIdx,"nil");
					arrayIdx++;
					break;

				case'%':
					message[targetIdx]='%';
					targetIdx++;
					break;

				case 's':
				default:
					if (slotJ) targetIdx += snprintf (&message[targetIdx], LUA_MSG_MAX_LENGTH-targetIdx,"%s", json_object_get_string(slotJ));
					else  targetIdx += snprintf (&message[targetIdx], LUA_MSG_MAX_LENGTH-targetIdx,"nil");
					arrayIdx++;
				}

		} else {
			if (targetIdx >= LUA_MSG_MAX_LENGTH) {
				AFB_WARNING ("LuaFormatMessage: message[%s] owerverflow LUA_MSG_MAX_LENGTH=%d", format, LUA_MSG_MAX_LENGTH);
				targetIdx --; // move backward for EOL
				break;
			} else {
				message[targetIdx++] = format[idx];
			}
		}
	}
	message[targetIdx]='\0';

PrintMessage:
	switch (action) {
		case  AFB_MSG_WARNING:
			AFB_WARNING ("%s", message);
			break;
		case AFB_MSG_NOTICE:
			AFB_NOTICE ("%s", message);
			break;
		case AFB_MSG_DEBUG:
			AFB_DEBUG ("%s", message);
			break;
		case AFB_MSG_INFO:
			AFB_INFO ("%s", message);
			break;
		case AFB_MSG_ERROR:
		default:
			AFB_ERROR ("%s", message);
	}
	return 0;  // nothing return to lua

  OnErrorExit: // on argument to return (the error message)
	return 1;
}

static int LuaPrintInfo(lua_State* luaState) {
	int err=LuaFormatMessage (luaState, AFB_MSG_INFO);
	return err;
}

static int LuaPrintError(lua_State* luaState) {
	int err=LuaFormatMessage (luaState, AFB_MSG_ERROR);
	return err; // no value return
}

static int LuaPrintWarning(lua_State* luaState) {
	int err=LuaFormatMessage (luaState, AFB_MSG_WARNING);
	return err;
}

static int LuaPrintNotice(lua_State* luaState) {
	int err=LuaFormatMessage (luaState, AFB_MSG_NOTICE);
	return err;
}

static int LuaPrintDebug(lua_State* luaState) {
	int err=LuaFormatMessage (luaState, AFB_MSG_DEBUG);
	return err;
}

static int LuaAfbSuccess(lua_State* luaState) {
	LuaAfbContextT *afbContext= LuaCtxCheck(luaState, LUA_FIST_ARG);
	if (!afbContext) goto OnErrorExit;

	// ignore context argument
	json_object *responseJ= LuaPopArgs(luaState, LUA_FIST_ARG+1);
	if (responseJ == JSON_ERROR) return 1;

	afb_req_success(afbContext->request, responseJ, NULL);

	LuaCtxFree(afbContext);
	return 0;

 OnErrorExit:
		lua_error(luaState);
		return 1;
}

static int LuaAfbFail(lua_State* luaState) {
	LuaAfbContextT *afbContext= LuaCtxCheck(luaState, LUA_FIST_ARG);
	if (!afbContext) goto OnErrorExit;

	json_object *responseJ= LuaPopArgs(luaState, LUA_FIST_ARG+1);
	if (responseJ == JSON_ERROR) return 1;

	afb_req_fail(afbContext->request, afbContext->info, json_object_get_string(responseJ));

	LuaCtxFree(afbContext);
	return 0;

 OnErrorExit:
		lua_error(luaState);
		return 1;
}

static void LuaAfbServiceCB(void *handle, int iserror, struct json_object *responseJ) {
	LuaCallServiceT *contextCB= (LuaCallServiceT*)handle;
	int count=1;

	lua_getglobal(luaState, contextCB->callback);

	// push error status & response
	lua_pushboolean(luaState, iserror);
	count+= LuaPushArgument(responseJ);
	count+= LuaPushArgument(contextCB->context);

	int err=lua_pcall(luaState, count, LUA_MULTRET, 0);
	if (err) {
		AFB_ERROR ("LUA-SERICE-CB:FAIL response=%s err=%s", json_object_get_string(responseJ), lua_tostring(luaState,-1) );
	}

	free (contextCB);
}


static int LuaAfbService(lua_State* luaState) {
	int count = lua_gettop(luaState);

	// note: argument start at 2 because of AFB: table
	if (count <5 || !lua_isstring(luaState, 2) || !lua_isstring(luaState, 3) || !lua_istable(luaState, 4) || !lua_isstring(luaState, 5)) {
		lua_pushliteral (luaState, "ERROR: syntax AFB:service(api, verb, {[Lua Table]})");
		goto OnErrorExit;
	}

	// get api/verb+query
	const char *api = lua_tostring(luaState,2);
	const char *verb= lua_tostring(luaState,3);
	json_object *queryJ= LuaTableToJson(luaState, 4);
	if (queryJ == JSON_ERROR) return 1;

	LuaCallServiceT *contextCB = calloc (1, sizeof(LuaCallServiceT));
	contextCB->callback= lua_tostring(luaState, 5);
	contextCB->context =  LuaPopArgs(luaState, 6);

	afb_service_call(api, verb, queryJ, LuaAfbServiceCB, contextCB);

	return 0; // no value return

  OnErrorExit:
		lua_error(luaState);
		return 1;
}

static int LuaAfbServiceSync(lua_State* luaState) {
	int count = lua_gettop(luaState);
	json_object *responseJ;

	// note: argument start at 2 because of AFB: table
	if (count <3 || !lua_isstring(luaState, 2) || !lua_isstring(luaState, 3) || !lua_istable(luaState, 4)) {
		lua_pushliteral (luaState, "ERROR: syntax AFB:servsync(api, verb, {[Lua Table]})");
		goto OnErrorExit;
	}

	// get api/verb+query
	const char *api = lua_tostring(luaState,2);
	const char *verb= lua_tostring(luaState,3);
	json_object *queryJ= LuaTableToJson(luaState, 4);

	int iserror=afb_service_call_sync (api, verb, queryJ, &responseJ);

	// push error status & response
	count=1; lua_pushboolean(luaState, iserror);
	count+= LuaPushArgument(responseJ);

	return count; // return count values

  OnErrorExit:
		lua_error(luaState);
		return 1;
}

static int LuaAfbEventPush(lua_State* luaState) {
	LuaAfbEvent *afbevt;
	int index;

	// if no private event handle then use default binding event
	if (lua_islightuserdata(luaState, LUA_FIST_ARG)) {
		afbevt = (LuaAfbEvent*) lua_touserdata(luaState, LUA_FIST_ARG);
		index=LUA_FIST_ARG+1;
	} else {
		index=LUA_FIST_ARG;
		afbevt=luaDefaultEvt;
	}

	if (!afb_event_is_valid(afbevt->event)) {
		lua_pushliteral (luaState, "LuaAfbMakePush-Fail invalid event");
		goto OnErrorExit;
	}

	json_object *ctlEventJ= LuaTableToJson(luaState, index);
	if (!ctlEventJ) {
		lua_pushliteral (luaState, "LuaAfbEventPush-Syntax is AFB:signal ([evtHandle], {lua table})");
		goto OnErrorExit;
	}

	int done = afb_event_push(afbevt->event, ctlEventJ);
	if (!done) {
		lua_pushliteral (luaState, "LuaAfbEventPush-Fail No Subscriber to event");
		AFB_ERROR ("LuaAfbEventPush-Fail name subscriber event=%s count=%d", afbevt->name, afbevt->count);
		goto OnErrorExit;
	}
	afbevt->count++;
	return 0;

  OnErrorExit:
		lua_error(luaState);
		return 1;
}

static int LuaAfbEventSubscribe(lua_State* luaState) {
	LuaAfbEvent *afbevt;

	LuaAfbContextT *afbContext= LuaCtxCheck(luaState, LUA_FIST_ARG);
	if (!afbContext) {
		lua_pushliteral (luaState, "LuaAfbEventSubscribe-Fail Invalid request handle");
		goto OnErrorExit;
	}

	// if no private event handle then use default binding event
	if (lua_islightuserdata(luaState, LUA_FIST_ARG+1)) {
		afbevt = (LuaAfbEvent*) lua_touserdata(luaState, LUA_FIST_ARG+1);
	} else {
		afbevt=luaDefaultEvt;
	}

	if (!afb_event_is_valid(afbevt->event)) {
		lua_pushliteral (luaState, "LuaAfbMakePush-Fail invalid event handle");
		goto OnErrorExit;
	}

	int err = afb_req_subscribe(afbContext->request, afbevt->event);
	if (err) {
		lua_pushliteral (luaState, "LuaAfbEventSubscribe-Fail No Subscriber to event");
		AFB_ERROR ("LuaAfbEventPush-Fail name subscriber event=%s count=%d", afbevt->name, afbevt->count);
		goto OnErrorExit;
	}
	afbevt->count++;
	return 0;

  OnErrorExit:
		lua_error(luaState);
		return 1;
}

static int LuaAfbEventMake(lua_State* luaState) {
	int count = lua_gettop(luaState);
	LuaAfbEvent *afbevt=calloc(1,sizeof(LuaAfbEvent));

	if (count != LUA_FIST_ARG || !lua_isstring(luaState, LUA_FIST_ARG)) {
		lua_pushliteral (luaState, "LuaAfbEventMake-Syntax is evtHandle= AFB:event ('myEventName')");
		goto OnErrorExit;
	}

	// event name should be the only argument
	afbevt->name= strdup (lua_tostring(luaState,LUA_FIST_ARG));

	// create a new binder event
	afbevt->event = afb_daemon_make_event(afbevt->name);
	if (!afb_event_is_valid(afbevt->event)) {
		lua_pushliteral (luaState, "LuaAfbEventMake-Fail to Create Binder event");
		goto OnErrorExit;
	}

	// push event handler as a LUA opaque handle
	lua_pushlightuserdata(luaState, afbevt);
	return 1;

  OnErrorExit:
		lua_error(luaState);
		return 1;
}

// Function call from LUA when lua2c plugin L2C is used
int Lua2cWrapper(lua_State* luaState, char *funcname, Lua2cFunctionT callback, void *context) {

	json_object *argsJ= LuaPopArgs(luaState, LUA_FIST_ARG+1);
	int response = (*callback) (funcname, argsJ, context);

	// push response to LUA
	lua_pushinteger(luaState, response);
	return 1;
}

// Generated some fake event based on watchdog/counter
int LuaCallFunc (DispatchSourceT source, DispatchActionT *action, json_object *queryJ) {

	int err, count;

	json_object* argsJ  = action->argsJ;
	const char*  func   = action->call;

	// load function (should exist in CONTROL_PATH_LUA
	lua_getglobal(luaState, func);

	// push source on the stack
	count=1;
	lua_pushinteger(luaState, source);

	// push argsJ on the stack
	if (!argsJ) {
		lua_pushnil(luaState);
		count++;
	} else {
		count+= LuaPushArgument (argsJ);
	}

	// push queryJ on the stack
	if (!queryJ) {
		lua_pushnil(luaState);
		count++;
	} else {
		count+= LuaPushArgument (queryJ);
	}

	// effectively exec LUA script code
	err=lua_pcall(luaState, count, 1, 0);
	if (err)  {
		AFB_ERROR("LuaCallFunc Fail calling %s error=%s", func, lua_tostring(luaState,-1));
		goto OnErrorExit;
	}

	// return LUA script value
	int rc= (int)lua_tointeger(luaState, -1);
	return rc;

  OnErrorExit:
	return -1;
}


// Execute LUA code from received API request
static void LuaDoAction (LuaDoActionT action, afb_req request) {

	int err, count=0;

	json_object* queryJ = afb_req_json(request);

	switch (action) {

		case LUA_DOSTRING: {
			const char *script = json_object_get_string(queryJ);
			err=luaL_loadstring(luaState, script);
			if (err) {
				AFB_ERROR ("LUA-DO-COMPILE:FAIL String=%s err=%s", script, lua_tostring(luaState,-1) );
				goto OnErrorExit;
			}
			// Push AFB client context on the stack
			LuaAfbContextT *afbContext= LuaCtxPush(luaState, request,NULL,script);
			if (!afbContext) goto OnErrorExit;

			break;
		}

		case LUA_DOCALL: {
			const char *func;
			json_object *argsJ=NULL;

			err= wrap_json_unpack (queryJ, "{s:s, s?o !}", "target", &func, "args", &argsJ);
			if (err) {
				AFB_ERROR ("LUA-DOCALL-SYNTAX missing target|args query=%s", json_object_get_string(queryJ));
				goto OnErrorExit;
			}

			// load function (should exist in CONTROL_PATH_LUA
			lua_getglobal(luaState, func);

			// Push AFB client context on the stack
			LuaAfbContextT *afbContext= LuaCtxPush(luaState, request, NULL, func);
			if (!afbContext) goto OnErrorExit;

			// push query on the stack
			if (!argsJ) {
				lua_pushnil(luaState);
				count++;
			} else {
				count+= LuaPushArgument (argsJ);
			}

			break;
		}

		case LUA_DOSCRIPT: {   // Fulup need to fix argument passing
			char *filename; char*fullpath;
			char luaScriptPath[CONTROL_MAXPATH_LEN];
			int index;

			// scan luascript search path once
			static json_object *luaScriptPathJ =NULL;

			// extract value from query
			const char *target=NULL,*func=NULL;
			json_object *argsJ=NULL;
			err= wrap_json_unpack (queryJ, "{s:s,s?s,s?s,s?o !}","target", &target,"path",&luaScriptPathJ,"function",&func,"args",&argsJ);
			if (err) {
				AFB_ERROR ("LUA-DOSCRIPT-SYNTAX:missing target|[path]|[function]|[args] query=%s", json_object_get_string(queryJ));
				goto OnErrorExit;
			}

			// search for filename=script in CONTROL_LUA_PATH
			if (!luaScriptPathJ)  {
				strncpy(luaScriptPath,CONTROL_DOSCRIPT_PRE, sizeof(luaScriptPath));
				strncat(luaScriptPath,"-", sizeof(luaScriptPath)-strlen(luaScriptPath)-1);
				strncat(luaScriptPath,target, sizeof(luaScriptPath)-strlen(luaScriptPath)-1);
				luaScriptPathJ= ScanForConfig(CONTROL_LUA_PATH , CTL_SCAN_RECURSIVE,luaScriptPath,".lua");
			}
			for (index=0; index < json_object_array_length(luaScriptPathJ); index++) {
				json_object *entryJ=json_object_array_get_idx(luaScriptPathJ, index);

				err= wrap_json_unpack (entryJ, "{s:s, s:s !}", "fullpath",  &fullpath,"filename", &filename);
				if (err) {
					AFB_ERROR ("LUA-DOSCRIPT-SCAN:HOOPs invalid config file path = %s", json_object_get_string(entryJ));
					goto OnErrorExit;
				}

				if (index > 0) AFB_WARNING("LUA-DOSCRIPT-SCAN:Ignore second script=%s path=%s", filename, fullpath);
				else {
					strncpy (luaScriptPath, fullpath, sizeof(luaScriptPath));
					strncat (luaScriptPath, "/", sizeof(luaScriptPath)-strlen(luaScriptPath)-1);
					strncat (luaScriptPath, filename, sizeof(luaScriptPath)-strlen(luaScriptPath)-1);
				}
			}

			err= luaL_loadfile(luaState, luaScriptPath);
			if (err) {
				AFB_ERROR ("LUA-DOSCRIPT HOOPs Error in LUA loading scripts=%s err=%s", luaScriptPath, lua_tostring(luaState,-1));
				goto OnErrorExit;
			}

			// script was loaded we need to parse to make it executable
			err=lua_pcall(luaState, 0, 0, 0);
			if (err) {
				AFB_ERROR ("LUA-DOSCRIPT:FAIL to load %s", luaScriptPath);
				goto OnErrorExit;
			}

			// if no func name given try to deduct from filename
			if (!func && (func=(char*)GetMidleName(filename))!=NULL) {
				strncpy(luaScriptPath,"_", sizeof(luaScriptPath));
				strncat(luaScriptPath,func, sizeof(luaScriptPath)-strlen(luaScriptPath)-1);
				func=luaScriptPath;
			}
			if (!func) {
				AFB_ERROR ("LUA-DOSCRIPT:FAIL to deduct funcname from %s", filename);
				goto OnErrorExit;
			}

			// load function (should exist in CONTROL_PATH_LUA
			lua_getglobal(luaState, func);

			// Push AFB client context on the stack
			LuaAfbContextT *afbContext= LuaCtxPush(luaState, request, NULL, func);
			if (!afbContext) goto OnErrorExit;

			// push function arguments
			if (!argsJ) {
				lua_pushnil(luaState);
				count++;
			} else {
				count+= LuaPushArgument(argsJ);
			}

			break;
		}

		default:
			AFB_ERROR ("LUA-DOSCRIPT-ACTION unknown query=%s", json_object_get_string(queryJ));
			goto OnErrorExit;
	}

	// effectively exec LUA code (afb_reply/fail done later from callback)
	err=lua_pcall(luaState, count+1, 0, 0);
	if (err) {
		AFB_ERROR ("LUA-DO-EXEC:FAIL query=%s err=%s", json_object_get_string(queryJ), lua_tostring(luaState,-1));
		goto OnErrorExit;
	}
	return;

 OnErrorExit:
	afb_req_fail(request,"LUA:ERROR", lua_tostring(luaState,-1));
	return;
}

void ctlapi_execlua (afb_req request) {
	LuaDoAction (LUA_DOSTRING, request);
}

void ctlapi_request (afb_req request) {
	LuaDoAction (LUA_DOCALL, request);
}

void ctlapi_debuglua (afb_req request) {
	LuaDoAction (LUA_DOSCRIPT, request);
}

static int LuaTimerClear (lua_State* luaState) {

	// Get Timer Handle
	LuaAfbContextT *afbContext= LuaCtxCheck(luaState, LUA_FIST_ARG);
	if (!afbContext) goto OnErrorExit;

	// retrieve useful information opaque handle
	TimerHandleT *timerHandle = (TimerHandleT*)afbContext->handle;

	AFB_NOTICE ("LuaTimerClear timer=%s", timerHandle->label);
	TimerEvtStop(timerHandle);

	return 0; //happy end

OnErrorExit:
	return 1;
}
static int LuaTimerGet (lua_State* luaState) {

	// Get Timer Handle
	LuaAfbContextT *afbContext= LuaCtxCheck(luaState, LUA_FIST_ARG);
	if (!afbContext) goto OnErrorExit;

	// retrieve useful information opaque handle
	TimerHandleT *timerHandle = (TimerHandleT*)afbContext->handle;

	// create response as a JSON object
	json_object *responseJ= json_object_new_object();
	json_object_object_add(responseJ,"label", json_object_new_string(timerHandle->label));
	json_object_object_add(responseJ,"delay", json_object_new_int(timerHandle->delay));
	json_object_object_add(responseJ,"count", json_object_new_int(timerHandle->count));

	// return JSON object as Lua table
	int count=LuaPushArgument(responseJ);

	// free json object
	json_object_put(responseJ);

	return count; // return argument

OnErrorExit:
	return 0;
}

// Timer Callback

// Set timer
static int LuaTimerSetCB (void *handle) {
	LuaCallServiceT *contextCB =(LuaCallServiceT*) handle;
	TimerHandleT *timerHandle = (TimerHandleT*) contextCB->handle;
	int count;

	// push timer handle and user context on Lua stack
	lua_getglobal(luaState, contextCB->callback);

	// Push timer handle
	LuaAfbContextT *afbContext= LuaCtxPush(luaState, NULL_AFBREQ, contextCB->handle, timerHandle->label);
	if (!afbContext) goto OnErrorExit;
	count=1;

	// Push user Context
	count+= LuaPushArgument(contextCB->context);

	int err=lua_pcall(luaState, count, LUA_MULTRET, 0);
	if (err) {
		AFB_ERROR ("LUA-TIMER-CB:FAIL response=%s err=%s", json_object_get_string(contextCB->context), lua_tostring(luaState,-1));
		goto OnErrorExit;
	}

	// get return parameter
	if (!lua_isboolean(luaState, -1)) {
		return (lua_toboolean(luaState, -1));
	}

	// timer last run free context resource
	if (timerHandle->count == 1) {
		LuaCtxFree(afbContext);
	}
	return 0;  // By default we are happy

 OnErrorExit:
	return 1;  // stop timer
}

static int LuaTimerSet(lua_State* luaState) {
	const char *label=NULL, *info=NULL;
	int delay=0, count=0;

	json_object *timerJ = LuaPopOneArg(luaState, LUA_FIST_ARG);
	const char *callback = lua_tostring(luaState, LUA_FIST_ARG + 1);
	json_object *contextJ = LuaPopOneArg(luaState, LUA_FIST_ARG + 2);

	if (lua_gettop(luaState) != LUA_FIST_ARG+2 || !timerJ || !callback || !contextJ) {
		lua_pushliteral(luaState, "LuaTimerSet-Syntax timerset (timerT, 'callback', contextT)");
		goto OnErrorExit;
	}

	int err = wrap_json_unpack(timerJ, "{ss, s?s si, si !}", "label", &label, "info", &info, "delay", &delay, "count", &count);
	if (err) {
		lua_pushliteral(luaState, "LuaTimerSet-Syntax timerT={label:xxx delay:ms, count:xx}");
		goto OnErrorExit;
	}

	// everything look fine create timer structure
	TimerHandleT *timerHandle = malloc (sizeof (TimerHandleT));
	timerHandle->delay=delay;
	timerHandle->count=count;
	timerHandle->label=label;

	// Allocate handle to store context and callback
	LuaCallServiceT *contextCB = calloc (1, sizeof(LuaCallServiceT));
	contextCB->callback= callback;
	contextCB->context = contextJ;
	contextCB->handle  = timerHandle;

	// fire timer
	TimerEvtStart (timerHandle, LuaTimerSetCB, contextCB);

	return 0;  // Happy No Return Function

OnErrorExit:
	lua_error(luaState);
	return 1; // return error code
}

// Register a new L2c list of LUA user plugin commands
void LuaL2cNewLib(const char *label, luaL_Reg *l2cFunc, int count) {
	// luaL_newlib(luaState, l2cFunc); macro does not work with pointer :(
	luaL_checkversion(luaState);
	lua_createtable(luaState, 0, count+1);
	luaL_setfuncs(luaState,l2cFunc,0);
	lua_setglobal(luaState, label);
}

static const luaL_Reg afbFunction[] = {
	{"timerclear", LuaTimerClear},
	{"timerget"  , LuaTimerGet},
	{"timerset"  , LuaTimerSet},
	{"notice"	, LuaPrintNotice},
	{"info"	  , LuaPrintInfo},
	{"warning"   , LuaPrintWarning},
	{"debug"	 , LuaPrintDebug},
	{"error"	 , LuaPrintError},
	{"servsync"  , LuaAfbServiceSync},
	{"service"   , LuaAfbService},
	{"success"   , LuaAfbSuccess},
	{"fail"	  , LuaAfbFail},
	{"subscribe" , LuaAfbEventSubscribe},
	{"evtmake"   , LuaAfbEventMake},
	{"evtpush"   , LuaAfbEventPush},

	{NULL, NULL}  /* sentinel */
};

// Create Binding Event at Init
int LuaLibInit () {
	int err, index;

	// search for default policy config file
	char fullprefix[CONTROL_MAXPATH_LEN];
	strncpy (fullprefix, CONTROL_CONFIG_PRE "-", sizeof(fullprefix));
	strncat (fullprefix, GetBinderName(), sizeof(fullprefix)-strlen(fullprefix)-1);
	strncat (fullprefix, "-", sizeof(fullprefix)-strlen(fullprefix)-1);

	const char *dirList= getenv("CONTROL_LUA_PATH");
	if (!dirList) dirList=CONTROL_LUA_PATH;

	json_object *luaScriptPathJ = ScanForConfig(dirList , CTL_SCAN_RECURSIVE, fullprefix, "lua");

	// open a new LUA interpretor
	luaState = luaL_newstate();
	if (!luaState)  {
		AFB_ERROR ("LUA_INIT: Fail to open lua interpretor");
		goto OnErrorExit;
	}

	// load auxiliary libraries
	luaL_openlibs(luaState);

	// redirect print to AFB_NOTICE
	luaL_newlib(luaState, afbFunction);
	lua_setglobal(luaState, "AFB");

	// create default lua event to send test pause/resume
	luaDefaultEvt=calloc(1,sizeof(LuaAfbEvent));
	luaDefaultEvt->name=CONTROL_LUA_EVENT;
	luaDefaultEvt->event = afb_daemon_make_event(CONTROL_LUA_EVENT);
	if (!afb_event_is_valid(luaDefaultEvt->event)) {
		AFB_ERROR ("POLCTL_INIT: Cannot register lua-events=%s ", CONTROL_LUA_EVENT);
		goto OnErrorExit;;
	}

	// load+exec any file found in LUA search path
	for (index=0; index < json_object_array_length(luaScriptPathJ); index++) {
		json_object *entryJ=json_object_array_get_idx(luaScriptPathJ, index);

		char *filename; char*fullpath;
		err= wrap_json_unpack (entryJ, "{s:s, s:s !}", "fullpath",  &fullpath,"filename", &filename);
		if (err) {
			AFB_ERROR ("LUA-INIT HOOPs invalid config file path = %s", json_object_get_string(entryJ));
			goto OnErrorExit;
		}

		char filepath[CONTROL_MAXPATH_LEN];
		strncpy(filepath, fullpath, sizeof(filepath));
		strncat(filepath, "/", sizeof(filepath)-strlen(filepath)-1);
		strncat(filepath, filename, sizeof(filepath)-strlen(filepath)-1);
		err= luaL_loadfile(luaState, filepath);
		if (err) {
			AFB_ERROR ("LUA-LOAD HOOPs Error in LUA loading scripts=%s err=%s", filepath, lua_tostring(luaState,-1));
			goto OnErrorExit;
		}

		// exec/compil script
		err = lua_pcall(luaState, 0, 0, 0);
		if (err) {
			AFB_ERROR ("LUA-LOAD HOOPs Error in LUA exec scripts=%s err=%s", filepath, lua_tostring(luaState,-1));
			goto OnErrorExit;
		}
	}

	// no policy config found remove control API from binder
	if (index == 0)  {
		AFB_WARNING ("POLICY-INIT:WARNING (setenv CONTROL_LUA_PATH) No LUA '%s*.lua' in '%s'", fullprefix, dirList);
	}

	AFB_DEBUG ("Audio control-LUA Init Done");
	return 0;

 OnErrorExit:
	return 1;
}
c