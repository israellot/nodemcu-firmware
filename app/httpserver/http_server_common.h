#ifndef __HTTP_SERVER_COMMON_
#define __HTTP_SERVER_COMMON_

#include "user_interface.h"

#include "mem.h"

#define strncasecmp c_strncasecmp

#ifndef malloc 
#define malloc(a) c_malloc(a)
#endif

#ifndef realloc 
#define realloc(a,b) c_realloc(a,b)
#endif

#ifndef zalloc 
#define zalloc(a) c_zalloc(a)
#endif

#ifndef free 
#define free(a) c_free(a)
#endif

#ifndef memcpy 
#define memcpy(a,b,c) c_memcpy(a,b,c)
#endif

#ifndef printf 
#define printf c_sprintf
#endif

static const char log_prefix[] = "hs ";

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

#define MAX_MODULES 20
#define MAX_HEADERS 15
#define MAX_PATTERNS_MODULE 5

#define HTTP_REQUEST_ON_URL 0
#define HTTP_REQUEST_ON_HEADERS 1
#define HTTP_REQUEST_ON_BODY 2
#define HTTP_REQUEST_ON_BODY_COMPLETE 3

#define HTTP_MAX_TCP_CHUNK 2048

#define HTTP_VERSION "1.1"
#define HTTP_DEFAULT_SERVER "Node MCU"

#define HTTP_ACCESS_CONTROL_REQUEST_HEADERS "Access-Control-Request-Headers"
#define HTTP_ACCESS_CONTROL_REQUEST_METHOD "Access-Control-Request-Method"

#endif