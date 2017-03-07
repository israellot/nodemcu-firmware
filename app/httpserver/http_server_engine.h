/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Israel Lot <me@israellot.com> wrote this file.
 * As long as you retain this notice you can do whatever you 
 * want with this stuff. If we meet some day, and you think this stuff is 
 * worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#ifndef __HTTP_SERVER_ENGINE_H
#define __HTTP_SERVER_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

struct http_module;
struct http_server_engine;

#include "queue.h"
#include "http_server_common.h"
#include "http_server_module.h"
#include "http_parser.h"
#include "user_config.h"



typedef struct http_request_header {
	const char *key;
	char *value;	
    unsigned char found;
} http_request_header;

typedef struct http_response_header{
    char *key;
    char *value;
} http_response_header;

typedef struct http_request {
        
        const char *method;
        unsigned int method_code;

        struct url{
            char *data;
            struct http_parser_url url_parsed;            
            char *path;
            char *query;
        } url;

        http_request_header headers[MAX_HEADERS];
        
        struct body{
            char save_flag;
            char *data;
            unsigned int length;
	    } body;	


} http_request;

typedef struct http_response {

        unsigned int code;

        http_response_header headers[MAX_HEADERS]; 

        struct http_response_output{
            char *buffer;
            unsigned int buffer_pos;
            unsigned int buffer_len;
        } output;

        

} http_response;

typedef void (*http_connection_close_delegate)(void *reference);
typedef void (*http_connection_send_data)(void *reference,char *data, unsigned short len);
struct http_server_engine_connection;
struct http_server_engine;

typedef struct http_server_engine_connection{

    struct http_server_engine *server;

    LIST_ENTRY(http_server_engine_connection) list;

    void *reference;

    struct http_parser parser;
	struct http_parser_settings parser_settings;

    http_request request;
    http_response response;

    struct http_module* module_list[MAX_MODULES];

    struct http_module *response_module; //module responsible to sending response

    //callbacks
    http_connection_close_delegate connection_close_callback;
    http_connection_send_data send_data_callback;
    

}http_server_engine_connection;

 
typedef struct http_server_engine{

    LIST_HEAD(connection_list, http_server_engine_connection) connection_list;

    struct http_module* module_list[MAX_MODULES];

    void *reference;
}http_server_engine;

typedef struct http_server_engine http_server_engine;
typedef struct http_server_engine_connection http_server_engine_connection;




http_server_engine* http_server_engine_new();
http_server_engine_connection* http_server_engine_new_connection(http_server_engine *server, void *reference);
void http_server_engine_tcp_received(http_server_engine *server,void *reference,char *buffer,unsigned short len);
void http_server_engine_tcp_ready_to_send(http_server_engine *server,void *reference);
void http_server_engine_tcp_disconnected(http_server_engine *server,void *reference);
void http_server_connection_add_request_header(http_server_engine_connection *connection,char *new_header);
http_request_header* http_server_connection_get_header(http_server_engine_connection *c,char *key);

//parser 
static int parser_on_message_begin(http_parser *parser);
static int parser_on_url(http_parser *parser, const char *url, size_t length);
static int parser_on_status(http_parser *parser, const char *at, size_t length);
static int parser_on_header_field(http_parser *parser, const char *at, size_t length);
static int parser_on_header_value(http_parser *parser, const char *at, size_t length);
static int parser_on_headers_complete(http_parser *parser);
static int parser_on_body(http_parser *parser, const char *at, size_t length);
static int parser_on_message_complete(http_parser *parser);

#ifdef __cplusplus
}
#endif
#endif




