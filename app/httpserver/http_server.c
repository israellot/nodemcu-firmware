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
#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/dns.h" 
#include "lwip/igmp.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"

static void http_server_connection_close_callback_task(task_param_t flag, uint8 priority);
static void yield_request_callback_task(task_param_t flag, uint8 priority);

static http_server_instance_list* server_list;
static esp_tcp_connection_list* esp_connection_list;

static task_handle_t connection_close_callback_task_signal;
static task_handle_t yield_request_callback_task_signal;

static int global_init=0;


static void http_server_global_init(){

    if(!global_init){
        HTTPSERVER_DEBUG("global_init");
        connection_close_callback_task_signal = task_get_id(http_server_connection_close_callback_task);
        yield_request_callback_task_signal = task_get_id(yield_request_callback_task);
        global_init=1;
    }

}

static esp_tcp_connection* http_server_new_tcp_connection(http_server_instance *server,struct tcp_pcb *pcb){

    uint32_t remote_ip = (uint32_t)(pcb->remote_ip.addr);
    uint32_t remote_port = (uint32_t)pcb->remote_port;

    uint32_t local_ip = (uint32_t)(pcb->local_ip.addr);
    uint32_t local_port = (uint32_t)pcb->local_port;

    uint8_t *remote_ip_8 = (uint8_t *)&remote_ip;

    esp_tcp_connection *connection;

    // LIST_FOREACH(connection,esp_connection_list,list){
	// 	if(connection->remote_ip==remote_ip && connection->remote_port==remote_port)
    //     {
    //         connection->tcp_pcb=pcb;

    //         return connection;
    //     }
	// }

    HTTPSERVER_DEBUG("http_server_new_tcp_connection. Remote IP: %d.%d.%d.%d , port : %ld",remote_ip_8[0],remote_ip_8[1],remote_ip_8[2],remote_ip_8[3],remote_port);

    //couldn't find, so create
    connection = (esp_tcp_connection*)zalloc(sizeof(esp_tcp_connection));
    connection->remote_port = remote_port;
    connection->remote_ip = (remote_ip);
    connection->local_port = local_port;
    connection->local_ip = (local_ip);
    connection->tcp_pcb =pcb;
    connection->server=server;

    LIST_INSERT_HEAD(esp_connection_list,connection,list);

    return connection;

}

int http_server_connection_count(http_server_instance *server_instance ){

    int count=0;
     esp_tcp_connection *connection;
    LIST_FOREACH(connection,esp_connection_list,list){
		if(connection->local_port==server_instance->server_tcp.local_port)
        {
            count++;
        }
	}

    return count;

}



static void http_server_esp_data_sent_callback(void *arg);

static void http_server_esp_send_data(esp_tcp_connection *tcp_conn){

    u16_t tcp_buffer_size = tcp_sndbuf(tcp_conn->tcp_pcb);

    //HTTPSERVER_DEBUG("http_server_esp_send_data b size: %d",tcp_buffer_size);

    if(tcp_conn->tcp_data.buffer_len>tcp_buffer_size){
        //send partial data
        
        err_t err = tcp_write(tcp_conn->tcp_pcb, tcp_conn->tcp_data.buffer, tcp_buffer_size, 0);
       
        HTTPSERVER_DEBUG("err:%d sent %d",err,tcp_buffer_size);

        tcp_conn->tcp_data.buffer+=tcp_buffer_size;
        tcp_conn->tcp_data.buffer_len -= tcp_buffer_size;       

        
    }else{
        
         err_t err = tcp_write(tcp_conn->tcp_pcb, tcp_conn->tcp_data.buffer, tcp_conn->tcp_data.buffer_len, 0);

        HTTPSERVER_DEBUG("err :%d sent %d",err,tcp_conn->tcp_data.buffer_len);
       
        tcp_conn->tcp_data.buffer=NULL;
        tcp_conn->tcp_data.buffer_len = 0;
        
    }
    
}

/* 
 *   This callback is called by the SDK when a connection is ready to send more data
 */
static err_t http_server_lwip_data_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len){

    
    //find the connection reference
    esp_tcp_connection *tcp_conn = (esp_tcp_connection*)(arg);


    if(tcp_conn->tcp_data.buffer!=NULL){
        //send more data
        http_server_esp_send_data(tcp_conn);
    }
    else{
        //callback engine

        //forward to engine
        http_server_engine_tcp_ready_to_send(tcp_conn->server->engine,tcp_conn);
    }   

    return ERR_OK;

}

/* 
 *   This callback is called by the ENGINE when a connection should send data
 */
static void http_server_send_data_callback(void *reference, char *data, unsigned int len) {
    
    esp_tcp_connection *tcp_conn = (esp_tcp_connection*)reference;
    
    
    tcp_conn->tcp_data.buffer=data;
    tcp_conn->tcp_data.buffer_len = len;
    
    
    http_server_esp_send_data(tcp_conn);
}

/* 
 *   This callback is called by LWIP when a connection has received data
 */

static void http_server_lwip_error_callback(void *arg, err_t err) {
  
    esp_tcp_connection *tcp_conn = (esp_tcp_connection*)arg;
    tcp_conn->tcp_pcb=NULL; // Will be freed at LWIP level

    HTTPSERVER_DEBUG("http_server_lwip_error_callback %d",err);
      
    tcp_conn->disconnected=true;

    //forward to engine
    http_server_engine_tcp_disconnected(tcp_conn->server->engine,tcp_conn);

}

static err_t http_server_lwip_data_received_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err){
    
    esp_tcp_connection *tcp_conn = (esp_tcp_connection*)arg;

    if (!p) {
        //network eror
        return tcp_close(tpcb);
    }
   
    //HTTPSERVER_DEBUG("http_server_data_received_callback Remote IP: %d.%d.%d.%d Remote Port: %d, Local Port: %d,  Bytes: %d",remote_ip[0],remote_ip[1],remote_ip[2],remote_ip[3],remote_port,local_port,len);
   
    //forward to http engine
    http_server_engine_tcp_received(tcp_conn->server->engine,tcp_conn,p->payload,p->len);
    pbuf_free(p);
    tcp_recved(tpcb, TCP_WND);

    return ERR_OK;
}

static void yield_request_callback_task(task_param_t flag, uint8 priority){

    http_server_engine_yield *yield_data=(void*)flag;

    if(yield_data->callback!=NULL){
        yield_data->callback(yield_data->connection,yield_data->data);
    }

}

/* 
 *   This callback is called by the engine when it wants to yield the thread
 */
static void http_server_connection_yield_callback(http_server_engine_yield *yield_data){

    //we should disconnect from a task rather than from a callback. SDK instructions
    task_post_low(yield_request_callback_task_signal,(uint32_t)yield_data);

}

static void http_server_connection_close_callback_task(task_param_t flag, uint8 priority) {

    HTTPSERVER_DEBUG("http_server_connection_close_callback_task. Reference: %ld,  Priority: %d",flag,priority);


    esp_tcp_connection *connection = (void*)flag;


    if(connection!=NULL){
        
        if(!connection->disconnected){
            
            int disconnect_result = tcp_close(connection->tcp_pcb);
            connection->disconnected=1;
            HTTPSERVER_DEBUG("http_server_connection_close_callback_task. Disconnect result: %d",disconnect_result);        
        }       

        LIST_REMOVE(connection,list);
        free(connection);

    }

 
}

/* 
 *   This callback is called by the engine once it's done with a connection
 */
static void http_server_connection_close_callback(void *reference){

    
    //we should disconnect from a task rather than from a callback. SDK instructions
    task_post_low(connection_close_callback_task_signal,(task_param_t)reference);

}

static err_t http_server_lwip_connect_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {

    http_server_global_init();

    http_server_instance *server=(http_server_instance *)arg;

    esp_tcp_connection *tcp_conn = http_server_new_tcp_connection(server,newpcb);

    http_server_engine_connection *engine_connection= http_server_engine_new_connection(tcp_conn->server->engine,tcp_conn);

    //setup callbacks
    engine_connection->connection_close_callback=http_server_connection_close_callback;
    engine_connection->send_data_callback = http_server_send_data_callback;
    engine_connection->yield_callback=http_server_connection_yield_callback;

    tcp_arg(newpcb, tcp_conn);
    tcp_err(newpcb, http_server_lwip_error_callback);
    tcp_recv(newpcb, http_server_lwip_data_received_callback);
    tcp_sent(newpcb, http_server_lwip_data_sent_callback);
    tcp_nagle_disable(newpcb);
    // newpcb->so_options |= SOF_KEEPALIVE;
    // newpcb->keep_idle = 5 * 1000;
    // newpcb->keep_cnt = 1;
    tcp_accepted(server->tcp_pcb);

    return ERR_OK;
}


http_server_instance *http_server_new(){

    HTTPSERVER_DEBUG("http_server_new");

    if(server_list==NULL){
        server_list = (http_server_instance_list*)zalloc(sizeof(http_server_instance_list));
        LIST_INIT(server_list);
    }
    if(esp_connection_list==NULL){
        esp_connection_list = (esp_tcp_connection_list*)zalloc(sizeof(esp_tcp_connection_list));
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


    server->tcp_pcb=tcp_new();

    const char *domain = "0.0.0.0";
    ip_addr_t addr;
    ipaddr_aton(domain, &addr);
    
    server->tcp_pcb->so_options |= SOF_REUSEADDR;
    tcp_bind(server->tcp_pcb, &addr, port);
    tcp_arg(server->tcp_pcb, server);

    struct tcp_pcb *pcb = tcp_listen(server->tcp_pcb);
    server->tcp_pcb = pcb;
    tcp_accept(server->tcp_pcb, http_server_lwip_connect_callback);


    if(interface!=BOTH){
        HTTPSERVER_DEBUG("http_server_listen only AT+SP mode avaiable for now");
        return 0;
    }

 
    HTTPSERVER_DEBUG("http_server_listen espconn_accept");

    return 0;
}