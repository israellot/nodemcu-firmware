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
#include "http_server_engine.h"

typedef struct instance_userdata{

    http_server_engine *engine;

}instance_userdata;

static int http_server_new( lua_State* L )
{
    HTTPSERVER_DEBUG("LUA http_server_new()")

    const char* expected_usage = "http_server_new usage: http_server_new()"
    if (lua_gettop(L) != 0) {
        //wrong number of arguments
        return luaL_error(L,expected_usage);
    }

    //create user data object
    http_server_engine *engine = (http_server_engine*)lua_newuserdata(L, sizeof(http_server_engine));
    // set its metatable
    luaL_getmetatable(L, "httpserver.instance");
    lua_setmetatable(L, -2);
}

static int http_server_instance_destroy( lua_State* L ){

    http_server_engine *engine = (http_server_engine*)luaL_checkudata(L, 1, "httpserver.instance");
    luaL_argcheck(L, engine, 1, "httpserver.instance expected");
    if(engine==NULL){
        NODE_DBG("userdata is nil.\n");
        return 0;
    }

}

static int http_server_instance_listen(lust_State* L){

    http_server_engine *engine = (http_server_engine*)luaL_checkudata(L, 1, "httpserver.instance");
    luaL_argcheck(L, engine, 1, "httpserver.instance expected");
    if(engine==NULL){
        NODE_DBG("userdata is nil.\n");
        return 0;
    }

    

}


// Instance function map
static const LUA_REG_TYPE http_instance_map[] = {
  { LSTRKEY( "listen" ),   LFUNCVAL( http_server_instance_listen ) },  
  { LSTRKEY( "__gc" ),      LFUNCVAL( http_server_instance_destroy ) },
  { LSTRKEY( "__index" ),   LROVAL( http_instance_map ) },
  { LNILKEY, LNILVAL }
};

// Module function map
static const LUA_REG_TYPE httpserver_map[] = {
  { LSTRKEY( "new" ),             LFUNCVAL( http_server_new ) },
  
  { LNILKEY, LNILVAL }
};

int httpserver_init( lua_State *L )
{
  luaL_rometatable(L, "httpserver.instance", (void *)http_instance_map);  // httpserver.instance
  return 0;
}


NODEMCU_MODULE(HTTP, "httpserver", httpserver_map, httpserver_init);