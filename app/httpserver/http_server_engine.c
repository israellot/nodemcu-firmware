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
#include "c_stdint.h"
#include "c_stddef.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "c_limits.h"
#include "c_stdio.h"
#include "mem.h"
#include "queue.h"



static int ICACHE_FLASH_ATTR parser_on_message_begin(http_parser *parser);
static int parser_on_url(http_parser *parser, const char *url, size_t length);
static int parser_on_status(http_parser *parser, const char *at, size_t length);
static int parser_on_header_field(http_parser *parser, const char *at, size_t length);
static int parser_on_header_value(http_parser *parser, const char *at, size_t length);
static int parser_on_headers_complete(http_parser *parser);
static int parser_on_body(http_parser *parser, const char *at, size_t length);
static int parser_on_message_complete(http_parser *parser);



http_server_engine* http_server_engine_new() {	
	
    http_server_engine *new_instance = (http_server_engine*)malloc(sizeof(http_server_engine));
			
	
	LIST_INIT(&(new_instance->connections));

	return new_instance;
}



void http_server_engine_new_connection(http_server_engine *server, void *reference){

	HTTPSERVER_DEBUG("http_server_engine_new_connection. Reference: %p",reference);

	//allocate a new connection object
	http_server_tcp_connection *new_connection = (http_server_tcp_connection*)zalloc(sizeof(http_server_tcp_connection));
	//save external referenc
	new_connection->reference=reference; 
	//insert it into the list
	LIST_INSERT_HEAD(&(server->connections),new_connection,list);
	
}

static http_server_tcp_connection* find_connection(http_server_engine *server,void *reference){

	http_server_tcp_connection *conn;
	LIST_FOREACH(conn,&(server->connections),list){
		if(conn->reference==reference)
			return conn;
	}

	return NULL;
}

void http_server_engine_tcp_received(http_server_engine *server,void *reference,char *buffer,unsigned short len){

	http_server_tcp_connection *connection = find_connection(server,reference);

	if(connection==NULL){
		HTTPSERVER_DEBUG("http_server_engine_tcp_received : Connection not found")
		return;
	}

	if(connection->parser.data==NULL){
		HTTPSERVER_DEBUG("http_server_engine_tcp_received: Begin receive data. Initing parser")
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
}



//HTTP PARSER CALLBACKS

static int ICACHE_FLASH_ATTR parser_on_message_begin(http_parser *parser){
	HTTPSERVER_DEBUG("http_parser message begin");
	
	//nothing to do here
	return 0;
}



static int parser_on_url(http_parser *parser, const char *url, size_t length)
{	
	//grab the connection
	http_server_tcp_connection *c = (http_server_tcp_connection *)parser->data;

	//copy url to request info
	c->request.url = (char*)malloc(length+1);
	memcpy(c->request.url,url,length); 
	c->request.url[length]=0; //null terminate string
	
	HTTPSERVER_DEBUG("parser_on_url : method: %d",parser->method);
	HTTPSERVER_DEBUG("parser_on_url : url: %s",c->request.url);

	//Parse url
	memset(&(c->request.url_parsed),0,sizeof(struct http_parser_url));

	http_parser_parse_url(
		(const char *)c->request.url,
		length,
		0,
		&(c->request.url_parsed)
		);

	#ifdef DEVELOP_VERSION

		#define URL_DEBUG(FIELD) do{ \
			if(c->request.url_parsed.field_set & (1<<FIELD)){	\
				HTTPSERVER_DEBUG("parser_on_url " #FIELD ": %.*s",c->request.url_parsed.field_data[ FIELD ].len,c->request.url+c->request.url_parsed.field_data[ FIELD ].off)	\
			}	\
			while(0)
		

		HTTPSERVER_DEBUG("parser_on_url PORT: %d",c->request.url_parsed.port);			

		URL_DEBUG(UF_SCHEMA);
		URL_DEBUG(UF_HOST);
		URL_DEBUG(UF_PORT);
		URL_DEBUG(UF_PATH);
		URL_DEBUG(UF_QUERY);
		URL_DEBUG(UF_FRAGMENT);
		URL_DEBUG(UF_USERINFO);

	#endif

	

	return 0;
}

static int parser_on_status(http_parser *parser, const char *at, size_t length)
{	
	HTTPSERVER_DEBUG("parser_on_status STATUS: %.*s",length,at);

	//grab the connection
	http_server_tcp_connection *c = (http_server_tcp_connection *)parser->data;
	
	return 0;
}


static int parser_on_header_field(http_parser *parser, const char *at, size_t length)
{
	HTTPSERVER_DEBUG("parser_on_header_field Header: %.*s",length,at);

	//grab the connection
	http_server_tcp_connection *c = (http_server_tcp_connection *)parser->data;

	
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
	HTTPSERVER_DEBUG("parser_on_header_value Header: %.*s",length,at);
	
	//grab the connection
	http_server_tcp_connection *c = (http_server_tcp_connection *)parser->data;


	for(int i=0;i<MAX_HEADERS;i++){
		if(c->request.headers[i].key==NULL)
			break;

		if(c->request.headers[i].found==1 && c->request.headers[i].value==NULL){
			HTTPSERVER_DEBUG("parser_on_header_value Saving header");
			
			c->request.headers[i].value=(char *)malloc(length+1);
			os_memcpy(c->request.headers[i].value,at,length);
			c->request.headers[i].value[length]=0;

			break;
		}		
	}	

	
	return 0;
}

static int parser_on_headers_complete(http_parser *parser)
{
	HTTPSERVER_DEBUG("parser_on_headers_complete");


	return 0;	
}

static int parser_on_body(http_parser *parser, const char *at, size_t length)
{
	HTTPSERVER_DEBUG("parser_on_body data :%.*s",length,at);

	//grab the connection
	http_server_tcp_connection *c = (http_server_tcp_connection *)parser->data;

	

	if(c->request.body.saveFlag){

		HTTPSERVER_DEBUG("parser_on_body saving %d bytes",length);

		if(c->request.body.data==NULL){
			c->request.body.data = (char *)malloc(length+1);		
			os_memcpy(c->request.body.data,at,length);	
			c->request.body.data[length]=0;
			c->request.body.lenght = length;			
		}
		else{
			//assuming body can come in different tcp packets, this callback will be called
			//more than once

			unsigned int newLenght = c->request.body.lenght+length;
			char * newBuffer = (char *)malloc(newLenght+1);
			os_memcpy(newBuffer,c->request.body.data,c->request.body.lenght); //copy previous data
			os_memcpy(newBuffer+c->request.body.lenght,at,length); //copy new data
			os_free(c->request.body.data); //free previous
			c->request.body.data=newBuffer;
			c->request.body.lenght=newLenght;
			c->request.body.data[newLenght]=0;
		}

	}
	
	

	return 0;	
}



static int parser_on_message_complete(http_parser *parser)
{

	HTTPSERVER_DEBUG("parser_on_message_complete");

	//grab the connection
	http_server_tcp_connection *c = (http_server_tcp_connection *)parser->data;

	//free body, as soon as possible
	if(c->request.body.saveFlag==1 && c->request.body.data!=NULL){
		HTTPSERVER_DEBUG("parser_on_message_complete freeing body memory");
		free(c->request.body.data);
		c->request.body.lenght=0;
	}	

	return 0;
}
