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

#include "queue.h"
#include "http_parser.h"

#define MAX_HEADERS 10



static const char log_prefix[] = "HTTP Server: ";

#if defined(DEVELOP_VERSION)
    #ifndef HTTPSERVER_DEBUG_ON
        #define HTTPSERVER_DEBUG_ON 1
    #endif    
#else
    #ifndef HTTPSERVER_DEBUG_ON
        #define HTTPSERVER_DEBUG_ON 0
    #endif
#endif
#if HTTPSERVER_DEBUG_ON == 1
  #define HTTPSERVER_DEBUG(format, ...) dbg_printf("%s"format"\n", log_prefix, ##__VA_ARGS__)  
#else
  #define HTTPSERVER_DEBUG(...)
#endif
#if defined(NODE_ERROR)
  #define HTTPSERVER_ERR(format, ...) NODE_ERR("%s"format"\n", log_prefix, ##__VA_ARGS__)
#else
  #define HTTPSERVER_ERR(...)
#endif

typedef struct {
	const char * key;
	char * value;	
    unsigned char found;
} header;

typedef struct http_server_tcp_connection{
    LIST_ENTRY(http_server_tcp_connection) list;

    void *reference;

    struct http_parser parser;
	struct http_parser_settings parser_settings;

    struct request {
        char *url;
	    struct http_parser_url url_parsed;
        header headers[MAX_HEADERS];

        
        struct {
            char saveFlag;
            char *data;
            unsigned int lenght;
	    } body;	


    } request;

    

} http_server_tcp_connection;


typedef LIST_HEAD(connection_list, http_server_tcp_connection) connection_list;
typedef struct http_server_engine{

    connection_list connections;

    void *reference;
} http_server_engine;





http_server_engine* http_server_engine_new();
void http_server_engine_new_connection(http_server_engine *server, void *reference);
void http_server_engine_tcp_received(http_server_engine *server,void *reference,char *buffer,unsigned short len);



#ifdef __cplusplus
}
#endif
#endif




