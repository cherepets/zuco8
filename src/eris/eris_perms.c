#include <stddef.h>

#include "../z8lua/lua.h"
#include "../z8lua/lauxlib.h"
#include "../z8lua/lstate.h"

static int noop(lua_State *L) {
  (void)L;
  return 0;
}

static void pushlibfunc(lua_State *L, const char *lib, const char *name) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_getfield(L, -1, lib);
  lua_remove(L, -2);
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, name);
    lua_remove(L, -2);
  }
}

static lua_CFunction resultfunc(lua_State *L, const char *lib,
                                const char *name, int nargs) {
  lua_CFunction f = NULL;
  pushlibfunc(L, lib, name);
  lua_insert(L, -1 - nargs);
  if (!lua_isfunction(L, -1 - nargs)) {
    lua_pop(L, 1 + nargs);
    return NULL;
  }
  if (lua_pcall(L, nargs, 1, 0) == LUA_OK) {
    f = lua_tocfunction(L, -1);
  }
  lua_pop(L, 1);
  return f;
}

static lua_CFunction pcallcont(lua_State *L) {
  lua_CFunction k = NULL;
  lua_State *co = lua_newthread(L);
  pushlibfunc(co, "_G", "pcall");
  pushlibfunc(co, "coroutine", "yield");
  if (lua_resume(co, L, 1) == LUA_YIELD) {
    k = co->ci->previous->u.c.k;
  }
  lua_pop(L, 1);
  return k;
}

static void perm(lua_State *L, int forUnpersist, lua_CFunction f,
                 const char *name) {
  if (f == NULL) {
    return;
  }
  if (forUnpersist) {
    lua_pushstring(L, name);
    lua_pushcfunction(L, f);
  }
  else {
    lua_pushcfunction(L, f);
    lua_pushstring(L, name);
  }
  lua_rawset(L, -3);
}

void eris_permbaselib(lua_State *L, int forUnpersist) {
  lua_CFunction f;
  luaL_checkstack(L, 5, NULL);

  perm(L, forUnpersist, pcallcont(L), "__eris.baselib_pcallcont");

  pushlibfunc(L, "_G", "next");
  f = lua_tocfunction(L, -1);
  lua_pop(L, 1);
  perm(L, forUnpersist, f, "__eris.baselib_luaB_next");

  lua_newtable(L);
  perm(L, forUnpersist, resultfunc(L, "_G", "ipairs", 1),
       "__eris.baselib_ipairsaux");
}

void eris_permcorolib(lua_State *L, int forUnpersist) {
  luaL_checkstack(L, 5, NULL);
  lua_pushcfunction(L, noop);
  perm(L, forUnpersist, resultfunc(L, "coroutine", "wrap", 1),
       "__eris.corolib_luaB_auxwrap");
}

void eris_permstrlib(lua_State *L, int forUnpersist) {
  luaL_checkstack(L, 5, NULL);
  lua_pushliteral(L, "");
  lua_pushliteral(L, "");
  perm(L, forUnpersist, resultfunc(L, "string", "gmatch", 2),
       "__eris.strlib_gmatch_aux");
}
