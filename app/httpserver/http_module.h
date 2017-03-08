/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Israel Lot <me@israellot.com> wrote this file.
 * As long as you retain this notice you can do whatever you 
 * want with this stuff. If we meet some day, and you think this stuff is 
 * worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

#ifndef __HTTP_MODULE_
#define __HTTP_MODULE_

#include "http_server_common.h"
#include "http_server_engine.h"
#include "http_server_module.h"
#include "c_stdint.h"
#include "c_stddef.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "c_limits.h"
#include "c_stdio.h"
#include "mem.h"
#include "queue.h"


http_module* http_module_404_new();
http_module* http_module_default_headers_new();
http_module* http_module_cors_new();
http_module* http_module_file_new();

#endif




