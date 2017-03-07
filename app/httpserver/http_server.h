/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Israel Lot <me@israellot.com> wrote this file.
 * As long as you retain this notice you can do whatever you 
 * want with this stuff. If we meet some day, and you think this stuff is 
 * worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#ifndef __HTTP_SERVER_H
#define __HTTP_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "http_server_engine.h"
#include "user_interface.h"
#include "espconn.h"
#include "queue.h"






typedef enum listen_interface{
    ST =1,
    SOFTAP=2,
    BOTH=3
} listen_interface;

typedef struct http_server_instance{

    LIST_ENTRY(http_server_instance) list;

    
    http_server_engine* engine;

    //Listening connection data
	struct espconn server_conn;
	esp_tcp server_tcp;

}http_server_instance;

typedef struct esp_tcp_connection{
    LIST_ENTRY(esp_tcp_connection) list;

    uint32_t remote_ip;
    uint32_t local_ip;
    uint32_t remote_port;
    uint32_t local_port;
    struct espconn *conn;
    uint8_t disconnected;

    struct tcp_data{
        char *buffer;
        unsigned int buffer_len;
        
    } tcp_data;

}esp_tcp_connection;

typedef LIST_HEAD(http_server_instance_list, http_server_instance) http_server_instance_list;
typedef LIST_HEAD(esp_tcp_connection_list, esp_tcp_connection) esp_tcp_connection_list;

//public interface
http_server_instance *http_server_new();
int http_server_listen(http_server_instance* server,listen_interface interface,unsigned int port);

//This are the functions that need to be implemented by port

#ifdef __cplusplus
}
#endif
#endif