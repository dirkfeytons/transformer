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


#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "lua.h"
#include "lauxlib.h"

struct async_stats_st {
  unsigned enqueued;
  unsigned dequeued;
  unsigned inqueue;
};

typedef struct list_element_st LIST_ELEMENT_ST, *LIST_ELEMENT;
struct list_element_st {
  LIST_ELEMENT	next;
  char		cmd[];
};

static struct {
  unsigned num_inqueue;
  unsigned num_enqueued;
  unsigned num_dequeued;
  LIST_ELEMENT head;
  LIST_ELEMENT tail;
} async_list = { .num_inqueue = 0, .head = NULL, .tail = NULL };

static struct {
  unsigned queue_size;
  const unsigned max_queue_size;
} config = { .queue_size = 128, .max_queue_size = 1024 };

static pthread_once_t async_is_initialized = PTHREAD_ONCE_INIT;
static pthread_mutex_t async_mutex;

static int async_task_running = 0;

static void async_init (void)
{
  pthread_mutexattr_t attr;

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);

  pthread_mutex_init(&async_mutex, &attr);
}

static inline void async_lock (void)
{
  if (pthread_mutex_lock(&async_mutex)) {
    pthread_exit(NULL);
  }
}

static inline void async_unlock (void)
{
  pthread_mutex_unlock(&async_mutex);
}

static void async_list_enqueue (LIST_ELEMENT e)
{
  e->next = NULL;	/* just to be sure */

  async_lock();
  if (async_list.num_inqueue) {
    async_list.tail->next = e;
  } else {
    async_list.head = e;
  }
  async_list.tail = e;
  async_list.num_inqueue++;
  async_list.num_enqueued++;
  async_unlock();
}

static void async_list_dequeue (LIST_ELEMENT* e)
{
  async_lock();
  *e = async_list.head;
  if (async_list.num_inqueue) {
    async_list.num_inqueue--;
    async_list.num_dequeued++;
    async_list.head = async_list.head->next;
    if (!async_list.num_inqueue) {
      async_list.tail = NULL;
    }
    (*e)->next = NULL;
  }
  async_unlock();
}

static void* execute_task (void* v __attribute__((unused)))
{
  int keep_looping = 1;

  LIST_ELEMENT elem;

  do {
    if (async_list.num_inqueue) {
      int r = system(async_list.head->cmd);
      (void)r;
      async_list_dequeue(&elem);
      free(elem);
    } else {
      async_lock();
      if (!async_list.num_inqueue) {
        keep_looping = 0;
        async_task_running = 0;
      }
      async_unlock();
    }
  } while (keep_looping);

  return NULL;
}

static void start_async_task (void)
{
  pthread_t pid;

  async_lock();
  if (!pthread_create(&pid, 0, execute_task, NULL)) {
    async_task_running = 1;
    pthread_detach(pid);
  }
  async_unlock();
}

static int async_execute (const char *cmd)
{
  LIST_ELEMENT elem;

  pthread_once(&async_is_initialized, async_init);

  if (async_list.num_inqueue >= config.queue_size)
    return -1;

  /* allocate an extra byte for the NULL at the end of cmd */
  elem = (LIST_ELEMENT) malloc(sizeof(*elem) + strlen(cmd) + 1);
  elem->next = NULL;
  strcpy(elem->cmd, cmd);

  async_list_enqueue(elem);

  if (!async_task_running) {
    start_async_task();
  }
  return 0;
}

static int async_queue_size_get (unsigned *size)
{
  *size = config.queue_size;
  return 0;
}

static int async_queue_size_set (unsigned size)
{
  if (!size || size > config.max_queue_size)
    return -1;

  config.queue_size = size;
  return 0;
}

static int async_get_stats (struct async_stats_st *stats)
{
  async_lock();

  stats->enqueued = async_list.num_enqueued;
  stats->dequeued = async_list.num_dequeued;
  stats->inqueue = async_list.num_inqueue;

  async_unlock();
  return 0;
}

static int luaT_execute_cmd (lua_State *L)
{
  const char *cmd;
  size_t len;

  cmd = lua_tolstring(L, -1, &len);

  return (cmd == NULL || !len || len != strlen(cmd)
      || async_execute(cmd))? -1 : 0;
}

static int luaT_execute_list (lua_State *L)
{
  int rv = 0;

  lua_pushnil(L);                 /* push key zero */
  while (lua_next(L, -2) != 0) {
    lua_pop(L, 1);          /* pop value, keep new key */
    if (!lua_isstring(L, -1)
        || luaT_execute_cmd(L)) {
      return -1;
    }
  }

  return rv;
}

static int luaT_execute (lua_State *L)
{
  int rv = -1;

  if (lua_gettop(L) == 1) {
    if (lua_isstring(L, 1))
      rv = luaT_execute_cmd(L);
    else if (lua_istable(L, 1))
      rv = luaT_execute_list(L);
  }

  lua_pushnumber(L, rv);
  return 1;
}

static int luaT_config_get (lua_State *L)
{
  unsigned queue_size;

  async_queue_size_get(&queue_size);

  lua_createtable(L, 0, 1);
  lua_pushstring(L, "queue_size");
  lua_pushnumber(L, queue_size);
  lua_settable(L, -3);
  return 1;
}

static int luaT_config_set (lua_State *L)
{
  int rv = -1;
  unsigned queue_size;

  if (lua_istable(L, 1)) {
    lua_pushstring(L, "queue_size");
    lua_gettable(L, 1);
    queue_size = lua_tointeger(L, -1);
    rv = async_queue_size_set(queue_size);
  }

  lua_pushnumber(L, rv);
  return 1;
}

static int luaT_stats (lua_State *L)
{
  struct async_stats_st stats;

  async_get_stats(&stats);

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

  return 1;
}

int luaopen_lasync (lua_State *L)
{
  static const luaL_reg liblua_tch_async [] = {
      {"execute",     luaT_execute},
      {"config_get",  luaT_config_get},
      {"config_set",  luaT_config_set},
      {"stats",       luaT_stats},
      {NULL, NULL}  /* sentinel */
  };

  lua_createtable(L, 0, sizeof(liblua_tch_async)/sizeof(*liblua_tch_async));
  luaL_register(L, NULL, liblua_tch_async);
  return 1;
}
