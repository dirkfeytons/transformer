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

/* Lua wrapper for the syslog C API
 * See syslog(3)
 */
#include <syslog.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"

#include <stdio.h>

typedef struct
{
    const char *name;
    int value;
} ConstantEntry;

#define CONSTANT(c) { #c, c},

static void create_constant_table(lua_State *L, const ConstantEntry *c)
{
    lua_createtable(L, 0, 0);
    for( ; c->name ; c++) {
        lua_pushinteger(L, c->value);
        lua_setfield(L, -2, c->name);
    }
}


/* openlog(ident, option[, facility]) 
 *	ident:		string
 *	option:		int, (see options table)
 *	facility:	int, one of LOG_... (see facility table)
 *
 */
static int l_openlog(lua_State *L)
{
    int option = luaL_checkint(L, 2);
    int facility = luaL_checkint(L, 3);

    /* syslog() needs a pointer to the ident string that must outlive this
     * function call */
    size_t len;
    const char *ident = luaL_checklstring(L, 1, &len);
    /* play safe and keep a copy of ident in a new userdatum, whose contents
     * won't be touched by Lua in any way, but let Lua manage the allocated
     * memory */
    char *identcp = (char *)lua_newuserdata(L, len + 1);
    strcpy(identcp, ident);
    /* store it in our environment table, so that it is not garbage collected */
    lua_setfield(L, LUA_ENVIRONINDEX, "ident");

    openlog(identcp, option, facility);
    return 0;
}

/* syslog(priority, message) 
 *	priority:	string, one of "LOG_..."
 *	message:	string
 *
 */
static int l_syslog(lua_State *L)
{
    int priority = luaL_checkint(L, 1);
    const char *msg = luaL_checkstring(L, 2);

    syslog(priority, "%s", msg);
    return 0;
}

static int do_log(lua_State *L, int priority)
{
    const char *msg = luaL_checkstring(L, 1);
    syslog(priority, "%s", msg);
    return 0;
}

static int l_critical(lua_State *L)
{
    return do_log(L, LOG_CRIT);
}

static int l_error(lua_State *L)
{
    return do_log(L, LOG_ERR);
}

static int l_warning(lua_State *L)
{
    return do_log(L, LOG_WARNING);
}

static int l_notice(lua_State *L)
{
    return do_log(L, LOG_NOTICE);
}

static int l_info(lua_State *L)
{
    return do_log(L, LOG_INFO);
}

static int l_debug(lua_State *L)
{
    return do_log(L, LOG_DEBUG);
}


/* closelog() 
 *
 */
static int l_closelog(lua_State *L)
{
    /* release any memory reserved for ident in a previous call to l_openlog */
    lua_pushnil(L);
    lua_setfield(L, LUA_ENVIRONINDEX, "ident");
    closelog();
    return 0;
}

static const struct luaL_Reg mylib [] = {
    { "openlog", l_openlog },
    { "syslog", l_syslog },
    { "critical", l_critical },
    { "error", l_error},
    { "warning", l_warning },
    { "notice", l_notice },
    { "info", l_info },
    { "debug", l_debug },
    { "closelog", l_closelog },
    { NULL, NULL }
};

static const ConstantEntry options[] = {
    CONSTANT(LOG_CONS)
    CONSTANT(LOG_NDELAY)
    CONSTANT(LOG_NOWAIT)
    CONSTANT(LOG_ODELAY)
    CONSTANT(LOG_PERROR)
    CONSTANT(LOG_PID)
    {NULL, 0}
};

static const ConstantEntry facilities[] = {
    CONSTANT(LOG_AUTH)
    CONSTANT(LOG_AUTHPRIV)
    CONSTANT(LOG_CRON)
    CONSTANT(LOG_DAEMON)
    CONSTANT(LOG_FTP)
    CONSTANT(LOG_KERN)
    CONSTANT(LOG_LOCAL0)
    CONSTANT(LOG_LOCAL1)
    CONSTANT(LOG_LOCAL2)
    CONSTANT(LOG_LOCAL3)
    CONSTANT(LOG_LOCAL4)
    CONSTANT(LOG_LOCAL5)
    CONSTANT(LOG_LOCAL6)
    CONSTANT(LOG_LOCAL7)
    CONSTANT(LOG_LPR)
    CONSTANT(LOG_MAIL)
    CONSTANT(LOG_NEWS)
    CONSTANT(LOG_SYSLOG)
    CONSTANT(LOG_USER)
    CONSTANT(LOG_UUCP)
    {NULL, 0}
};

static const ConstantEntry priorities[] = {
    CONSTANT(LOG_EMERG)
    CONSTANT(LOG_ALERT)
    CONSTANT(LOG_CRIT)
    CONSTANT(LOG_ERR)
    CONSTANT(LOG_WARNING)
    CONSTANT(LOG_NOTICE)
    CONSTANT(LOG_INFO)
    CONSTANT(LOG_DEBUG)
    {NULL, 0}
};


int luaopen_syslog (lua_State *L)
{
    /* create a module environment to keep the ident string */
    lua_newtable(L);
    lua_replace(L, LUA_ENVIRONINDEX);

    lua_createtable(L, 0, 11);
    luaL_register(L, NULL, mylib);

    /* add options used by openlog() options to the lib table */
    create_constant_table(L, options);
    lua_setfield(L, -2, "options");

    /* add constants used by openlog() facility to the lib table */
    create_constant_table(L, facilities);
    lua_setfield(L, -2, "facilities");

    /* add priority constants */
    create_constant_table(L, priorities);
    lua_setfield(L, -2, "priorities");
    return 1;
}

