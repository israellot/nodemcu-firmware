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
#include "user_interface.h"


http_server_instance *http_server_new(){

    http_server_instance *instance = (http_server_instance*)zalloc(sizeof(http_server_instance));

    instance->engine = http_server_engine_new();

}