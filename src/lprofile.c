#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lua.h"
#include "lauxlib.h"

const char* C_SOURCE = "=[C]";

typedef struct scall {
  clock_t start_time;
  clock_t end_time;
  const char *caller_source;
  const char *caller_name;
  int caller_linedefined;
  const char *source;
  const char *name;
  int linedefined;
} CALL;

CALL* calls;
CALL* calls_tmp;
int num_calls;
int size_calls;

int *call_stack;
int *call_stack_tmp;
int num_call_stack;
int size_call_stack;

static void callhook(lua_State *L, lua_Debug *ar) {
  int call_index;
  int call_stack_index;

  lua_getinfo(L, "nS", ar);

  if (!ar->event) {
    /* Entering a function */
    CALL call;

    /* Get info on the caller of this function */
    lua_Debug previous_ar;
    if (lua_getstack(L, 1, &previous_ar) == 0) {
      call.caller_name = "(top)";
      call.caller_source = "(top)";
      call.caller_linedefined = -1;
    } else {
      lua_getinfo(L, "nS", &previous_ar);
      call.caller_name = previous_ar.name;
      call.caller_source = previous_ar.source;
      call.caller_linedefined = previous_ar.linedefined;
    }

    /* Populate the call info */
    call.start_time = clock();
    call.source = ar->source;
    call.name = ar->name;
    call.linedefined = ar->linedefined;

    /* Insert the call into the calls array, resizing as needed */
    call_index = num_calls;
    num_calls += 1;
    if (num_calls > size_calls) {
      calls_tmp = malloc(2 * size_calls * sizeof(CALL));
      for (int i = 0; i < size_calls; i ++) {
        calls_tmp[i] = calls[i];
      }
      free(calls);
      calls = calls_tmp;
      size_calls = size_calls * 2;
    }
    calls[call_index] = call;

    /* Push call onto the call stack only if it's not a C function  */
    if (strncmp(C_SOURCE, call.source, 4) != 0) {
      call_stack_index = num_call_stack;
      num_call_stack += 1;
      if (num_call_stack > size_call_stack) {
        call_stack_tmp = malloc(2 * size_call_stack * sizeof(int));
        for (int i = 0; i < size_call_stack; i ++) {
          call_stack_tmp[i] = call_stack[i];
        }
        free(call_stack);
        call_stack = call_stack_tmp;
        size_call_stack = size_call_stack * 2;
      }
      call_stack[call_stack_index] = call_index;
    }

  } else {
    /* Exiting a function */
    CALL *call;

    /* Pop the latest call off the stack and set its end_time */
    call_stack_index = num_call_stack - 1;
    call_index = call_stack[call_stack_index];
    call = &calls[call_index];
    call->end_time = clock();
    num_call_stack = num_call_stack - 1;
  }

}

static int profiler_start(lua_State *L) {
  /* Initialize the profiler state */
  size_calls = 10000;
  calls = malloc(size_calls * sizeof(CALL));
  size_call_stack = size_calls;
  call_stack = malloc(size_call_stack * sizeof(int));

  /* Register the callhook with the lua debug module */
  lua_sethook(L, (lua_Hook)callhook, LUA_MASKCALL | LUA_MASKRET, 0);

  /* Return success */
  lua_pushboolean(L, 1);
  return 1;
}

static int profiler_stop(lua_State *L) {
  CALL *call;
  float duration;

  /* Deregister the callhook */
  lua_sethook(L, (lua_Hook)callhook, 0, 0);

  /* Deallocate the call_stack */
  free(call_stack);

  /* Output the calls */
  for (int i = 0; i < num_calls; i ++) {
    call = &calls[i];
    if (call->end_time) {
      duration = ((float)(call->end_time - call->start_time)) / ((float) CLOCKS_PER_SEC);
    } else {
      duration = -1.0;
    }
    fprintf(stderr, "%s.%s(%d) -> %s.%s(%d): %f\n",
      call->caller_source,
      call->caller_name,
      call->caller_linedefined,
      call->source,
      call->name,
      call->linedefined,
      duration);
  }
  free(calls);

  /* Return success */
  lua_pushboolean(L, 1);
  return 1;
}

static const luaL_reg prof_funcs[] = {
  { "start", profiler_start },
  { "stop", profiler_stop },
  { NULL, NULL }
};

int luaopen_lprofile(lua_State *L) {
  luaL_openlib(L, "profiler", prof_funcs, 0);
  lua_pushliteral (L, "_COPYRIGHT");
  lua_pushliteral (L, "Copyright (C) 2014 Carlo Cabanilla <carlo.cabanilla@gmail.com>");
  lua_settable (L, -3);
  lua_pushliteral (L, "_DESCRIPTION");
  lua_pushliteral (L, "A Lua profiler");
  lua_settable (L, -3);
  lua_pushliteral (L, "_NAME");
  lua_pushliteral (L, "LProfile");
  lua_settable (L, -3);
  lua_pushliteral (L, "_VERSION");
  lua_pushliteral (L, "0.1");
  lua_settable (L, -3);
  return 1;
}
