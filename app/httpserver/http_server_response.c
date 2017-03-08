/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Israel Lot <me@israellot.com> wrote this file.
 * As long as you retain this notice you can do whatever you 
 * want with this stuff. If we meet some day, and you think this stuff is 
 * worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

#include "http_server_common.h"
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


//Struct to keep extension->mime data in
typedef struct {
	unsigned int code;
	const char *text;
} http_code;////The mappings from file extensions to mime types. If you need an extra mime type,
//add it here.
static const http_code http_code_map[]={
	{200, "OK"},
	{204, "No Content"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {304, "Not Modified"},
	{400, "Bad Request"},
	{404, "Not Found"},
	{500, "Internal Server Error"},
	{0, ""}, //default value
};

static const char  *translate_code(unsigned int code){

    unsigned int i=0;

    while(http_code_map[i].code!=0){

        if(http_code_map[i].code==code)
            break;
            
        i++;
    }

    return http_code_map[i].text;

}

void http_engine_response_add_header(http_server_engine_connection *c,char *key,char *value){

    http_response *response = &(c->response);
    
    for(unsigned int i=0;i<MAX_HEADERS;i++){

        http_response_header *header = &(response->headers[i]);

        if(header->key==NULL){
            //empty position - add headers

            unsigned int keyLen = strlen(key);
            unsigned int valueLen = strlen(value);

            char *buffer = (char*)malloc(keyLen+valueLen+2);

            memcpy(buffer,key,keyLen);
            buffer[keyLen]=0;//null terminate key
            header->key=buffer;

            memcpy(buffer+keyLen+1,value,valueLen);
            buffer[keyLen+valueLen+1]=0; //null terminate value
            header->value=buffer+keyLen+1;

            return;
        }

    }

    HTTPSERVER_DEBUG("http_engine_response_add_header : max header count reached");

}

void http_engine_response_clear_headers(http_server_engine_connection *c){

    http_response *response = &(c->response);

    for(unsigned int i=0;i<MAX_HEADERS;i++){

        http_response_header *header = &(response->headers[i]);

        if(header->key!=NULL){
            free(header->key);
            header->key=NULL;
        }
        else{
            break;
        }

    }

}

void http_engine_response_sent_to_tcp(http_server_engine_connection *c,char *buffer,unsigned int len){
    
    if(c->send_data_callback!=NULL){
       
            c->send_data_callback(c->reference,buffer,len);
            
    }
    else{
        HTTPSERVER_DEBUG("http_engine_response_write_tcp_direct no callback");
    }

    
}


static void http_engine_response_write_output(http_server_engine_connection *c,const char *data,unsigned int len){

    http_response *r = &(c->response);

    if(r->output.buffer==NULL){
        
        r->output.buffer = (char*)malloc(HTTP_MAX_TCP_CHUNK);
        r->output.buffer_pos=0;
        r->output.buffer_len=HTTP_MAX_TCP_CHUNK;
        
    }

    if(len==NULL_TERMINATED_STRING){
        
        char *buffer = &(r->output.buffer[r->output.buffer_pos]);
        char *buffer_init = buffer;

        while(*data!=0){

            if((r->output.buffer_len - r->output.buffer_pos) == 0){
                //resize the buffer
                unsigned int new_size = r->output.buffer_len + HTTP_MAX_TCP_CHUNK/2;
                r->output.buffer=realloc(r->output.buffer,new_size);
                r->output.buffer_len=new_size;
                
            }

            *buffer=*data;            
            data++;
            buffer++;

        }
        r->output.buffer_pos += (buffer-buffer_init);

    }
    else if(len>0){

        if((r->output.buffer_len - r->output.buffer_pos) < len){

            //resize the buffer
            unsigned int new_size = r->output.buffer_len + HTTP_MAX_TCP_CHUNK*((len/(HTTP_MAX_TCP_CHUNK/2))+1);

            r->output.buffer=realloc(r->output.buffer,new_size);
            r->output.buffer_len=new_size;

        }

        char *buffer = &(r->output.buffer[r->output.buffer_pos]);

        memcpy(buffer,data,len);
        
        r->output.buffer_pos+=len;

    }

}

#define SEND_NEW_LINE(connection) do{ \
     http_engine_response_write_output(connection,"\r\n",2);\
}while(0)

#define SEND_HEADER(connection,key,value) do{ \
    http_engine_response_write_output(c,key,NULL_TERMINATED_STRING);\
    http_engine_response_write_output(c,": ",NULL_TERMINATED_STRING); \
    http_engine_response_write_output(c,value,NULL_TERMINATED_STRING);\
    SEND_NEW_LINE(c);                                                   \
}while(0)

static int ui2a(unsigned int num, char *bf)
{
    int n = 0,base=10,uc=0;
    unsigned int d = 1;
    int len = 1;
    while (num / d >= base)
    {
        d *= base;
        len ++;
    }
    while (d != 0)
    {
        int dgt = num / d;
        num %= d;
        d /= base;
        if (n || dgt > 0 || d == 0)
        {
            *bf++ = dgt + (dgt < 10 ? '0' : (uc ? 'A' : 'a') - 10);
            ++n;
        }
    }
    *bf = 0;
    return len;
}

void http_engine_response_send_headers(http_server_engine_connection *c){
    
    http_response *response = &(c->response);

    char code[16];
    memset(&code[0],0,16);
    ui2a(response->code,code);
   
    
    //SEND STATUS
    http_engine_response_write_output(c,"HTTP/"HTTP_VERSION" ",NULL_TERMINATED_STRING);
    
    http_engine_response_write_output(c,code,NULL_TERMINATED_STRING);

    http_engine_response_write_output(c,translate_code(response->code),NULL_TERMINATED_STRING);
    SEND_NEW_LINE(c);

    

    //send fixed headers    
     SEND_HEADER(c,HTTP_SERVER,HTTP_DEFAULT_SERVER); 
     SEND_HEADER(c,HTTP_CONNECTION,HTTP_CONNECTION_CLOSE);     

    //SEND HEADERS
    for(unsigned int i=0;i<MAX_HEADERS;i++){

        http_response_header *header = &(response->headers[i]);

        if(header->key==NULL){
             break;
        }
        SEND_HEADER(c,header->key,header->value);         
    }

    

    if(response->content_length>0){
        //set content length
        char content_length[16];
        ui2a(response->content_length,content_length);        
        SEND_HEADER(c,HTTP_CONTENT_LENGTH,content_length);
    }

    //set content type
    if(response->content_type==NULL) 
        response->content_type=HTTP_TEXT_CONTENT;

     SEND_HEADER(c,HTTP_CONTENT_TYPE,response->content_type); 
    
     SEND_NEW_LINE(c); // header | body separation

     

     http_engine_response_sent_to_tcp(c,response->output.buffer,response->output.buffer_pos);

}


void http_engine_response_write_response(http_server_engine_connection *c,char *buffer,unsigned int len){

    //allocate exact size
    if(c->response.output.buffer==NULL){
        c->response.output.buffer=(char*)malloc(len);
        c->response.output.buffer_pos=0;
        c->response.output.buffer_len=len;
    }

    http_engine_response_write_output(c,buffer,len);
    http_engine_response_sent_to_tcp(c,c->response.output.buffer,c->response.output.buffer_pos);
}

void http_engine_response_send_response(http_server_engine_connection *c){
   
    http_response *response = &(c->response);

    if(!response->headers_sent){
       
         http_engine_response_send_headers(c);
         response->headers_sent=1;
    }
    else{
        
        //clean headers
        http_engine_response_clear_headers(c);

        //free the output buffer as soon as possible
        if(c->response.output.buffer!=NULL){
            
            free(c->response.output.buffer);
            c->response.output.buffer=NULL;
        }

        //invoke responding module
        if(c->response_module->module->process.on_send_response!=NULL){
           
            c->response_module->module->process.on_send_response(c->response_module->module,c,&c->response_module->data);
        }
        else{
           
            c->response.body_finished=1;
        }
    }


}

