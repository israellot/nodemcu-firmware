/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Israel Lot <me@israellot.com> wrote this file.
 * As long as you retain this notice you can do whatever you 
 * want with this stuff. If we meet some day, and you think this stuff is 
 * worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */
#include <c_stdlib.h>
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "http_server.h"
#include "http_server_engine.h"
#include "http_server_module.h"
#include "http_module.h"

#define HTTPSERVERL_DEBUG(fmt,...) HTTPSERVER_DEBUG("Lua "#fmt, ##__VA_ARGS__)

typedef struct instance_userdata{

    http_server_instance *server;

}instance_userdata;

static int lua_http_server_new( lua_State* L )
{
    HTTPSERVERL_DEBUG("http_server_new()");

    const char* expected_usage = "http_server_new usage: http_server_new()";
    if (lua_gettop(L) != 0) {
        //wrong number of arguments
        return luaL_error(L,expected_usage);
    }

    //create user data object
    instance_userdata *data = (instance_userdata*)lua_newuserdata(L, sizeof(instance_userdata));
    // set its metatable
    luaL_getmetatable(L, "httpserver.instance");
    lua_setmetatable(L, -2);

    data->server = http_server_new();

    return 1;
}



static int lua_http_server_instance_destroy( lua_State* L ){

    HTTPSERVERL_DEBUG("lua_http_server_instance_destroy()");

    instance_userdata *data = (instance_userdata*)luaL_checkudata(L, 1, "httpserver.instance");
    luaL_argcheck(L, data, 1, "httpserver.instance expected");
    if(data==NULL){
        NODE_DBG("userdata is nil.\n");
        return 0;
    }

}

static int lua_http_server_connection_count(lua_State *L){

    HTTPSERVERL_DEBUG("lua_http_server_instance_listen()");

    instance_userdata *data = (instance_userdata*)luaL_checkudata(L, 1, "httpserver.instance");
    luaL_argcheck(L, data, 1, "httpserver.instance expected");
    if(data==NULL){
        NODE_DBG("userdata is nil.\n");
        return 0;
    }

    unsigned int count= http_server_connection_count(data->server);

    lua_pushinteger( L, count );

    return 1;

}

static int lua_http_server_use_filesystem(lua_State* L){

    HTTPSERVERL_DEBUG("lua_http_server_use_filesystem()");

    instance_userdata *data = (instance_userdata*)luaL_checkudata(L, 1, "httpserver.instance");
    luaL_argcheck(L, data, 1, "httpserver.instance expected");
    if(data==NULL){
        NODE_DBG("userdata is nil.\n");
        return 0;
    }

    int length;
    const char * prefix=NULL;
    const char * folder=NULL;

     if (lua_isstring(L, 2))
    {
        prefix     = luaL_checklstring(L, 2, &length);
    }
    if (lua_isstring(L, 3))
    {
        folder     = luaL_checklstring(L, 3, &length);
    }

    http_module *module = http_module_file_new(prefix,folder);
    http_module_attach_to_engine(data->server->engine,module);

    //create user data object
    http_module **module_data = (http_module **)lua_newuserdata(L, sizeof(http_module *));
   
    // set its metatable
    luaL_getmetatable(L, "httpmodule.filesystem");
    lua_setmetatable(L, -2);

    *module_data=module;

    return 1;


}

static int lua_http_server_instance_listen(lua_State* L){

    HTTPSERVERL_DEBUG("lua_http_server_instance_listen()");

    instance_userdata *data = (instance_userdata*)luaL_checkudata(L, 1, "httpserver.instance");
    luaL_argcheck(L, data, 1, "httpserver.instance expected");
    if(data==NULL){
        NODE_DBG("userdata is nil.\n");
        return 0;
    }

    unsigned port = luaL_checkinteger( L, 2 );
    luaL_argcheck(L, port>0 , 2, "Invalid port");

    HTTPSERVERL_DEBUG("lua_http_server_instance_listen port %d",port);

    http_server_listen(data->server,3,port);

    return 0;
}



// File system module function map
static const LUA_REG_TYPE http_module_filesystem_map[] = {
  
  { LSTRKEY( "__index" ),   LROVAL( http_module_filesystem_map ) },
  { LNILKEY, LNILVAL }
};



// Instance function map
static const LUA_REG_TYPE http_instance_map[] = {
  { LSTRKEY( "use_filesystem" ),   LFUNCVAL( lua_http_server_use_filesystem ) },
  { LSTRKEY( "listen" ),   LFUNCVAL( lua_http_server_instance_listen ) },  
  { LSTRKEY("connection_count"),  LFUNCVAL(lua_http_server_connection_count)},
  { LSTRKEY( "__gc" ),      LFUNCVAL( lua_http_server_instance_destroy ) },
  { LSTRKEY( "__index" ),   LROVAL( http_instance_map ) },
  { LNILKEY, LNILVAL }
};

// Module function map
static const LUA_REG_TYPE httpserver_map[] = {
  { LSTRKEY( "new" ),             LFUNCVAL( lua_http_server_new ) },
  
  { LNILKEY, LNILVAL }
};

int httpserver_init( lua_State *L )
{
  luaL_rometatable(L, "httpserver.instance", (void *)http_instance_map);  // httpserver.instance
  luaL_rometatable(L, "httpmodule.filesystem", (void *)http_module_filesystem_map);  
  return 0;
}


NODEMCU_MODULE(HTTPSERVER, "httpserver", httpserver_map, httpserver_init);