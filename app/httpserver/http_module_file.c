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
#include "vfs.h"



typedef struct http_module_file_data{

    int file_fd;
    unsigned int file_size;
    unsigned int file_pos;
    

}http_module_file_data;

#define CHUNK_SIZE 1400


void http_server_file_on_destroy(http_module *module){

    //any cleanup necessary

}

void http_module_file_on_send_response(struct http_module *module,http_server_engine_connection *c,void **data){

        char file_buffer[CHUNK_SIZE];

        http_module_file_data *module_data=(http_module_file_data*)(*data);

        if(module_data->file_size < CHUNK_SIZE){
            //send all at once
            // char *file_buffer = (char*)malloc(module_data->file_size);
            vfs_read(module_data->file_fd,file_buffer,module_data->file_size);           

            http_engine_response_write_response(c,file_buffer,module_data->file_size);

            vfs_close(module_data->file_fd);
            //free(file_buffer);
            free(*data);
            

            c->response.body_finished=1;

        } else{

            //module_data->buffer = (char*)malloc(CHUNK_SIZE);
            unsigned int read = vfs_read(module_data->file_fd,file_buffer,CHUNK_SIZE);

            http_engine_response_write_response(c,file_buffer,read);

            module_data->file_pos+=read;

            if(vfs_eof(module_data->file_fd)){
                vfs_close(module_data->file_fd);
                //free(module_data->buffer);
                free(*data);
                

                c->response.body_finished=1;
            }

        }

}

void http_module_file_on_headers(struct http_module *module,http_server_engine_connection *c,void **data){

    if(c->request.method_code == HTTP_GET){
                
        char *path = c->request.url.path;

        if(*path=='/')
            path++;

        //try match file
        int file_fd = vfs_open(path, "r");      
       

        if(file_fd){
            //found file
            
            unsigned int file_size =vfs_size(file_fd);

            
            c->response.code=HTTP_OK;
            c->response.content_length=file_size;

            http_module_file_data *module_data=(http_module_file_data*)zalloc(sizeof(http_module_file_data));

            module_data->file_size=file_size;
            module_data->file_fd = file_fd;

            *data=(void*)module_data;

        }
       

    }

}

http_module* http_module_file_new(){

    http_module *module =  http_module_new("HTTP_FILE_SYSTEM");    
    http_module_add_url(module,"/*");

    module->self_callbacks.on_destroy = http_server_file_on_destroy;
    module->process.on_headers = http_module_file_on_headers;
    module->process.on_send_response=http_module_file_on_send_response;

    http_module_add_header(module,HTTP_ACCESS_CONTROL_REQUEST_HEADERS);
    http_module_add_header(module,HTTP_ACCESS_CONTROL_REQUEST_METHOD);

    return module;


}


