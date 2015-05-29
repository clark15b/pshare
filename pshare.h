#ifndef __PSHARE_H
#define __PSHARE_H

#include <stdio.h>

namespace pshare
{
    struct list
    {
        char* buf;
        int len;
        list* next;
    };

    struct mime
    {
        const char* file_ext;
        const char* upnp_class;
        const char* type;
        const char* dlna_type_extras;
        const char* http_mime_type;
        int container;
    };

    struct playlist_item
    {
        int parent_id;
        int object_id;
        playlist_item* parent;
        int track_number; 
        char* name;
        char* url;
        char* logo_url;
        char* type_extras;
        const char* logo_dlna_profile;
        const mime* type_info;
        int proxy;
        int childs;

        playlist_item* next;
    };

    extern FILE* verb_fp;
    extern int xbox360;
    extern int dlna_extend;            

    extern const char upnp_video[];
    extern const char upnp_audio[];
    extern const char upnp_container[];
    extern const char upnp_folder[];

    extern const char server_name[];

    int get_gmt_date(char* dst,int ndst);

    int print_http_hdrs(FILE* fp,const char* content_type,int extras);
    int print_http_hdrs_no_content(FILE* fp,int extras);
    int print_http_error_hdrs(FILE* fp,const char* status,int extras);

    int do_proxy(playlist_item* item,FILE* fp);
    int do_http_proxy(const char* url,int fd);
}

#endif
