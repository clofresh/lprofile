#include "lua.h"
#include "lauxlib.h"

static int profiler_start(lua_State *L) {
  fprintf(stderr, "started!\n");
  lua_pushboolean(L, 1);
  return 1;
}

static int profiler_stop(lua_State *L) {
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
