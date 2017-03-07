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
#include "http_parser.h"
#include "http_server.h"
#include "c_stdint.h"
#include "c_stddef.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "c_limits.h"
#include "c_stdio.h"
#include "mem.h"
#include "queue.h"
#include "user_interface.h"
#include "task/task.h"

static void http_server_connection_close_callback_task(task_param_t flag, uint8 priority);

static http_server_instance_list* server_list;
static esp_tcp_connection_list* esp_connection_list;

static task_handle_t connection_close_callback_task_signal;

static int globalInit=0;

static void http_server_global_init(){

    if(!globalInit){
        connection_close_callback_task_signal = task_get_id(http_server_connection_close_callback_task);
    }

}

static esp_tcp_connection* find_connection(struct espconn *conn){

    uint32_t *remote_ip = (uint32_t*)(&(conn->proto.tcp->remote_ip));
    uint32_t remote_port = (uint32_t)conn->proto.tcp->remote_port;

    uint32_t *local_ip = (uint32_t*)(&(conn->proto.tcp->local_ip));
    uint32_t local_port = (uint32_t)conn->proto.tcp->local_port;

    esp_tcp_connection *connection;

    LIST_FOREACH(connection,esp_connection_list,list){
		if(connection->remote_ip==*remote_ip && connection->remote_port==remote_port)
        {
            connection->conn=conn;

            return connection;
        }
	}

    HTTPSERVER_DEBUG("find_connection. Insert on list Remote IP: %d.%d.%d.%d , port : %ld",remote_ip[0],remote_ip[1],remote_ip[2],remote_ip[3],remote_port);

    //couldn't find, so create
    connection = (esp_tcp_connection*)malloc(sizeof(esp_tcp_connection));
    connection->remote_port = remote_port;
    connection->remote_ip = (*remote_ip);
    connection->local_port = local_port;
    connection->local_ip = (*local_ip);
    connection->conn =conn;

    LIST_INSERT_HEAD(esp_connection_list,connection,list);

    return connection;

}

static http_server_instance* find_server(esp_tcp_connection *tcp_conn){
    
    http_server_instance *instance;
	LIST_FOREACH(instance,server_list,list){
		if(instance->server_tcp.local_port==tcp_conn->local_port)
			return instance;
	}


	return NULL;

}

static void http_server_esp_data_sent_callback(void *arg);

static void http_server_esp_send_data(esp_tcp_connection *tcp_conn){

    struct espconn *esp_conn =tcp_conn->conn;

    

    if(tcp_conn->tcp_data.buffer_len>HTTP_MAX_TCP_CHUNK){
        //send partial data

        espconn_send(esp_conn,tcp_conn->tcp_data.buffer,HTTP_MAX_TCP_CHUNK);

        tcp_conn->tcp_data.buffer+=HTTP_MAX_TCP_CHUNK;
        tcp_conn->tcp_data.buffer_len -= HTTP_MAX_TCP_CHUNK;       

        
    }else{
        espconn_send(esp_conn,tcp_conn->tcp_data.buffer,tcp_conn->tcp_data.buffer_len);
        tcp_conn->tcp_data.buffer=NULL;
        tcp_conn->tcp_data.buffer_len = 0;
        
    }
    
}

/* 
 *   This callback is called by the SDK when a connection is ready to send more data
 */
static void http_server_esp_data_sent_callback(void *arg){

    struct espconn *esp_conn = (void*)arg;

    //find the connection reference
    esp_tcp_connection *tcp_conn = find_connection(esp_conn);


    if(tcp_conn->tcp_data.buffer!=NULL){
        //send more data
        http_server_esp_send_data(tcp_conn);
    }
    else{
        //callback engine

         //find the server
        http_server_instance *server_instance = find_server(tcp_conn);

        //forwar to engine
        http_server_engine_tcp_ready_to_send(server_instance->engine,tcp_conn);
    }   

}

/* 
 *   This callback is called by the ENGINE when a connection should send data
 */
static void http_server_send_data_callback(void *reference, char *data, unsigned short len) {

    esp_tcp_connection *tcp_conn = (esp_tcp_connection*)reference;
    
    tcp_conn->tcp_data.buffer=data;
    tcp_conn->tcp_data.buffer_len = len;
    
    http_server_esp_send_data(tcp_conn);
}

/* 
 *   This callback is called by the SDK when a connection has received data
 */
static void http_server_data_received_callback(void *arg, char *data, unsigned short len) {

    struct espconn *conn=arg;
    esp_tcp_connection *tcp_conn = find_connection(conn);

    uint8_t *remote_ip = conn->proto.tcp->remote_ip;
    int remote_port = conn->proto.tcp->remote_port;
    int local_port = conn->proto.tcp->local_port;

    //HTTPSERVER_DEBUG("http_server_data_received_callback Remote IP: %d.%d.%d.%d Remote Port: %d, Local Port: %d,  Bytes: %d",remote_ip[0],remote_ip[1],remote_ip[2],remote_ip[3],remote_port,local_port,len);

    //find the server
    http_server_instance *server_instance = find_server(tcp_conn);

    //forward to http engine
    http_server_engine_tcp_received(server_instance->engine,tcp_conn,data,len);

}


static void http_server_connection_close_callback_task(task_param_t flag, uint8 priority) {

    HTTPSERVER_DEBUG("http_server_connection_close_callback_task. Reference: %ld,  Priority: %d",flag,priority);

    esp_tcp_connection *tcp_conn=(void*)flag;

    if(!tcp_conn->disconnected){
         int disconnect_result = espconn_disconnect(tcp_conn->conn);

        HTTPSERVER_DEBUG("http_server_connection_close_callback_task. Disconnect result: %d",disconnect_result);
        
    }
   

    LIST_REMOVE(tcp_conn,list);
    free(tcp_conn);

}

/* 
 *   This callback is called by the engine once it's done with a connection
 */
static void http_server_connection_close_callback(void *reference){

    //we should disconnect from a task rather than from a callback. SDK instructions
    task_post_medium(connection_close_callback_task_signal,(uint32_t)reference);

}

/* 
 *   This callback is called by the SDK when a connection is closed client side
 */
static void http_server_tcp_disconnected_callback(void *arg){
    
    struct espconn *conn=arg;
    esp_tcp_connection *tcp_conn = find_connection(conn);

    uint8_t *remote_ip = conn->proto.tcp->remote_ip;
    int remote_port = conn->proto.tcp->remote_port;

    HTTPSERVER_DEBUG("http_server_disconnected_callback. Reference: %ld, Remote IP: %d.%d.%d.%d Port: %d",arg,remote_ip[0],remote_ip[1],remote_ip[2],remote_ip[3],remote_port);

    http_server_instance *server_instance = find_server(tcp_conn);

    if(server_instance==NULL){
        HTTPSERVER_DEBUG("http_server_disconnected_callback server instane not found");
        return;
    }

    tcp_conn->disconnected=true;

    //forward to engine
    http_server_engine_tcp_disconnected(server_instance->engine,tcp_conn);

    
}

/* 
 *   This callback is called by the SDK when a new connections arrives
 */
static void http_server_connect_callback(void *arg) {

    http_server_global_init();

	struct espconn *conn=arg;
    esp_tcp_connection *tcp_conn = find_connection(conn);

    uint8_t *remote_ip = conn->proto.tcp->remote_ip;
    int remote_port = conn->proto.tcp->remote_port;

    //HTTPSERVER_DEBUG("http_server_connect_callback new connection. Reference: %ld, Remote IP: %d.%d.%d.%d Port: %d",arg,remote_ip[0],remote_ip[1],remote_ip[2],remote_ip[3],remote_port);

    http_server_instance *server_instance = find_server(tcp_conn);

    if(server_instance==NULL){
        HTTPSERVER_DEBUG("http_server_connect_callback server instane not found");
        return;
    }

	http_server_engine_connection *engine_connection= http_server_engine_new_connection(server_instance->engine,tcp_conn);

    //setup callbacks
    engine_connection->connection_close_callback=http_server_connection_close_callback;
    engine_connection->send_data_callback = http_server_send_data_callback;

	//let's disable NAGLE alg so TCP outputs faster ( in theory )
	espconn_set_opt(conn, ESPCONN_NODELAY );
    espconn_set_opt(conn, ESPCONN_REUSEADDR );


    //register callbacks for tcp//register espconn callbacks
    espconn_regist_recvcb(conn, http_server_data_received_callback);
    // espconn_regist_reconcb(conn, http_process_reconnect_cb);
    espconn_regist_disconcb(conn, http_server_tcp_disconnected_callback);    
    espconn_regist_sentcb(conn,http_server_esp_data_sent_callback);

    
}



http_server_instance *http_server_new(){

    HTTPSERVER_DEBUG("http_server_new");

    if(server_list==NULL){
        server_list = (http_server_instance_list*)os_malloc(sizeof(http_server_instance_list));
        LIST_INIT(server_list);
    }
    if(esp_connection_list==NULL){
        esp_connection_list = (esp_tcp_connection_list*)os_malloc(sizeof(esp_tcp_connection_list));
        LIST_INIT(esp_connection_list);
    }

    http_server_instance *instance = (http_server_instance*)zalloc(sizeof(http_server_instance));

    if(instance==NULL){
        HTTPSERVER_DEBUG("http_server_new failed to allocate memory for instance");
        return NULL;
    }

    http_server_engine * engine = http_server_engine_new();

    if(engine==NULL){
        HTTPSERVER_DEBUG("http_server_new failed to allocate memory for engine");
        return NULL;
    }

    instance->engine = engine;

    LIST_INSERT_HEAD(server_list,instance,list);


    return instance;

}


int http_server_listen(http_server_instance* server,listen_interface interface,unsigned int port){

    HTTPSERVER_DEBUG("http_server_listen");

    if(server==NULL){
        HTTPSERVER_DEBUG("http_server_listen null instance parameter");
        return 0;
    }

    espconn_delete(&server->server_conn); //just to be sure we are on square 1
	
	server->server_conn.type = ESPCONN_TCP;
	server->server_conn.state = ESPCONN_NONE;
	server->server_conn.proto.tcp = &server->server_tcp;
	server->server_conn.proto.tcp->local_port = port;

    if(interface!=BOTH){
        HTTPSERVER_DEBUG("http_server_listen only AT+SP mode avaiable for now");
        return 0;
    }

     os_memset(server->server_conn.proto.tcp->local_ip,0,4); //any ip

    // uint8_t wifi_mode = wifi_get_opmode();
    
    // if(wifi_mode = 3){       
    //     os_memset(server->server_conn.proto.tcp->local_ip,0,4);
    // }
    // else{
    //     struct ip_info ipconfig;
    //     wifi_get_ip_info(interface==ST?0:1,&ipconfig);
    //     os_memcpy(server->server_conn.proto.tcp->local_ip,&ipconfig,4);
    // }
    
    
    espconn_regist_connectcb(&server->server_conn, http_server_connect_callback);	
    int result=	espconn_accept(&server->server_conn);

    HTTPSERVER_DEBUG("http_server_listen espconn_accept result: %d",result);

    return 0;
}