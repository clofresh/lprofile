#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lua.h"
#include "lauxlib.h"

const char* C_SOURCE = "=[C]";

typedef struct scall {
  clock_t incl_start_time;
  clock_t incl_end_time;
  clock_t excl_start_time;
  int excl_duration;
  const char *caller_source;
  const char *caller_name;
  int caller_linedefined;
  const char *source;
  const char *name;
  int line;
  int linedefined;
} CALL;

FILE* f;
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
  clock_t now = clock();

  if (!ar->event) {
    lua_getinfo(L, "nS", ar);
    /* Entering a function */
    CALL call;
    CALL *prev_top = NULL;
    if (num_call_stack > 0) {
      prev_top = &calls[call_stack[num_call_stack - 1]];
      if (prev_top->excl_start_time != 0) {
        prev_top->excl_duration += now - prev_top->excl_start_time;
        prev_top->excl_start_time = 0;
      }
    }

    /* Get info on the caller of this function */
    lua_Debug previous_ar;
    if (lua_getstack(L, 1, &previous_ar) == 0) {
      call.caller_name = "(top)";
      call.caller_source = "(top)";
      call.caller_linedefined = -1;
      call.line = -1;
    } else {
      lua_getinfo(L, "nSl", &previous_ar);
      if (previous_ar.name == NULL) {
        call.caller_name = "(null)";
      } else {
        call.caller_name = previous_ar.name;
      }
      if (previous_ar.source == NULL) {
        call.caller_source = "(null)";
      } else {
        call.caller_source = previous_ar.source;
      }
      call.caller_linedefined = previous_ar.linedefined;
      call.line = previous_ar.currentline;
    }

    /* Populate the call info */
    call.incl_start_time = now;
    call.incl_end_time = 0;
    call.excl_start_time = now;
    if (ar->source == NULL) {
      call.source = "(null)";
    } else {
      call.source = ar->source;
    }
    if (ar->name == NULL) {
      call.name = "(null)";
    } else {
      call.name = ar->name;
    }
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
    call->incl_end_time = now;
    num_call_stack = num_call_stack - 1;

    if (num_call_stack > 0) {
      call = &calls[call_stack[num_call_stack - 1]];
      call->excl_start_time = now;
    }
  }

}

static int profiler_start(lua_State *L) {
  /* Initialize the profiler state */


  const char* outfile;
  if(lua_gettop(L) >= 1) {
    outfile = luaL_checkstring(L, 1);
  } else {
    lua_pushboolean(L, 0);
    return 1;
  }

  f = fopen(outfile, "w");
  if (f == NULL) {
    lua_pushboolean(L, 0);
    return 1;
  }

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

static int cmpcalls(const void *p1, const void *p2) {
  int cmp;
  CALL *c1 = (CALL *)p1;
  CALL *c2 = (CALL *)p2;
  cmp = strcmp(c1->caller_name, c2->caller_name);
  if (cmp == 0) {
    cmp = strcmp(c1->caller_source, c2->caller_source);
    if (cmp == 0) {
      cmp = strcmp(c1->source, c2->source);
      if (cmp == 0) {
        cmp = strcmp(c1->name, c2->name);
      }
    }
  }
  return cmp;
}

static int profiler_stop(lua_State *L) {

  CALL *call;
  int incl_duration, count, linedefined, line;
  const char *caller_source = NULL;
  const char *caller_name = NULL;
  const char *source = NULL;
  const char *name = NULL;

  /* Deregister the callhook */
  lua_sethook(L, (lua_Hook)callhook, 0, 0);

  /* Deallocate the call_stack */
  free(call_stack);

  /* Output the calls */
  qsort(calls, num_calls, sizeof(CALL), cmpcalls);

  fprintf(f, "events: Time\n\n");
  for (int i = 0; i < num_calls; i ++) {
    call = &calls[i];

    if (caller_source == NULL || caller_name == NULL || source == NULL || name == NULL) {

      caller_source = call->caller_source;
      caller_name = call->caller_name;
      source = call->source;
      name = call->name;
      linedefined = call->linedefined;
      line = call->line;
      incl_duration = 0;
      count = 0;

      fprintf(f, "\nfl=%s\nfn=%s\n", caller_source, caller_name);

    } else if (strcmp(caller_source, call->caller_source) != 0 || strcmp(caller_name, call->caller_name) != 0) {

      fprintf(f, "cfi=%s\ncfn=%s\ncalls=%d %d\n%d %d\n",
        source,
        name,
        count,
        linedefined,
        line,
        incl_duration);

      caller_source = call->caller_source;
      caller_name = call->caller_name;
      source = call->source;
      name = call->name;
      linedefined = call->linedefined;
      line = call->line;
      incl_duration = 0;
      count = 0;

      fprintf(f, "\nfl=%s\nfn=%s\n", caller_source, caller_name);

    } else if (strcmp(source, call->source) != 0 || strcmp(name, call->name) != 0) {

      fprintf(f, "cfi=%s\ncfn=%s\ncalls=%d %d\n%d %d\n",
        source,
        name,
        count,
        linedefined,
        line,
        incl_duration);

      source = call->source;
      name = call->name;
      linedefined = call->linedefined;
      line = call->line;
      incl_duration = 0;
      count = 0;
    }
    fprintf(f, "%d %d\n", call->line, call->excl_duration);

    if (call->incl_end_time != 0) {
      incl_duration += (int)(call->incl_end_time - call->incl_start_time);
    }
    count += 1;
  }
  if (source != NULL && name != NULL) {
    fprintf(f, "cfi=%s\ncfn=%s\ncalls=%d %d\n%d %d\n",
    source,
    name,
    count,
    linedefined,
    line,
    incl_duration);
  }

  free(calls);

  fclose(f);
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
