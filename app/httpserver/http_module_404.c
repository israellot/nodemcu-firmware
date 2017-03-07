/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Israel Lot <me@israellot.com> wrote this file.
 * As long as you retain this notice you can do whatever you 
 * want with this stuff. If we meet some day, and you think this stuff is 
 * worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */



#include "http_module_default.h"
#include "http_server_module.h"
#include "http_server_engine.h"
#include "http_server_response.h"
#include "c_stdint.h"
#include "c_stddef.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "c_limits.h"
#include "c_stdio.h"
#include "mem.h"
#include "queue.h"


http_module http_module_cors;

void http_module_404_on_destroy(http_module *module){

    //any cleanup necessary

}

void http_module_404_on_body(struct http_module *module,http_server_engine_connection *c){

    if(c->response_module==NULL){
        //no module claimed to respond

        HTTPSERVER_DEBUG("http_module_404_on_body sending response");

        c->response_module=module;

        c->response.code=HTTP_NOT_FOUND;

        http_engine_response_send_no_body(c);

    }

}

http_module* http_module_404_new(){

    HTTPSERVER_DEBUG("http_module_404_new");

    http_module *module =  http_module_new("HTTP_404");    
    http_module_add_url(module,"/*");

    module->self_callbacks.on_destroy = http_module_404_on_destroy;

    module->process.on_body = http_module_404_on_body;
    module->process.on_body_complete = http_module_404_on_body;

    return module;


}


