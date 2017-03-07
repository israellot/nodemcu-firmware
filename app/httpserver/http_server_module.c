/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Israel Lot <me@israellot.com> wrote this file.
 * As long as you retain this notice you can do whatever you 
 * want with this stuff. If we meet some day, and you think this stuff is 
 * worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

#include "http_server_module.h"
#include "http_server_engine.h"
#include "c_stdint.h"
#include "c_stddef.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "c_limits.h"
#include "c_stdio.h"
#include "user_interface.h"
#include "espconn.h"
#include "user_config.h"
#include "c_types.h"
#include "user_interface.h"
#include "mem.h"
#include "osapi.h"


void http_module_attach_headers_to_connection(http_server_engine_connection *c,http_module *module){
    
    for(unsigned i=0;i<MAX_HEADERS;i++){
        char *new_header = module->header_list[i];

        if(new_header==NULL)
            return;

        http_server_connection_add_request_header(c,new_header);
    }

}

void http_module_attach_to_connection(http_server_engine_connection *c,http_module *new_module){

    

    for(unsigned int i=0;i<MAX_MODULES;i++){

        http_module *module = c->module_list[i];
        
        if(module==NULL){
            c->module_list[i]=new_module;
            http_module_attach_headers_to_connection(c,new_module);
            //HTTPSERVER_DEBUG("http_module_process module %s added to pipeline of path %s",new_module->module_name,c->request.url.path);
            return;
        }

        if(module==new_module)
            return;

    }

    
}

void http_module_process(http_server_engine_connection *c,unsigned int state){

    //HTTPSERVER_DEBUG("http_module_process state: %d",state);

    http_request *request  = &(c->request);

    http_server_engine *engine = c->server;

    

    if(state == HTTP_REQUEST_ON_URL){
        
        //lets match modules that should run
        
        char *path=request->url.path;

        

        for(unsigned int m=0;m<MAX_MODULES;m++){

            http_module *module = engine->module_list[m];
            
            if(module==NULL)
                break;

            

            for(unsigned int r=0;r<MAX_PATTERNS_MODULE;r++){

                char *pattern=module->route_list[r];     

                

                if(pattern==NULL)
                    break;           

                if (c_strcasecmp(pattern, path)==0 || (pattern[strlen(pattern)-1]=='*' && c_strncasecmp(pattern, path, strlen(pattern)-1)==0) )
               {
                   

                   http_module_attach_to_connection(c,module);

                   break;
               }

            }

        }

        //run modules
        for(unsigned int m=0;m<MAX_MODULES;m++){

            http_module *module = c->module_list[m];

            if(module==NULL)
                break;

            if(module->process.on_url!=NULL){
                module->process.on_url(module,c);
            }

        }

    }
    else if(state == HTTP_REQUEST_ON_HEADERS){

        for(unsigned int m=0;m<MAX_MODULES;m++){

            http_module *module = c->module_list[m];

            if(module==NULL)
                break;

            if(module->process.on_headers!=NULL){
                module->process.on_headers(module,c);
                if(c->response.code!=0) break; //response sent
            }
        }
    }
    else if(state == HTTP_REQUEST_ON_BODY){

        for(unsigned int m=0;m<MAX_MODULES;m++){

            http_module *module = c->module_list[m];

            if(module==NULL)
                break;

            if(module->process.on_body!=NULL){
                module->process.on_body(module,c);
                if(c->response.code!=0) break; //response sent
            }
        }
    }
    else if(state == HTTP_REQUEST_ON_BODY_COMPLETE){

        for(unsigned int m=0;m<MAX_MODULES;m++){

            http_module *module = c->module_list[m];

            if(module==NULL)
                break;

            if(module->process.on_body_complete!=NULL){
                module->process.on_body_complete(module,c);
                if(c->response.code!=0) break; //response sent
            }
        }
    }


}

http_module* http_module_new(char *name){

    HTTPSERVER_DEBUG("http_module_new");

    http_module *module = (http_module*)zalloc(sizeof(http_module));

    module->module_name = name;    
    
    return module;
}

void http_module_attach_to_engine(http_server_engine *engine,http_module *new_module){
    

    for(unsigned int i=0;i<MAX_MODULES;i++){
        http_module *module = engine->module_list[i];

        if(module==new_module)
            return;

        if(module==NULL){
            engine->module_list[i]=new_module;    
            return;
        }
    }
   
    HTTPSERVER_DEBUG("http_module_attach_to_engine max module count reached");
    
}

void http_module_add_header(http_module *module,char *new_header){
    
    
     for(unsigned int i=0;i<MAX_HEADERS;i++){
        char *header = module->header_list[i];
        
            //add
        if(header==NULL){
             module->header_list[i]=new_header;
             break;
             return;
        }

        if(c_strcasecmp(new_header, header)==0)
            return;
    }

    HTTPSERVER_DEBUG("http_module_add_header max header count reached");

    
}

void http_module_add_url(http_module *module,char *new_pattern){
    HTTPSERVER_DEBUG("http_module_add_url %s, %s",module->module_name,new_pattern);
    
    if(*new_pattern!='/'){
        HTTPSERVER_DEBUG("http_module_add_url pattern should start with /");
        return;
    }

     for(unsigned int i=0;i<MAX_PATTERNS_MODULE;i++){
        char *pattern = module->route_list[i];
        
        //add
        if(pattern==NULL){
             HTTPSERVER_DEBUG("http_module_add_url adding to pos %d",i);
             module->route_list[i]=new_pattern;             
             return;
        }

        if(c_strcasecmp(pattern, new_pattern)==0)
            return;
    }

    HTTPSERVER_DEBUG("http_module_add_url max pattern count reached");


}

void http_module_destroy(http_module *module){

    if(module->self_callbacks.on_destroy!=NULL)
        module->self_callbacks.on_destroy(module); //call inner destroy
    
        
    //free root module
    free(module);

}


