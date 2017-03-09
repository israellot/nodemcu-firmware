/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Israel Lot <me@israellot.com> wrote this file.
 * As long as you retain this notice you can do whatever you 
 * want with this stuff. If we meet some day, and you think this stuff is 
 * worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */



#include "http_module.h"
#include "http_server_module.h"
#include "http_server_engine.h"
#include "http_server_response.h"
#include "http_parser.h"
#include "c_stdint.h"
#include "c_stddef.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "c_limits.h"
#include "c_stdio.h"
#include "mem.h"
#include "queue.h"
#include "vfs.h"


//Struct to keep extension->mime data in
typedef struct {
	const char *ext;
	const char *mimetype;
} mime_map;

static const mime_map mime_types[]={
	{"htm", "text/htm"},
	{"html", "text/html"},
	{"css", "text/css"},
	{"js", "text/javascript"},
	{"txt", "text/plain"},
	{"jpg", "image/jpeg"},
    {"gif", "image/gif"},
	{"jpeg", "image/jpeg"},
	{"png", "image/png"},
	{"json","application/json"},
	{"svg","image/svg+xml"},
	{NULL, "text/plain"}, //default value
};

static const char* index_mapping[]={
    "index.html",
    // "default.html",
    // "home.html",
    NULL
};

typedef struct http_module_file_data{

    char *prefix;
    char *folder;

}http_module_file_data;

typedef struct http_module_file_response_data{

    int file_fd;
    unsigned int file_size;
    unsigned int file_pos;

}http_module_file_response_data;

void http_server_file_on_destroy(http_module *module){

    //any cleanup necessary
    free(module->module_data);

}

void http_module_file_on_send_response(struct http_module *module,http_server_engine_connection *c,void **data,unsigned int max_length){

        char file_buffer[HTTP_MAX_TCP_CHUNK]; //stack risk?

        http_module_file_response_data *response_data=(http_module_file_response_data*)(*data);

        if(response_data->file_size < max_length){
            //send all at once
            
            vfs_read(response_data->file_fd,file_buffer,response_data->file_size);           
           
            http_engine_response_write_response(c,file_buffer,response_data->file_size);

            vfs_close(response_data->file_fd);
            //free(file_buffer);
            free(*data);
            

            c->response.body_finished=1;

        } else{

           
            unsigned int read = vfs_read(response_data->file_fd,file_buffer,max_length);

            http_engine_response_write_response(c,file_buffer,read);

            response_data->file_pos+=read;

            if(vfs_eof(response_data->file_fd)){
                vfs_close(response_data->file_fd);
                //free(module_data->buffer);
                free(*data);
                

                c->response.body_finished=1;
            }

        }
}

const char * http_module_file_get_mime_type(char *path){
   
	//find the extension
	char *ext=path+(strlen(path)-1);
	while (ext!=path && *ext!='.') ext--;
	if (*ext=='.') ext++;	

     int i=0;
	//ToDo: os_strcmp is case sensitive; we may want to do case-intensive matching here...
	while (mime_types[i].ext!=NULL && strcasecmp(ext, mime_types[i].ext)!=0) i++;
	return mime_types[i].mimetype;
}

static int try_file(const char *base,const char *path,const char *file,char **output){

    char *full_file_name = *output;
    
    full_file_name[0]=0;

    if(base!=NULL){
        strcat(full_file_name,base);

        //fix '/'
        int file_length=strlen(full_file_name);
        if(file_length>0 && full_file_name[file_length-1]!='/'){
            full_file_name[file_length]='/';
            full_file_name[file_length+1]=0;
        }
    }
        

    if(path!=NULL){
         strcat(full_file_name,path);   
      
          //fix '/'
        int file_length=strlen(full_file_name);
        if(full_file_name[file_length-1]!='/'){
            full_file_name[file_length]='/';
            full_file_name[file_length+1]=0;
        }
    }

    strcat(full_file_name,file);              

    vfs_item *f_stat = vfs_stat(full_file_name);

    HTTPSERVER_DEBUG("http_file trying : %s, found %d",full_file_name,f_stat?1:0);

    if(f_stat){
        vfs_closeitem(f_stat);
        return vfs_open(full_file_name, "r");
    }    

            

    return 0;
}

static int seek_index(char *base,char *path,char **output){
    
    int fd=0;
    int i=0;
    while(index_mapping[i]!=NULL){

        fd = try_file(base,path,index_mapping[i],output);
        if(fd)
            break;

        i++;
    }

    return fd;

}



void http_module_file_on_headers(struct http_module *module,http_server_engine_connection *c,void **data){

    http_module_file_data *module_data = (http_module_file_data*)module->module_data;

    if(c->request.method_code != HTTP_GET)
        return;


    int file_fd=0;
    char full_file_name[50];

    char *output = full_file_name;

    char *path = c->request.url.path;

    char *file_path=NULL;
    

    unsigned int prefix_length=strlen(module_data->prefix)-1;// -1 to account for trailling *

    //get file name
    if(strlen(path) <= prefix_length)
    {
        //dealing with root on first level, possibly without trailing '/' eg: /folder to get /folder/index.html
        file_fd=seek_index(module_data->folder,NULL,&output);
    }
    else{

        file_path=&(path[prefix_length]);

        //try file as is
        file_fd = try_file(module_data->folder,NULL,file_path,&output);

        if(!file_fd){
            if(strchr(file_path,'.')==NULL){
                //file with no extension, maybe looking for root url on sub folder 
                //eg: /folder/sub for /folder/sub/index.html

                file_fd = seek_index(module_data->folder,file_path,&output);                    
            }
        }
        
    }

    if(file_fd){
        //found file

        c->response.code=HTTP_OK;            
        c->response.content_type=http_module_file_get_mime_type(full_file_name);

        http_request_header *accept_encoding_header = http_server_connection_get_header(c,HTTP_ACCEPT_ENCODING);
        
        if(accept_encoding_header!=NULL){

            HTTPSERVER_DEBUG("accept-encoding : %s",accept_encoding_header->value);

            if(strstr(accept_encoding_header->value,HTTP_ENCODING_GZIP)!=NULL){
                //request accepts gzip
                
                strcat(full_file_name,".gz");

                int gzip_fd =  vfs_open(full_file_name, "r");
                HTTPSERVER_DEBUG("trying to open gzip %s, fs: %d",full_file_name,gzip_fd);

                if(gzip_fd){

                    //check if file has gzip content 
                    char signature[2];
                    vfs_read(gzip_fd, signature, 2);
                    
                    if (signature[0] == 0x1f && signature[1] == 0x8b)
                    {
                        //we have a gzipped version of the file

                        //rewind file
                        vfs_lseek(gzip_fd, 0, VFS_SEEK_SET);

                        //close old file
                        vfs_close(file_fd); 

                        file_fd=gzip_fd; //swap file descriptors

                        //gzip content encoding
                        http_engine_response_add_header(c,HTTP_CONTENT_ENCODING,HTTP_ENCODING_GZIP);
                    }
                    else{
                        vfs_close(gzip_fd); //close old file
                    }
                    


                }

            }

        }

        unsigned int file_size =vfs_size(file_fd);

        c->response.content_length=file_size;

        http_module_file_response_data *response_data=(http_module_file_response_data*)zalloc(sizeof(http_module_file_response_data));

        response_data->file_size=file_size;
        response_data->file_fd = file_fd;

        *data=(void*)response_data;

        
       

    }

}

http_module* http_module_file_new(const char *path_prefix,const char *fs_folder){

    http_module *module =  http_module_new("HTTP_FILE_SYSTEM");   

    http_module_file_data *module_data = (http_module_file_data*)malloc(sizeof(http_module_file_data));
    
    module->module_data=module_data;

    if(path_prefix==NULL)
        path_prefix="/";

    //adjust prefix
    unsigned int prefix_length=strlen(path_prefix);
    char *prefix = (char*)zalloc(prefix_length+4);
    if(path_prefix[0]!='/')
        strcat(prefix,"/");

    strcat(prefix,path_prefix);

    if(prefix[prefix_length-1]!='/')
        strcat(prefix,"/*");
    else 
        strcat(prefix,"*");
    
    module_data->prefix=prefix;
    http_module_add_url(module,module_data->prefix);

    //adjust folder
    if(fs_folder==NULL || (strcmp(fs_folder,"/")==0) ){
        //serving root of file system
        module_data->folder=NULL;
    }
    else{
        if(fs_folder[0]=='/') fs_folder++; //skip leading '/'
        unsigned int folder_length=strlen(fs_folder);
        char *folder = (char*)malloc(folder_length+1);
        strcpy(folder,fs_folder);
        if(folder[folder_length-1]!='/')
            strcat(folder,"/");

        module_data->folder=folder;
    }
    
    

    http_module_add_header(module,HTTP_ACCEPT_ENCODING);

    module->self_callbacks.on_destroy = http_server_file_on_destroy;
    module->process.on_headers = http_module_file_on_headers;
    module->process.on_send_response=http_module_file_on_send_response;

    http_module_add_header(module,HTTP_ACCESS_CONTROL_REQUEST_HEADERS);
    http_module_add_header(module,HTTP_ACCESS_CONTROL_REQUEST_METHOD);



    return module;


}


