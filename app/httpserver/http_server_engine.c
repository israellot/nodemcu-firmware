/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Israel Lot <me@israellot.com> wrote this file.
 * As long as you retain this notice you can do whatever you 
 * want with this stuff. If we meet some day, and you think this stuff is 
 * worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

#include "http_server_engine.h"
#include "http_parser.h"
#include "http_server.h"
#include "http_server_module.h"
#include "http_module_default.h"
#include "c_stdint.h"
#include "c_stddef.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "c_limits.h"
#include "c_stdio.h"
#include "mem.h"
#include "queue.h"




http_server_engine* http_server_engine_new() {	
	
    http_server_engine *new_instance = (http_server_engine*)zalloc(sizeof(http_server_engine));			
	
	LIST_INIT(&(new_instance->connection_list));

	//default modules	
	http_module_attach_to_engine(new_instance,http_module_404_new());
	http_module_attach_to_engine(new_instance,http_module_default_headers_new());
	http_module_attach_to_engine(new_instance,http_module_cors_new());


	return new_instance;
}


http_server_engine_connection* http_server_engine_new_connection(http_server_engine *server, void *reference){

	HTTPSERVER_DEBUG("http_server_engine_new_connection. Reference: %ld",reference);

	//allocate a new connection object
	http_server_engine_connection *new_connection = (http_server_engine_connection*)zalloc(sizeof(http_server_engine_connection));

	if(new_connection==NULL){
		HTTPSERVER_DEBUG("http_server_engine_new_connection failed to allocate memory for new connection");
		return NULL;
	}

	new_connection->server =server;//attach to server

	//save external reference
	new_connection->reference=reference; 
	//insert it into the list
	LIST_INSERT_HEAD(&(server->connection_list),new_connection,list);
	
	return new_connection;
}

static http_server_engine_connection* find_connection(http_server_engine *server,void *reference){

	http_server_engine_connection *conn;
	LIST_FOREACH(conn,&(server->connection_list),list){
		if(conn->reference==reference)
			return conn;
	}

	return NULL;
}

void http_server_engine_close_connection(http_server_engine *server,http_server_engine_connection *c){

	HTTPSERVER_DEBUG("http_server_engine_close_connection. Reference: %ld",c->reference);

	//free url
	if(c->request.url.data!=NULL){
		free(c->request.url.data);		
	}

	//free request headers
	for(int i=0;i<MAX_HEADERS;i++){
		if(c->request.headers[i].value!=NULL)
			free(c->request.headers[i].value);
	}

	//free response headers
	for(int i=0;i<MAX_HEADERS;i++){
		if(c->response.headers[i].key!=NULL)
			free(c->response.headers[i].key);
	}

	//free body
	if(c->request.body.data!=NULL){
		free(c->request.body.data);
	}

	//free response buffer
	if(c->response.output.buffer!=NULL){
		free(c->response.output.buffer);
	}

	if(c->connection_close_callback!=NULL)
		c->connection_close_callback(c->reference);

	//remove from the list
	LIST_REMOVE(c,list);	

	//finally free the connection object
	free(c);

}

void http_server_engine_tcp_disconnected(http_server_engine *server,void *reference){

	http_server_engine_connection *connection = find_connection(server,reference);

	if(connection==NULL){
		HTTPSERVER_DEBUG("http_server_engine_tcp_disconnected : Connection not found");
		return;
	}

	http_server_engine_close_connection(server,connection);

}

//called by server when tcp is ready to send more data
void http_server_engine_tcp_ready_to_send(http_server_engine *server,void *reference){


	http_server_engine_connection *connection = find_connection(server,reference);

	if(connection==NULL){
		HTTPSERVER_DEBUG("http_server_engine_tcp_disconnected : Connection not found");
		return;
	}

	//close for now
	http_server_engine_close_connection(server,connection);


}

void http_server_engine_tcp_received(http_server_engine *server,void *reference,char *buffer,unsigned short len){

	http_server_engine_connection *connection = find_connection(server,reference);

	if(connection==NULL){
		HTTPSERVER_DEBUG("http_server_engine_tcp_received : Connection not found");
		return;
	}

	if(connection->parser.data==NULL){
		//HTTPSERVER_DEBUG("http_server_engine_tcp_received: Begin receive data. Initing parser");
		//init http parser settings
		http_parser_settings_init(&(connection->parser_settings));
		connection->parser_settings.on_message_begin=parser_on_message_begin;
		connection->parser_settings.on_url=parser_on_url;
		connection->parser_settings.on_header_field=parser_on_header_field;
		connection->parser_settings.on_header_value=parser_on_header_value;
		connection->parser_settings.on_headers_complete=parser_on_headers_complete;
		connection->parser_settings.on_body=parser_on_body;
		connection->parser_settings.on_message_complete=parser_on_message_complete;

		//init http parser
		http_parser_init(&(connection->parser),HTTP_REQUEST);

		connection->parser.data=connection; //save connection reference on parser 
	}

	//pass data to http_parser	
	unsigned int nparsed = http_parser_execute(
		&(connection->parser),
		&(connection->parser_settings),
		buffer,
		len);

	if (connection->parser.upgrade) {
  		/* handle new protocol */
		//on_websocket_data(&conn->parser,data,len);
		HTTPSERVER_DEBUG("http_server_engine_tcp_received: Connection Upgrade");

	} else if (nparsed != len) {
		HTTPSERVER_DEBUG("http_server_engine_tcp_received: Parser error");
	  	// Handle error. Usually just close the connection.
		http_server_engine_close_connection(server,connection);
		
	}
}


void http_server_connection_add_request_header(http_server_engine_connection *connection,char *new_header){

	for(unsigned int i=0;i<MAX_HEADERS;i++){

		http_request_header *header = &(connection->request.headers[i]);

		if(header->key==NULL){
			header->key=new_header;
			return;
		}

		if(strcasecmp(header->key,new_header)==0)
			return;

		
	}

	HTTPSERVER_DEBUG("http_server_connection_add_request_header: max headers count reached");	

}

http_request_header* http_server_connection_get_header(http_server_engine_connection *c,char *key){

	for(unsigned i=0;i<MAX_HEADERS;i++){

		http_request_header *header = &(c->request.headers[i]);

		if(header->key==NULL)
			break;

		if(strcasecmp(header->key,key)==0 && header->value!=NULL){
			return header;
		}
	}

	return NULL;

}

/*
 *  HTTP PARSER CALLBACKS
 */ 
static int  parser_on_message_begin(http_parser *parser){
	HTTPSERVER_DEBUG("http_parser message begin");
	
	//nothing to do here
	return 0;
}


static int parser_on_url(http_parser *parser, const char *url, size_t length)
{	
	//grab the connection
	http_server_engine_connection *c = (http_server_engine_connection *)parser->data;

	//copy url to request info
	c->request.url.data = (char*)malloc(length+1);
	memcpy(c->request.url.data,url,length); 
	c->request.url.data[length]=0; //null terminate string
	
	c->request.method =http_method_str(parser->method);
	c->request.method_code=(unsigned int)parser->method;

	HTTPSERVER_DEBUG("parser_on_url : method: %s",c->request.method);
	//HTTPSERVER_DEBUG("parser_on_url : url: %s",c->request.url.data);

	struct http_parser_url *url_parser = &(c->request.url.url_parsed);

	//Parse url
	memset(url_parser,0,sizeof(struct http_parser_url));

	http_parser_parse_url(
			(const char *)c->request.url.data,
			length,
			0,
			url_parser
	);

	if(url_parser->field_set & (1<<UF_PATH)){	

		char * start = c->request.url.data + url_parser->field_data[UF_PATH].off;
		char * end = start + url_parser->field_data[UF_PATH].len;
		*end=0;

		c->request.url.path = start;

		HTTPSERVER_DEBUG("parser_on_url path: %s",start);
	}
	if(url_parser->field_set & (1<<UF_QUERY)){	

		char * start = c->request.url.data + url_parser->field_data[UF_QUERY].off;		
		char * end = start + url_parser->field_data[UF_QUERY].len;
		*end=0;

		c->request.url.query = start;

		HTTPSERVER_DEBUG("parser_on_url query: %s",start);
	}

	http_module_process(c,HTTP_REQUEST_ON_URL);

	return 0;
}

static int parser_on_status(http_parser *parser, const char *at, size_t length)
{	
	HTTPSERVER_DEBUG("parser_on_status STATUS: %.*s",length,at);

	//grab the connection
	http_server_engine_connection *c = (http_server_engine_connection *)parser->data;
	
	return 0;
}


static int parser_on_header_field(http_parser *parser, const char *at, size_t length)
{
	//HTTPSERVER_DEBUG("parser_on_header_field Header: %.*s",length,at);

	//grab the connection
	http_server_engine_connection *c = (http_server_engine_connection *)parser->data;

	//clear found flag for all
	for(int i=0;i<MAX_HEADERS;i++){
		c->request.headers[i].found=0;
	}
	
	for(int i=0;i<MAX_HEADERS;i++){
		if(c->request.headers[i].key==NULL)
			break;

		if(strncasecmp(c->request.headers[i].key,at,length)==0){
			HTTPSERVER_DEBUG("parser_on_header_field Header marked for saving");
			c->request.headers[i].found=1;
			break;
		}		
	}	
	
	return 0;
}

static int parser_on_header_value(http_parser *parser, const char *at, size_t length)
{
	//HTTPSERVER_DEBUG("parser_on_header_value Header: %.*s",length,at);
	
	//grab the connection
	http_server_engine_connection *c = (http_server_engine_connection *)parser->data;


	for(int i=0;i<MAX_HEADERS;i++){
		if(c->request.headers[i].key==NULL)
			break;

		if(c->request.headers[i].found==1){

			if(c->request.headers[i].value==NULL){
				HTTPSERVER_DEBUG("parser_on_header_value Saving header");
				
				c->request.headers[i].value=(char *)malloc(length+1);
				memcpy(c->request.headers[i].value,at,length);
				c->request.headers[i].value[length]=0;

				break;
			}
			else{
				//header values can come in more than one packet, hance being split
				//here we join them

				unsigned int currentLength = strlen(c->request.headers[i].value);
				unsigned int newLength = currentLength+length+1;
				char * newBuffer = (char *)malloc(newLength+1);
				memcpy(newBuffer,c->request.headers[i].value,currentLength); //copy previous data
				memcpy(newBuffer+currentLength,at,length); //copy new data
				newBuffer[newLength]=0; //null terminate
				free(c->request.headers[i].value);
				c->request.headers[i].value=newBuffer;				
			}
		}
		
	}	

	
	return 0;
}

static int parser_on_headers_complete(http_parser *parser)
{
	HTTPSERVER_DEBUG("parser_on_headers_complete");

	//grab the connection
	http_server_engine_connection *c = (http_server_engine_connection *)parser->data;

	//clear found flag for all
	for(int i=0;i<MAX_HEADERS;i++){
		c->request.headers[i].found=0;
	}

	http_module_process(c,HTTP_REQUEST_ON_HEADERS);

	return 0;	
}

static int parser_on_body(http_parser *parser, const char *at, size_t length)
{
	HTTPSERVER_DEBUG("parser_on_body data :%.*s",length,at);

	//grab the connection
	http_server_engine_connection *c = (http_server_engine_connection *)parser->data;
	

	if(c->request.body.save_flag){

		HTTPSERVER_DEBUG("parser_on_body saving %d bytes",length);

		if(c->request.body.data==NULL){
			c->request.body.data = (char *)malloc(length+1);		
			os_memcpy(c->request.body.data,at,length);	
			c->request.body.data[length]=0;
			c->request.body.length = length;			
		}
		else{
			//assuming body can come in different tcp packets, this callback will be called
			//more than once

			unsigned int newLength = c->request.body.length+length;
			char * newBuffer = (char *)malloc(newLength+1);
			memcpy(newBuffer,c->request.body.data,c->request.body.length); //copy previous data
			memcpy(newBuffer+c->request.body.length,at,length); //copy new data
			free(c->request.body.data); //free previous
			c->request.body.data=newBuffer;
			c->request.body.length=newLength;
			c->request.body.data[newLength]=0;
		}

	}
	
	http_module_process(c,HTTP_REQUEST_ON_BODY);

	return 0;	
}

static int parser_on_message_complete(http_parser *parser)
{

	HTTPSERVER_DEBUG("parser_on_message_complete");

	//grab the connection
	http_server_engine_connection *c = (http_server_engine_connection *)parser->data;

	http_module_process(c,HTTP_REQUEST_ON_BODY_COMPLETE);

	// //free body, as soon as possible
	// if(c->request.body.data!=NULL){
	// 	HTTPSERVER_DEBUG("parser_on_message_complete freeing body memory");
	// 	free(c->request.body.data);
	// 	c->request.body.length=0;
	// }	

	return 0;
}
