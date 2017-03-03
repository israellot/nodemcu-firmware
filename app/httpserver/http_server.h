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


#define zalloc os_zalloc
#define strncasecmp c_strncasecmp
#define malloc os_malloc
#define free os_free
#define memcpy os_memcpy


typedef struct http_server_instance{

    http_server_engine* engine;

}http_server_instance;


//public interface
http_server_instance *http_server_new();

//This are the functions that need to be implemented by port

#ifdef __cplusplus
}
#endif
#endif