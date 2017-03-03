/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Israel Lot <me@israellot.com> wrote this file.
 * As long as you retain this notice you can do whatever you 
 * want with this stuff. If we meet some day, and you think this stuff is 
 * worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#ifndef http_parser_h
#define http_parser_h
#ifdef __cplusplus
extern "C" {
#endif


typedef struct {

	//Listening connection data
	struct espconn listener_connection;
	esp_tcp listener_tcp;

    

} http_server_tcp;


#ifdef __cplusplus
}
#endif
#endif