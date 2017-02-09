/*
 * Copyright (c) 2016 Technicolor Delivery Technologies, SAS
 *
 * The source code form of this Transformer component is subject
 * to the terms of the Clear BSD license.
 *
 * You can redistribute it and/or modify it under the terms of the
 * Clear BSD License (http://directory.fsf.org/wiki/License:ClearBSD)
 *
 * See LICENSE file for more details.
 */

#include <string.h>

#include "lua.h"
#include "lauxlib.h"

#include "async.h"

static struct queue* get_async_queue(lua_State *L)
{
  struct queue *queue;
  /* the pointer to the queue is stored in a light userdata in the Lua
   * registry. The key used is the address of this function.
   */
  lua_pushlightuserdata(L, get_async_queue);
  lua_gettable(L, LUA_REGISTRYINDEX);
  queue = (struct queue*)lua_touserdata(L, -1);
  /* pop the result of the registry query off the stack to make this function
   * stack neutral
   */
  lua_pop(L, 1);
  if( !queue ) {
    /* the queue was not created yet in this lua_State, do it now */
    queue = async_create_queue();
    /* put it in the Lua registry */
    lua_pushlightuserdata(L, get_async_queue);
    lua_pushlightuserdata(L, queue);
    lua_settable(L, LUA_REGISTRYINDEX);
  }
  return queue;
}

static bool execute_cmd (lua_State *L)
{
  const char *cmd;
  size_t len;

  cmd = lua_tolstring(L, -1, &len);

  if( cmd == NULL || !len || len != strlen(cmd) ) {
    /* not a string, empty string, or embedded NULs */
    return false;
  }
  return async_execute(get_async_queue(L), cmd);
}

static bool execute_list (lua_State *L)
{
  lua_pushnil(L);                 /* push key zero */
  while (lua_next(L, -2) != 0) {
    lua_pop(L, 1);          /* pop value, keep new key */
    if (!lua_isstring(L, -1)
        || !execute_cmd(L)) {
      return false;
    }
  }
  return true;
}

static int luaT_execute (lua_State *L)
{
  bool rv = false;

  if (lua_gettop(L) == 1) {
    if (lua_isstring(L, 1))
      rv = execute_cmd(L);
    else if (lua_istable(L, 1))
      rv = execute_list(L);
  }

  lua_pushboolean(L, rv);
  return 1;
}

static int luaT_stats (lua_State *L)
{
  struct queue_stats stats;

  if( async_get_stats(get_async_queue(L), &stats) ) {
    lua_createtable(L, 0, 3);
    lua_pushstring(L, "enqueued");
    lua_pushnumber(L, stats.enqueued);
    lua_settable(L, -3);
    lua_pushstring(L, "dequeued");
    lua_pushnumber(L, stats.dequeued);
    lua_settable(L, -3);
    lua_pushstring(L, "inqueue");
    lua_pushnumber(L, stats.inqueue);
    lua_settable(L, -3);
  }
  else {
    lua_pushnil(L);
  }
  return 1;
}

static int luaT_timeout(lua_State *L)
{
  int timeout;
  if( lua_isnoneornil(L, 1) ) {
    timeout = -1; //get only
  }
  else {
    timeout = luaL_checkinteger(L, 1);
  }

  timeout = async_exec_timeout(get_async_queue(L), timeout);

  lua_pushinteger(L, timeout);
  return 1;
}

__attribute__((visibility("default")))
int luaopen_lasync (lua_State *L)
{
  static const luaL_reg liblua_tch_async [] = {
      {"execute",     luaT_execute},
      {"stats",       luaT_stats},
      {"timeout",     luaT_timeout},
      {NULL, NULL}  /* sentinel */
  };

  lua_createtable(L, 0, sizeof(liblua_tch_async)/sizeof(*liblua_tch_async));
  luaL_register(L, NULL, liblua_tch_async);
  return 1;
}
