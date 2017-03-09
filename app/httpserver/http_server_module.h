/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Israel Lot <me@israellot.com> wrote this file.
 * As long as you retain this notice you can do whatever you 
 * want with this stuff. If we meet some day, and you think this stuff is 
 * worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#ifndef http_server_module_h
#define http_server_module_h
#ifdef __cplusplus
extern "C" {
#endif

#include "http_server_common.h"
#include "http_server_engine.h"

struct http_module;
struct http_server_engine_connection;

typedef void (*http_module_self_callback)(struct http_module *module);
typedef void (*http_module_event_delegate)(struct http_module *module,struct http_server_engine_connection *connection,void **data);
typedef void (*http_module_send_response_delegate)(struct http_module *module,struct http_server_engine_connection *connection,void **data,unsigned int max_length);


typedef struct http_module{

	char *module_name;

	char* route_list[MAX_PATTERNS_MODULE]; //list of routes it should be attached to
	char* header_list[MAX_HEADERS]; //list of headers should be saved
	
	void *module_data;

	struct process{

		http_module_event_delegate on_url;
		http_module_event_delegate on_headers;
		http_module_event_delegate on_body;
		http_module_event_delegate on_body_complete;
		http_module_send_response_delegate on_send_response;

	} process;

	struct self_callbacks{

		http_module_self_callback on_destroy;

	} self_callbacks;

} http_module;




void http_module_process(struct http_server_engine_connection *c,unsigned int state);
void http_module_add_url(http_module *module,char *url);
void http_module_add_header(http_module *module,char *header);
http_module* http_module_new(char *name);
void http_module_attach_to_engine(struct http_server_engine *engine,http_module *new_module);

#ifdef __cplusplus
}
#endif
#endif