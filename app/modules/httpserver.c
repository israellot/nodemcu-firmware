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


// Instance function map
static const LUA_REG_TYPE http_instance_map[] = {
  { LSTRKEY( "listen" ),   LFUNCVAL( lua_http_server_instance_listen ) },  
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
  return 0;
}


NODEMCU_MODULE(HTTPSERVER, "httpserver", httpserver_map, httpserver_init);