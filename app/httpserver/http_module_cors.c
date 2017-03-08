/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Israel Lot <me@israellot.com> wrote this file.
 * As long as you retain this notice you can do whatever you 
 * want with this stuff. If we meet some day, and you think this stuff is 
 * worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */



#include "http_module.h"
#include "http_server_module.h"
#include "http_server_engine.h"
#include "http_server_response.h"
#include "http_parser.h"
#include "c_stdint.h"
#include "c_stddef.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "c_limits.h"
#include "c_stdio.h"
#include "mem.h"
#include "queue.h"




void http_server_cors_on_destroy(http_module *module){

    //any cleanup necessary

}

void http_module_cors_on_headers(struct http_module *module,http_server_engine_connection *c,void **data){

    if(c->request.method_code == HTTP_OPTIONS){

        

        c->response.code=HTTP_OK;
        
        //SET CORS Allow Origin for every request		
        http_engine_response_add_header(c,HTTP_ACCESS_CONTROL_ALLOW_ORIGIN,"*");

        

        http_request_header *allow_headers = http_server_connection_get_header(c,HTTP_ACCESS_CONTROL_REQUEST_HEADERS);
        http_request_header *allow_methods = http_server_connection_get_header(c,HTTP_ACCESS_CONTROL_REQUEST_METHOD);
	
        

		if(allow_headers!=NULL)
			http_engine_response_add_header(c,HTTP_ACCESS_CONTROL_ALLOW_HEADERS,allow_headers->value);
		if(allow_methods!=NULL)
			http_engine_response_add_header(c,HTTP_ACCESS_CONTROL_ALLOW_METHODS,allow_methods->value);;

    }

}

http_module* http_module_cors_new(){

    http_module *module =  http_module_new("HTTP_CORS");    
    http_module_add_url(module,"/*");

    module->self_callbacks.on_destroy = http_server_cors_on_destroy;
    module->process.on_headers = http_module_cors_on_headers;

    http_module_add_header(module,HTTP_ACCESS_CONTROL_REQUEST_HEADERS);
    http_module_add_header(module,HTTP_ACCESS_CONTROL_REQUEST_METHOD);

    return module;


}


