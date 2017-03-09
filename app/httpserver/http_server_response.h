/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Israel Lot <me@israellot.com> wrote this file.
 * As long as you retain this notice you can do whatever you 
 * want with this stuff. If we meet some day, and you think this stuff is 
 * worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#ifndef __HTTP_SERVER_RESPONSE_H
#define __HTTP_SERVER_RESPONSE_H

#ifdef __cplusplus
extern "C" {
#endif


#include "http_server_common.h"
#include "http_server_engine.h"
#include "http_parser.h"
#include "user_config.h"

#define HTTP_CONTENT_LENGTH "Content-Length"
#define HTTP_TRANSFER_ENCODING "Transfer-Encoding"
#define HTTP_CONNECTION "Connection"
#define HTTP_CONNECTION_CLOSE "Close"
#define HTTP_CONTENT_TYPE "Content-Type"
#define HTTP_CONTENT_ENCODING "Content-Encoding"
#define HTTP_SERVER "Server"
#define HTTP_ACCESS_CONTROL_ALLOW_ORIGIN "Access-Control-Allow-Origin"
#define HTTP_ACCESS_CONTROL_ALLOW_METHODS "Access-Control-Allow-Methods"
#define HTTP_ACCESS_CONTROL_ALLOW_HEADERS "Access-Control-Allow-Headers"

#define HTTP_TEXT_CONTENT "text/plain"
#define HTTP_ENCODING_GZIP "gzip"


#define HTTP_OK 200
#define HTTP_NO_CONTENT 204
#define HTTP_MOVED_PERMANENTLY 301
#define HTTP_FOUND 302
#define HTTP_BAD_REQUEST 400
#define HTTP_NOT_FOUND 404
#define HTTP_INTERNAL_SERVER_ERROR 500 

#define NULL_TERMINATED_STRING -1


void http_engine_response_add_header(http_server_engine_connection *c,char *key,char *value);
void http_engine_response_write_response(http_server_engine_connection *c,char *buffer,unsigned int len);
void http_engine_response_send_response(http_server_engine_connection *c);
void http_engine_response_clear_headers(http_server_engine_connection *c);

#ifdef __cplusplus
}
#endif
#endif

