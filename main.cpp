#include "pshare.h"
#include "upnp.h"
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>
#include "soap.h"
#include "tmpl.h"
#include "mem.h"


// TODO: proxy level: 0 - no proxy, 1 - multicast only, 2 - multicast and radio, 3 - all
// TODO: multicast to http proxy (as udpxy)
// TODO: http proxy for radio
// TODO: Build playlist from file system

// PS3: only JPEG logos, no MP3 streaming (need proxy with 'transferMode.dlna.org: Streaming' ?)

// XBox360
//    no video in video folder (Browse)
//    Browse by category - (ContainerID=15 - video? ContainerID=4 - audio? ContainerID=16 - photo)

// WDTV Live: no video and audio
// DLNA headers to stream proxy

// http://www.dlna.org/industry/certification/guidelines/ (IEC61883)
// http://www.cybergarage.org/twiki/bin/view/Main/Windows7DLNA
// http://msdn.microsoft.com/en-us/library/aa468340.aspx
// http://msdn.microsoft.com/en-us/library/ee780971%28PROT.10%29.aspx


namespace pshare
{
    // upnp object classes
    const char upnp_video[]     = "object.item.videoItem";
    const char upnp_audio[]     = "object.item.audioItem.musicTrack";
    const char upnp_container[] = "object.container";
    const char upnp_folder[]    = "object.container.storageFolder";

    // upnp mime types
    const char upnp_avi[]       = "http-get:*:video/avi:";
    const char upnp_asf[]       = "http-get:*:video/x-ms-asf:";
    const char upnp_wmv[]       = "http-get:*:video/x-ms-wmv:";
    const char upnp_mp4[]       = "http-get:*:video/mp4:";
    const char upnp_mpeg[]      = "http-get:*:video/mpeg:";
    const char upnp_mpeg2[]     = "http-get:*:video/mpeg2:";
    const char upnp_mp2t[]      = "http-get:*:video/mp2t:";
    const char upnp_mp2p[]      = "http-get:*:video/mp2p:";
    const char upnp_mov[]       = "http-get:*:video/quicktime:";
    const char upnp_aac[]       = "http-get:*:audio/x-aac:";
    const char upnp_ac3[]       = "http-get:*:audio/x-ac3:";
    const char upnp_mp3[]       = "http-get:*:audio/mpeg:";
    const char upnp_ogg[]       = "http-get:*:audio/x-ogg:";
    const char upnp_wma[]       = "http-get:*:audio/x-ms-wma:";

    // http mime types
    const char http_mime_bin[]  = "application/x-octet-stream";
    const char http_mime_avi[]  = "video/avi";
    const char http_mime_asf[]  = "video/x-ms-asf";
    const char http_mime_wmv[]  = "video/x-ms-wmv";
    const char http_mime_mp4[]  = "video/mp4";
    const char http_mime_mpeg[] = "video/mpeg";
    const char http_mime_mpeg2[]= "video/mpeg2";
    const char http_mime_mp2t[] = "video/mp2t";
    const char http_mime_mp2p[] = "video/mp2p";
    const char http_mime_mov[]  = "video/quicktime";
    const char http_mime_aac[]  = "audio/x-aac";
    const char http_mime_ac3[]  = "audio/x-ac3";
    const char http_mime_mp3[]  = "audio/mpeg";
    const char http_mime_ogg[]  = "application/ogg";
    const char http_mime_wma[]  = "audio/x-ms-wma";

    const char http_mime_xml[]  = "text/xml";
    const char http_mime_html[] = "text/html; charset=\"utf-8\"";
    const char http_mime_text[] = "text/plain";


    const char* protocol_list[]=
        { upnp_avi,upnp_asf,upnp_wmv,upnp_mp4,upnp_mpeg,upnp_mpeg2,upnp_mp2t,upnp_mp2p,upnp_mov,upnp_aac,upnp_ac3,upnp_mp3,upnp_ogg,upnp_wma,0 };


    const char dlna_extras_none[]       = "*";
    const char dlna_extras_radio_mp3[]  = "DLNA.ORG_PN=MP3;DLNA.ORG_OP=01;DLNA.ORG_FLAGS=01700000000000000000000000000000";             // OP=00 - no seek, OP=01 - seek; DLNA.ORG_FLAGS=012000000000000000000000000000000
    const char dlna_extras_radio_wma[]  = "DLNA.ORG_PN=WMAFULL;DLNA.ORG_OP=01;DLNA.ORG_FLAGS=01700000000000000000000000000000";
    const char dlna_extras_radio_ac3[]  = "DLNA.ORG_PN=AC3;DLNA.ORG_OP=01;DLNA.ORG_FLAGS=01700000000000000000000000000000";
    const char dlna_extras_mpeg[]       = "DLNA.ORG_PN=MPEG_PS_PAL;DLNA.ORG_OP=00;DLNA.ORG_FLAGS=01700000000000000000000000000000";     // MPEG_PS_NTSC, MPEG1
    const char dlna_extras_m2ts[]       = "DLNA.ORG_PN=MPEG_TS_HD_NA;DLNA.ORG_OP=00;DLNA.ORG_FLAGS=01700000000000000000000000000000";   // MPEG_TS_SD_NA
    const char dlna_extras_wmv[]        = "DLNA.ORG_PN=WMVHIGH_FULL;DLNA.ORG_OP=00;DLNA.ORG_FLAGS=01700000000000000000000000000000";    // WMVMED_FULL
    const char dlna_extras_avi[]        = "DLNA.ORG_PN=AVI;DLNA.ORG_OP=00;DLNA.ORG_FLAGS=01700000000000000000000000000000";             // PV_DIVX_DX50
    

    mime mime_type_list[]=
    {
        {"mpg"  ,upnp_video,upnp_mpeg,dlna_extras_mpeg,http_mime_mpeg,0},              // default
        {"mpeg" ,upnp_video,upnp_mpeg,dlna_extras_mpeg,http_mime_mpeg,0},
        {"mpeg2",upnp_video,upnp_mpeg2,dlna_extras_mpeg,http_mime_mpeg2,0},
        {"m2v"  ,upnp_video,upnp_mpeg2,dlna_extras_mpeg,http_mime_mpeg2,0},
        {"ts"   ,upnp_video,upnp_mp2t,dlna_extras_m2ts,http_mime_mp2t,0},
        {"m2ts" ,upnp_video,upnp_mp2t,dlna_extras_m2ts,http_mime_mp2t,0},
        {"mts"  ,upnp_video,upnp_mp2t,dlna_extras_m2ts,http_mime_mp2t,0},
        {"vob"  ,upnp_video,upnp_mp2p,dlna_extras_mpeg,http_mime_mp2p,0},
        {"avi"  ,upnp_video,upnp_avi,dlna_extras_avi,http_mime_avi,0},
        {"asf"  ,upnp_video,upnp_asf,dlna_extras_none,http_mime_asf,0},
        {"wmv"  ,upnp_video,upnp_wmv,dlna_extras_wmv,http_mime_wmv,0},
        {"mp4"  ,upnp_video,upnp_mp4,dlna_extras_none,http_mime_mp4,0},
        {"mov"  ,upnp_video,upnp_mov,dlna_extras_none,http_mime_mov,0},
        {"aac"  ,upnp_audio,upnp_aac,dlna_extras_none,http_mime_aac,0},
        {"ac3"  ,upnp_audio,upnp_ac3,dlna_extras_radio_ac3,http_mime_ac3,0},
        {"mp3"  ,upnp_audio,upnp_mp3,dlna_extras_radio_mp3,http_mime_mp3,0},
        {"ogg"  ,upnp_audio,upnp_ogg,dlna_extras_none,http_mime_ogg,0},
        {"wma"  ,upnp_audio,upnp_wma,dlna_extras_radio_wma,http_mime_wma,0},
        {0,0,0,0,0}
    };

    const mime mime_type_none         = {"","","",dlna_extras_none,http_mime_bin,0};
    const mime mime_type_container    = {"",upnp_container,"",dlna_extras_none,http_mime_bin,1};
          mime mime_type_folder       = {"",upnp_container,"",dlna_extras_none,http_mime_bin,1};

    int xbox360=0;
    int dlna_extend=0;

    const char upnp_mgrp[]="239.255.255.250:1900";

    const int upnp_notify_timeout=120;

    const int http_timeout=15;

    const char server_name[]="pShare UPnP Playlist Browser";
    const char* device_name=server_name;

    const char* dev_desc="dev.xml";
    const char* dev_desc_wmc="wmc.xml";

    const char* device_friendly_name="UPnP-IPTV";
    const char manufacturer[]="Anton Burdinuk <clark15b@gmail.com>";
    const char manufacturer_url[]="http://ps3muxer.org/pshare.html";
    const char model_description[]="UPnP Playlist Browser from Anton Burdinuk <clark15b@gmail.com>";
    const char version_number[]="0.0.2";
    const char model_url[]="http://ps3muxer.org/pshare.html";

    int playlist_items_offset=1;

    char device_uuid[64]="";

    int http_port=4044;

    const char www_root_def[]="/opt/share/pshare/www";
    const char pls_root_def[]="/opt/share/pshare/playlists";


    playlist_item* playlist_beg=0;
    playlist_item* playlist_end=0;
    int playlist_counter=0;
    int playlist_track_counter=0;
    playlist_item* playlist_root=0;
    int update_id=0;


    playlist_item* playlist_add(playlist_item* parent,const char* name,const char* url,const mime* type_info,const char* logo_url,const char* type_extras)
    {
        if(!url)
            url="";
        if(!logo_url)
            logo_url="";
        if(!type_extras)
            type_extras="";
        if(!type_info)
            type_info=&mime_type_none;

        if(!name || !*name)
            name=url;

        int name_len=strlen(name)+1;
        int url_len=strlen(url)+1;
        int logo_url_len=strlen(logo_url)+1;
        int type_extras_len=strlen(type_extras)+1;

        int len=name_len+url_len+logo_url_len+type_extras_len+sizeof(playlist_item);
        
        playlist_item* p=(playlist_item*)MALLOC(len);
        if(!p)
            return 0;

        p->parent_id=-1;
        p->parent=0;
        p->track_number=0;
        p->proxy=0;

        if(parent)
        {
            p->parent_id=parent->object_id;
            p->parent=parent;
        }

        if(type_info->upnp_class==upnp_audio)
        {
            playlist_track_counter++;
            p->track_number=playlist_track_counter;
#ifdef WITH_PROXY
            p->proxy=1;
#endif
        }

#ifdef WITH_PROXY
        if(strstr(url,"//@"))
            p->proxy=1;
#endif

        p->object_id=playlist_counter;
        p->name=(char*)(p+1);
        p->url=p->name+name_len;
        p->logo_url=p->url+url_len;
        p->type_extras=p->logo_url+logo_url_len;
        p->type_info=type_info;
        p->childs=0;

        strcpy(p->name,name);
        strcpy(p->url,url);
        strcpy(p->logo_url,logo_url);
        strcpy(p->type_extras,type_extras);
        p->logo_dlna_profile="";

        {
            const char* ptr=strrchr(p->logo_url,'.');
            if(ptr)
            {
                ptr++;
                if(!strcasecmp(ptr,"jpeg") || !strcasecmp(ptr,"jpg"))
                    p->logo_dlna_profile="JPEG_TN";
            }
        }

        p->name[name_len-1]=0;
        p->url[url_len-1]=0;
        p->logo_url[logo_url_len-1]=0;
        p->type_extras[type_extras_len-1]=0;

        p->next=0;

        if(!playlist_beg)
            playlist_beg=playlist_end=p;
        else
        {
            playlist_end->next=p;
            playlist_end=p;
        }

        if(!playlist_counter)
            playlist_counter=playlist_items_offset;
        else
            playlist_counter++;

        if(parent)
            parent->childs++;

        return p;
    }

    void playlist_free(void)
    {
        while(playlist_beg)
        {
            playlist_item* p=playlist_beg;
            playlist_beg=playlist_beg->next;
            FREE(p);            
        }
        playlist_beg=0;
        playlist_end=0;
        playlist_counter=0;
        playlist_track_counter=0;
    }


    playlist_item* playlist_find_by_id(int id)
    {
        for(playlist_item* p=playlist_beg;p;p=p->next)
            if(p->object_id==id)
                return p;
        return 0;
    }


    volatile int __sig_quit=0;
    volatile int __sig_alarm=0;
    volatile int __sig_child=0;
    volatile int __sig_usr1=0;
    int __sig_pipe[2]={-1,-1};
    sigset_t __sig_proc_mask;

    pid_t parent_pid=-1;

    int sock_up=-1;
    int sock_down=-1;
    int sock_http=-1;

    FILE* verb_fp=0;


    list* ssdp_alive_list=0;
    list* ssdp_byebye_list=0;

    upnp::mcast_grp mcast_grp;

    void __sig_handler(int n);

    int signal(int num,void (*handler)(int));

    int get_gmt_date(char* dst,int ndst);

    int parse_playlist_file(const char* name);
    int parse_playlist(const char* name);

    enum {m_get=1, m_post=2, m_subscribe=3, m_unsubscribe=4};

    const char* get_method_name(int n);

    int init_ssdp(void);
    int done_ssdp(void);
    int send_ssdp_alive_notify(void);
    int send_ssdp_byebye_notify(void);
    int send_ssdp_msearch_response(sockaddr_in* sin,const char* st);
    int on_ssdp_message(char* buf,int len,sockaddr_in* sin);
    int on_http_connection(FILE* fp,sockaddr_in* sin);
    int upnp_print_item(FILE* fp,playlist_item* item);
    int upnp_browse(FILE* fp,int object_id,const char* flag,const char* filter,int index,int count);
    const char* search_object_type(const char* exp);
    int upnp_search(FILE* fp,int object_id,const char* what,const char* filter,int index,int count);
    int playlist_browse(const char* req,FILE* fp);

    list* add_to_list(list* lst,const char* s,int len);
    void free_list(list* lst);

    template<typename T>
    T max(T a,T b) { return a>b?a:b; }


    int playlist_load(const char* path)
    {
        if(playlist_root)
            playlist_free();

        playlist_root=playlist_add(0,device_friendly_name,0,&mime_type_container,0,0);
        parse_playlist(path);
        update_id++;
        if(update_id>4096)
            update_id=1;

        return 0;
    }
}

void pshare::__sig_handler(int n)
{
    int err=errno;

    switch(n)
    {
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
        __sig_quit=1;
        break;
    case SIGALRM:
        __sig_alarm=1;
        break;
    case SIGCHLD:
        __sig_child=1;
        break;
    case SIGUSR1:
        __sig_usr1=1;
        break;
    }

    send(__sig_pipe[1],"$",1,MSG_DONTWAIT);

    errno=err;
}


int pshare::signal(int num,void (*handler)(int))
{
    struct sigaction action;

    sigfillset(&action.sa_mask);

    action.sa_flags=(num==SIGCHLD)?SA_NOCLDSTOP:0;

    action.sa_handler=handler;

    return sigaction(num,&action,0);
}

pshare::list* pshare::add_to_list(pshare::list* lst,const char* s,int len)
{
    list* ll=(list*)MALLOC(sizeof(list)+len);
    ll->next=0;
    ll->len=len;
    ll->buf=(char*)(ll+1);
    memcpy(ll->buf,s,len);

    if(lst)
        lst->next=ll;

    return ll;
}

void pshare::free_list(pshare::list* lst)
{
    while(lst)
    {
        list* p=lst;
        lst=lst->next;
        FREE(p);
    }
}


int main(int argc,char** argv)
{
    const char* mcast_iface="";
    int mcast_ttl=0;
    int mcast_loop=0;

    const char* playlist_filename="";
    const char* www_root="";

    const char* app_name=strrchr(argv[0],'/');
    if(app_name)
        app_name++;
    else
        app_name=argv[0];

    if(argc<2)
    {
        fprintf(stderr,"type './%s -?' for help\n",app_name);
        return 1;
    }


    int opt;
    while((opt=getopt(argc,argv,"dvli:u:t:p:n:hr:xe?"))>=0)
        switch(opt)
        {
        case 'd':
            upnp::debug=1;
        case 'v':
            pshare::verb_fp=upnp::verb_fp=stderr;
            break;
        case 'l':
            mcast_loop=1;
            break;
        case 'i':
            mcast_iface=optarg;
            break;
        case 'u':
            snprintf(pshare::device_uuid,sizeof(pshare::device_uuid),"%s",optarg);
            break;
        case 't':
            mcast_ttl=atoi(optarg);
            break;
        case 'p':
            pshare::http_port=atoi(optarg);
            break;
        case 'n':
            pshare::device_friendly_name=optarg;
            break;
        case 'r':
            www_root=optarg;
            break;
        case 'x':
            pshare::xbox360=1;
            break;
        case 'e':
            pshare::dlna_extend=1;
            break;
        case 'h':
        case '?':
            fprintf(stderr,"%s %s UPnP Playlist Browser\n",app_name,pshare::version_number);
            fprintf(stderr,"\nThis program is a simple DLNA Media Server which provides ContentDirectory:1 service\n",app_name);
            fprintf(stderr,"    for sharing IPTV and Radio unicast streams over local area network ('udpxy' needed for multicast).\n\n");
            fprintf(stderr,"Copyright (C) 2010 Anton Burdinuk\n\n");
            fprintf(stderr,"clark15b@gmail.com\n");
            fprintf(stderr,"http://code.google.com/p/tsdemuxer\n\n");


            fprintf(stderr,"USAGE: ./%s [-v] [-l] [-x] [-e] -i iface [-u device_uuid] [-t mcast_ttl] [-p http_port] [-r www_root] [playlist]\n",app_name);
            fprintf(stderr,"   -x          XBox 360 compatible mode\n");
            fprintf(stderr,"   -e          DLNA protocolInfo extend (DLNA profiles)\n");
            fprintf(stderr,"   -v          Turn on verbose output\n");
            fprintf(stderr,"   -d          Turn on verbose output + debug messages\n");
            fprintf(stderr,"   -l          Turn on loopback multicast transmission\n");
            fprintf(stderr,"   -i          Multicast interface address or device name\n");
            fprintf(stderr,"   -u          DLNA server UUID\n");
            fprintf(stderr,"   -n          DLNA server friendly name\n");
            fprintf(stderr,"   -t          Multicast datagrams time-to-live (TTL)\n");
            fprintf(stderr,"   -p          TCP port for incoming HTTP connections\n");
            fprintf(stderr,"   -r          WWW root directory (default: '%s')\n",pshare::www_root_def);
            fprintf(stderr,"    playlist   single file or directory absolute path with utf8-encoded *.m3u files (default: '%s')\n\n",pshare::pls_root_def);
            fprintf(stderr,"example 1: './%s -i eth0 %s/playlist.m3u'\n",app_name,pshare::pls_root_def);
            fprintf(stderr,"example 2: './%s -v -i 192.168.1.1 -u 32ccc90a-27a7-494a-a02d-71f8e02b1937 -n IPTV -t 1 -p 4044 %s/'\n\n",app_name,pshare::pls_root_def);

            fprintf(stderr,"known files: ");
            for(int i=0;pshare::mime_type_list[i].file_ext;i++)
            {
                if(i)
                    fprintf(stderr,",");

                fprintf(stderr,"%s",pshare::mime_type_list[i].file_ext);
            }
            fprintf(stderr,"\n\n");

            return 0;
        }


    if(argc>optind)
        playlist_filename=argv[optind];
    else
        playlist_filename=pshare::pls_root_def;

    if(*playlist_filename!='/')
    {
        fprintf(stderr,"enter absolute path for '%s'\n",playlist_filename);
        return 1;
    }

    if(!*mcast_iface)
    {
        fprintf(stderr,"enter interface name of address ('-i' option)\n");
        return 1;
    }

    if(!*www_root)
        www_root=pshare::www_root_def;

    if(mcast_ttl<1)
        mcast_ttl=1;
    else if(mcast_ttl>4)
        mcast_ttl=4;

    if(pshare::http_port<0)
        pshare::http_port=0;

    if(pshare::xbox360)
    {
        pshare::playlist_items_offset=100;
        pshare::dev_desc=pshare::dev_desc_wmc;
        pshare::mime_type_folder.upnp_class=pshare::upnp_folder;
        pshare::dlna_extend=1;
    }

    if(!pshare::dlna_extend)
    {
        for(int i=0;pshare::mime_type_list[i].file_ext;i++)
            pshare::mime_type_list[i].dlna_type_extras=pshare::dlna_extras_none;
    }


    upnp::uuid_init();

    if(!*pshare::device_uuid)
    {
        char filename[512]="";
        snprintf(filename,sizeof(filename),"/var/tmp/%s.uuid",pshare::device_friendly_name);

        FILE* fp=fopen(filename,"r");
        if(fp)
        {
            int n=fread(pshare::device_uuid,1,sizeof(pshare::device_uuid)-1,fp);
            pshare::device_uuid[n]=0;
            fclose(fp);
        }

        if(!*pshare::device_uuid)
        {
            upnp::uuid_gen(pshare::device_uuid);

            FILE* fp=fopen(filename,"w");
            if(fp)
            {
                fprintf(fp,"%s",pshare::device_uuid);
                fclose(fp);
            }
        }
    }

    printf("starting UPnP service '%s'...\n",pshare::device_friendly_name);

    if(!pshare::verb_fp)
    {
        pid_t pid=fork();
        if(pid==-1)
        {
            perror("fork");
            return 1;
        }else if(pid)
        {
            printf("web port=%i, uuid=%s, pid=%d\n",pshare::http_port,pshare::device_uuid,pid);
            exit(0);
        }

        int fd=open("/dev/null",O_RDWR);
        if(fd!=-1)
        {
            for(int i=0;i<3;i++)
                dup2(fd,i);
            close(fd);
        }else
            for(int i=0;i<3;i++)
                close(i);
    }

    pshare::parent_pid=getpid();

    pshare::playlist_load(playlist_filename);

//pshare::upnp_search(stdout,0,"upnp:class derivedfrom \"object.item.videoItem.sas\" and @refID exists false","",0,0);
//pshare::upnp_search(stdout,0,"upnp:class derivedfrom \"object.item.audioItem.sas\" and @refID exists false","",0,0);
//pshare::upnp_search(stdout,0,"(upnp:class derivedfrom \"object.item.audioItem\")","",0,0);
//pshare::upnp_search(stdout,0,"(upnp:class = \"object.container.album.musicAlbum\")","",0,0);
//pshare::upnp_search(stdout,0,"(upnp:class = \"object.item.videoItem.as\")","",0,0);
//pshare::upnp_search(stdout,0,"*","",0,0);

    setsid();

    sigfillset(&pshare::__sig_proc_mask);
    socketpair(PF_UNIX,SOCK_STREAM,0,pshare::__sig_pipe);

    pshare::signal(SIGHUP ,SIG_IGN);
    pshare::signal(SIGPIPE,SIG_IGN);
    pshare::signal(SIGINT ,pshare::__sig_handler);
    pshare::signal(SIGQUIT,pshare::__sig_handler);
    pshare::signal(SIGTERM,pshare::__sig_handler);
    pshare::signal(SIGALRM,pshare::__sig_handler);
    pshare::signal(SIGCHLD,pshare::__sig_handler);
    pshare::signal(SIGUSR1,pshare::__sig_handler);

    sigprocmask(SIG_BLOCK,&pshare::__sig_proc_mask,0);

    pshare::mcast_grp.init(pshare::upnp_mgrp,mcast_iface,mcast_ttl,mcast_loop);

    setenv("DEV_FNAME",pshare::device_friendly_name,1);
    setenv("DEV_NAME",pshare::device_name,1);
    setenv("DEV_UUID",pshare::device_uuid,1);
    setenv("DEV_HOST",pshare::mcast_grp.interface,1);

    {
        char bb[32]; sprintf(bb,"%i",pshare::http_port);
        setenv("DEV_PORT",bb,1);
    }

    setenv("MANUFACTURER",pshare::manufacturer,1);
    setenv("MANUFACTURER_URL",pshare::manufacturer_url,1);
    setenv("MODEL_DESCRIPTION",pshare::model_description,1);
    setenv("VERSION_NUMBER",pshare::version_number,1);
    setenv("MODEL_URL",pshare::model_url,1);


    if(*www_root)
    {
        int rc=chdir(www_root);
    }

    if(pshare::verb_fp)
    {
        fprintf(pshare::verb_fp,"root device uuid: '%s'\n",pshare::device_uuid);
        fprintf(pshare::verb_fp,"device friendly name: '%s'\n",pshare::device_friendly_name);
    }
    pshare::sock_up=pshare::mcast_grp.upstream();

    if(pshare::sock_up!=-1)
    {
        pshare::sock_down=pshare::mcast_grp.join();

        if(pshare::sock_down!=-1)
        {
            pshare::sock_http=upnp::create_tcp_listener(pshare::http_port);

            if(pshare::sock_http!=-1)
            {
                fcntl(pshare::sock_http,F_SETFL,fcntl(pshare::sock_http,F_GETFL,0)|O_NONBLOCK);

                pshare::init_ssdp();

                alarm(pshare::upnp_notify_timeout);

                int maxfd=pshare::max(pshare::sock_up,pshare::sock_down);
                maxfd=pshare::max(maxfd,pshare::__sig_pipe[0]);
                maxfd=pshare::max(maxfd,pshare::sock_http);
                maxfd++;

                pshare::send_ssdp_alive_notify();

                while(!pshare::__sig_quit)
                {
                    fd_set fdset;

                    FD_ZERO(&fdset);
                    FD_SET(pshare::sock_up,&fdset);
                    FD_SET(pshare::sock_down,&fdset);
                    FD_SET(pshare::__sig_pipe[0],&fdset);
                    FD_SET(pshare::sock_http,&fdset);

                    sigprocmask(SIG_UNBLOCK,&pshare::__sig_proc_mask,0);

                    int rc=select(maxfd,&fdset,0,0,0);

                    sigprocmask(SIG_BLOCK,&pshare::__sig_proc_mask,0);

                    if(rc==-1)
                    {
                        if(errno!=EINTR)
                            break;
                    }

                    if(rc>0)
                    {
                        char tmp[1024];

                        if(FD_ISSET(pshare::__sig_pipe[0],&fdset))
                            while(recv(pshare::__sig_pipe[0],tmp,sizeof(tmp),MSG_DONTWAIT)>0);

                        if(FD_ISSET(pshare::sock_up,&fdset))
                            while(recv(pshare::sock_up,tmp,sizeof(tmp),MSG_DONTWAIT)>0);

                        if(FD_ISSET(pshare::sock_down,&fdset))
                        {
                            sockaddr_in sin;

                            int n;

                            while((n=pshare::mcast_grp.recv(pshare::sock_down,tmp,sizeof(tmp)-1,&sin,MSG_DONTWAIT))>0)
                            {
                                tmp[n]=0;

                                pshare::on_ssdp_message(tmp,n,&sin);
                            }
                        }

                        if(FD_ISSET(pshare::sock_http,&fdset))
                        {
                            sockaddr_in sin;
                            socklen_t sin_len=sizeof(sin);

                            int fd;
                            while((fd=accept(pshare::sock_http,(sockaddr*)&sin,&sin_len))!=-1)
                            {
                                int on=1;
                                setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&on,sizeof(on));

                                pid_t pid=fork();

                                if(!pid)
                                {
                                    close(pshare::sock_up);
                                    close(pshare::sock_down);
                                    close(pshare::sock_http);
                                    for(int i=0;i<sizeof(pshare::__sig_pipe)/sizeof(*pshare::__sig_pipe);i++)
                                        close(pshare::__sig_pipe[i]);
                                    pshare::done_ssdp();

                                    upnp::uuid_init();

                                    pshare::signal(SIGHUP ,SIG_DFL);
                                    pshare::signal(SIGPIPE,SIG_IGN);
                                    pshare::signal(SIGINT ,SIG_DFL);
                                    pshare::signal(SIGQUIT,SIG_DFL);
                                    pshare::signal(SIGTERM,SIG_DFL);
                                    pshare::signal(SIGALRM,SIG_DFL);
                                    pshare::signal(SIGCHLD,SIG_DFL);
                                    pshare::signal(SIGUSR1,SIG_DFL);

                                    int on=1;
                                    setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&on,sizeof(on));

                                    sigprocmask(SIG_UNBLOCK,&pshare::__sig_proc_mask,0);

                                    FILE* fp=fdopen(fd,"a+");

                                    if(fp)
                                    {
                                        pshare::on_http_connection(fp,&sin);
                                        fclose(fp);
                                    }else
                                        close(fd);


                                    pshare::playlist_free();
#ifdef DEBUG
                                    fprintf(stderr,"MEMORY USAGE (child): %i\n",mem::mem_total);
#endif

                                    exit(0);
                                }

                                close(fd);
                            }
                        }
                    }

                    if(pshare::__sig_child)
                    {
                        pshare::__sig_child=0;

                        while(wait3(0,WNOHANG,0)>0);
                    }

                    if(pshare::__sig_alarm)
                    {
                        pshare::__sig_alarm=0;
                        alarm(pshare::upnp_notify_timeout);

                        pshare::send_ssdp_alive_notify();
                    }

                    if(pshare::__sig_usr1)
                    {
                        pshare::__sig_usr1=0;
                        pshare::playlist_load(playlist_filename);
                    }

                }

                pshare::send_ssdp_byebye_notify();

                pshare::done_ssdp();

                close(pshare::sock_http);
            }

            pshare::mcast_grp.leave(pshare::sock_down);
            pshare::mcast_grp.close(pshare::sock_down);
        }

        pshare::mcast_grp.close(pshare::sock_up);
    }

    pshare::playlist_free();

    pshare::signal(SIGTERM,SIG_IGN);

    sigprocmask(SIG_UNBLOCK,&pshare::__sig_proc_mask,0);

    kill(0,SIGTERM);

    for(int i=0;i<sizeof(pshare::__sig_pipe)/sizeof(*pshare::__sig_pipe);i++)
        close(pshare::__sig_pipe[i]);

#ifdef DEBUG
    fprintf(stderr,"MEMORY USAGE (main): %i\n",mem::mem_total);
#endif

    return 0;
}

int pshare::init_ssdp(void)
{
    char tmp[1024]="";
    int n;

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/%s\r\n"
        "NT: uuid:%s\r\n"
        "NTS: ssdp:alive\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s\r\n\r\n",
        mcast_grp.interface,http_port,dev_desc,device_uuid,server_name,device_uuid);

    list* ll=0; ssdp_alive_list=ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/%s\r\n"
        "NT: upnp:rootdevice\r\n"
        "NTS: ssdp:alive\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::upnp:rootdevice\r\n\r\n",
        mcast_grp.interface,http_port,dev_desc,server_name,device_uuid);

    ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/%s\r\n"
        "NT: urn:schemas-upnp-org:device:MediaServer:1\r\n"
        "NTS: ssdp:alive\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:device:MediaServer:1\r\n\r\n",
        mcast_grp.interface,http_port,dev_desc,server_name,device_uuid);

    ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/%s\r\n"
        "NT: urn:schemas-upnp-org:service:ContentDirectory:1\r\n"
        "NTS: ssdp:alive\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:service:ContentDirectory:1\r\n\r\n",
        mcast_grp.interface,http_port,dev_desc,server_name,device_uuid);

    ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/%s\r\n"
        "NT: urn:schemas-upnp-org:service:ConnectionManager:1\r\n"
        "NTS: ssdp:alive\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:service:ConnectionManager:1\r\n\r\n",
        mcast_grp.interface,http_port,dev_desc,server_name,device_uuid);

    ll=add_to_list(ll,tmp,n);


    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/%s\r\n"
        "NT: urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1\r\n"
        "NTS: ssdp:alive\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1\r\n\r\n",
        mcast_grp.interface,http_port,dev_desc,server_name,device_uuid);

    ll=add_to_list(ll,tmp,n);


    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/%s\r\n"
        "NT: uuid:%s\r\n"
        "NTS: ssdp:byebye\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s\r\n\r\n",
        mcast_grp.interface,http_port,dev_desc,device_uuid,server_name,device_uuid);

    ll=0; ssdp_byebye_list=ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/%s\r\n"
        "NT: upnp:rootdevice\r\n"
        "NTS: ssdp:byebye\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::upnp:rootdevice\r\n\r\n",
        mcast_grp.interface,http_port,dev_desc,server_name,device_uuid);

    ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/%s\r\n"
        "NT: urn:schemas-upnp-org:device:MediaServer:1\r\n"
        "NTS: ssdp:byebye\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:device:MediaServer:1\r\n\r\n",
        mcast_grp.interface,http_port,dev_desc,server_name,device_uuid);

    ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/%s\r\n"
        "NT: urn:schemas-upnp-org:service:ContentDirectory:1\r\n"
        "NTS: ssdp:byebye\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:service:ContentDirectory:1\r\n\r\n",
        mcast_grp.interface,http_port,dev_desc,server_name,device_uuid);

    ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/%s\r\n"
        "NT: urn:schemas-upnp-org:service:ConnectionManager:1\r\n"
        "NTS: ssdp:byebye\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:service:ConnectionManager:1\r\n\r\n",
        mcast_grp.interface,http_port,dev_desc,server_name,device_uuid);

    ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/%s\r\n"
        "NT: urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1\r\n"
        "NTS: ssdp:byebye\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1\r\n\r\n",
        mcast_grp.interface,http_port,dev_desc,server_name,device_uuid);

    ll=add_to_list(ll,tmp,n);

    return 0;
}

int pshare::done_ssdp(void)
{
    free_list(ssdp_alive_list);
    free_list(ssdp_byebye_list);
    return 0;
}

int pshare::send_ssdp_alive_notify(void)
{
    for(list* l=ssdp_alive_list;l;l=l->next)
        mcast_grp.send(sock_up,l->buf,l->len);

    return 0;
}

int pshare::send_ssdp_byebye_notify(void)
{
    for(list* l=ssdp_byebye_list;l;l=l->next)
        mcast_grp.send(sock_up,l->buf,l->len);

    return 0;
}

int pshare::on_ssdp_message(char* buf,int len,sockaddr_in* sin)
{
    char tmp[1024];

    int line=0;

    int ignore=0;

    char what[256]="";

    for(int i=0,j=0;i<len && !ignore;i++)
    {
        if(buf[i]!='\r')
        {
            if(buf[i]!='\n')
            {
                if(j<sizeof(tmp)-1)
                {
                    ((unsigned char*)tmp)[j]=((unsigned char*)buf)[i];
                    j++;
                }
            }else
            {
                tmp[j]=0;

                if(j>0)
                {
                    if(!line)
                    {
                        char* p=strchr(tmp,' ');
                        if(p)
                        {
                            *p=0;
                            if(strcasecmp(tmp,"M-SEARCH"))
                                ignore=1;
                        }
                    }else
                    {
                        char* p=strchr(tmp,':');
                        if(p)
                        {
                            *p=0; p++;
                            while(*p && *p==' ')
                                p++;

                            if(!strcasecmp(tmp,"MAN"))
                            {
                                if(strcasecmp(p,"\"ssdp:discover\""))
                                    ignore=1;
                            }else if(!strcasecmp(tmp,"ST"))
                            {
                                int n=snprintf(what,sizeof(what),"%s",p);
                                if(n==-1 || n>=sizeof(what))
                                    what[sizeof(what)-1]=0;
                            }
                        }
                    }
                }
                j=0;
                line++;
            }
        }
    }

    if(!ignore)
        send_ssdp_msearch_response(sin,what);

    return 0;
}

int pshare::send_ssdp_msearch_response(sockaddr_in* sin,const char* st)
{
    const char* what=st;

    static const char dev_type[]="urn:schemas-upnp-org:device:MediaServer:1";

    if(strcmp(st,"ssdp:all") && strcmp(st,"upnp:rootdevice"))
    {
        static const char tag[]="urn:";
        static const char tag2[]="uuid:";

        if(!strncmp(st,tag,sizeof(tag)-1))
        {
            const char* p=st+sizeof(tag)-1;

            if(strcmp(p,"schemas-upnp-org:device:MediaServer:1") && strcmp(p,"schemas-upnp-org:service:ContentDirectory:1") &&
                strcmp(p,"schemas-upnp-org:service:ConnectionManager:1") && strcmp(p,"microsoft.com:service:X_MS_MediaReceiverRegistrar:1"))
                    return -1;
        }else if(!strncmp(st,tag2,sizeof(tag2)-1))
        {
            const char* p=st+sizeof(tag2)-1;

            if(strcmp(p,device_uuid))
                    return -1;

            what=dev_type;
        }
    }else
        what=dev_type;


    char date[64], tmp[1024];

    get_gmt_date(date,sizeof(date));

    int n=snprintf(tmp,sizeof(tmp),
        "HTTP/1.1 200 OK\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "DATE: %s\r\n"
        "EXT:\r\n"
        "LOCATION: http://%s:%i/t/%s\r\n"
        "Server: %s\r\n"
        "ST: %s\r\n"
        "USN: uuid:%s::%s\r\n\r\n",
        date,mcast_grp.interface,http_port,dev_desc,server_name,st,device_uuid,what);

    mcast_grp.send(sock_up,tmp,n,sin);

    return 0;
}

const char* pshare::get_method_name(int n)
{
    switch(n)
    {
    case m_get: return "GET"; break;
    case m_post: return "POST"; break;
    case m_subscribe: return "SUBSCRIBE"; break;
    case m_unsubscribe: return "UNSUBSCRIBE"; break;
    }
    return "";
}

int pshare::on_http_connection(FILE* fp,sockaddr_in* sin)
{
    alarm(http_timeout);

    do
    {
        // read HTTP request

        int method=0;

        char req[128]="";

        char soap_action[128]="";

        char sid[96]="";
        char timeout[64]="";

        int content_length=-1;

        char* content=0;

        char tmp[1024];

        int line=0;

        while(fgets(tmp,sizeof(tmp),fp))
        {
            char* p=strpbrk(tmp,"\r\n");
            if(p)
                *p=0;

            if(!*tmp)
                break;

            if(!line)
            {
                char* uri=strchr(tmp,' ');
                if(uri)
                {
                    *uri=0; uri++;
                    while(*uri && *uri==' ')
                        uri++;

                    char* ver=strchr(uri,' ');
                    if(ver)
                    {
                        *ver=0; ver++;
                        while(*ver && *ver==' ')
                            ver++;

                        if(!strcasecmp(tmp,"GET"))
                            method=m_get;
                        else if(!strcasecmp(tmp,"POST"))
                            method=m_post;
                        else if(!strcasecmp(tmp,"SUBSCRIBE"))
                            method=m_subscribe;
                        else if(!strcasecmp(tmp,"UNSUBSCRIBE"))
                            method=m_unsubscribe;

                        snprintf(req,sizeof(req),"%s",uri);
                    }
                }
            }else
            {
                p=strchr(tmp,':');
                if(p)
                {
                    *p=0; p++;
                    while(*p && *p==' ')
                        p++;

                    if(!strcasecmp(tmp,"Content-Length"))
                    {
                        char* endptr=0;
                        int ll=strtol(p,&endptr,10);

                        if((!*endptr || *endptr==' ') && ll>=0)
                            content_length=ll;
                    }else if(!strcasecmp(tmp,"SOAPAction"))
                    {
                        if(*p=='\"')
                            p++;

                        int n=snprintf(soap_action,sizeof(soap_action),"%s",p);
                        if(n==-1 || n>=sizeof(soap_action))
                        {
                            n=sizeof(soap_action)-1;
                            soap_action[n]=0;
                        }
                        if(n>0 && soap_action[n-1]=='\"')
                            soap_action[n-1]=0;
                    }else if(!strcasecmp(tmp,"SID"))
                    {
                        int n=snprintf(sid,sizeof(sid),"%s",p);
                        if(n==-1 || n>=sizeof(sid))
                            sid[sizeof(sid)-1]=0;
                    }else if(!strcasecmp(tmp,"TIMEOUT"))
                    {
                        int n=snprintf(timeout,sizeof(timeout),"%s",p);
                        if(n==-1 || n>=sizeof(timeout))
                            timeout[sizeof(timeout)-1]=0;
                    }                    
                }
            }

            line++;
        }

        if(!method || !*req || content_length>4096 || (method==m_get && content_length>0))
            break;

        if(content_length>0)
        {
            content=(char*)MALLOC(content_length+1);
            if(!content)
                break;

            int l=0;
            while(l<content_length)
            {
                int n=fread(content+l,1,content_length-l,fp);
                if(!n)
                    break;
                l+=n;
            }

            if(l!=content_length)
            {
                FREE(content);
                break;
            }

            content[content_length]=0;
        }


        if(verb_fp)
        {
            fprintf(verb_fp,"%s '%s' from '%s:%i'\n",get_method_name(method),req,inet_ntoa(sin->sin_addr),ntohs(sin->sin_port));

            if(upnp::debug && content)
            {
                fwrite(content,content_length,1,verb_fp);
                fprintf(verb_fp,"\n");
            }
        }

        const char* soap_req_type="";

        int soap_error=0;

        soap::node soap_req;

        if(*soap_action)
        {
            if(verb_fp)
                fprintf(verb_fp,"SOAPAction: %s\n",soap_action);

            soap_req_type=strchr(soap_action,'#');
            if(soap_req_type)
                soap_req_type++;
            else
                soap_req_type="";

            if(content && content_length>0)
                soap::parse(content,content_length,&soap_req);
        }

        static const char ttag[]="/t/";
        static const char ptag[]="/p/";
        static const char prtag[]="/pr/";

        if(!strcmp(req,"/"))
            tmpl::get_file("index.html",fp,1);
        else if(!strcmp(req,"/cds_control"))    // ContentDirectory
        {
            print_http_hdrs(fp,http_mime_xml,0);

            if(!strcmp(soap_req_type,"Browse"))
            {
                soap::node* req_data=soap_req.find("Envelope/Body/Browse");

                if(req_data)
                {
                    const char* obj_id=req_data->find_data("ObjectID");

                    if(!*obj_id)
                        obj_id=req_data->find_data("ContainerID");

                    int object_id=atoi(obj_id);

                    if(object_id<playlist_items_offset)
                        object_id=0;

                    const char* flag=req_data->find_data("BrowseFlag");
                    const char* filter=req_data->find_data("Filter");
                    int index=atoi(req_data->find_data("StartingIndex"));
                    int count=atoi(req_data->find_data("RequestedCount"));

                    if(verb_fp)
                        fprintf(verb_fp,"Browse: ObjectID=%i, BrowseFlag='%s', StartingIndex=%i, RequestedCount=%i\n",
                            object_id,flag,index,count);

                    upnp_browse(fp,object_id,flag,filter,index,count);
                }
            }else if(!strcmp(soap_req_type,"Search"))
            {
                soap::node* req_data=soap_req.find("Envelope/Body/Search");

                if(req_data)
                {
                    int object_id=atoi(req_data->find_data("ContainerID"));

                    if(object_id<playlist_items_offset)
                        object_id=0;

                    const char* what=req_data->find_data("SearchCriteria");
                    const char* filter=req_data->find_data("Filter");
                    int index=atoi(req_data->find_data("StartingIndex"));
                    int count=atoi(req_data->find_data("RequestedCount"));

                    if(verb_fp)
                        fprintf(verb_fp,"Search: ContainerID=%i, SearchCriteria='%s', StartingIndex=%i, RequestedCount=%i\n",
                            object_id,what,index,count);

                    upnp_search(fp,object_id,what,filter,index,count);
                }
            }else if(!strcmp(soap_req_type,"GetSystemUpdateID"))
            {
                fprintf(fp,
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
                    "   <s:Body>\n"
                    "      <u:GetSystemUpdateIDResponse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">\n"
                    "         <Id>%i</Id>\n"
                    "      </u:GetSystemUpdateIDResponse>\n"
                    "   </s:Body>\n"
                    "</s:Envelope>\n",pshare::update_id);                
            }else if(!strcmp(soap_req_type,"GetSortCapabilities"))
            {
                fprintf(fp,
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
                    "   <s:Body>\n"
                    "      <u:GetSortCapabilitiesResponse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">\n"
                    "         <SortCaps>dc:title</SortCaps>\n"
                    "      </u:GetSortCapabilitiesResponse>\n"
                    "   </s:Body>\n"
                    "</s:Envelope>\n");                
            }else
                soap_error=1;
        }
        else if(!strcmp(req,"/msr_control"))    // MediaReceiverRegistrar
        {
            print_http_hdrs(fp,http_mime_xml,0);

            if(!strcmp(soap_req_type,"IsAuthorized") || !strcmp(soap_req_type,"IsValidated"))
            {
                fprintf(fp,
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
                    "   <s:Body>\n"
                    "      <u:%sResponse xmlns:u=\"urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1\">\n"
                    "         <Result>1</Result>\n"
                    "      </u:%sResponse>\n"
                    "   </s:Body>\n"
                    "</s:Envelope>\n",soap_req_type,soap_req_type);
            }
/*            else if(!strcmp(soap_req_type,"RegisterDevice"))
            {
                fprintf(fp,
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
                    "   <s:Body>\n"
                    "      <u:RegisterDeviceResponse xmlns:u=\"urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1\">\n"
                    "         <RegistrationRespMsg></RegistrationRespMsg>\n"
                    "      </u:RegisterDeviceResponse>\n"
                    "   </s:Body>\n"
                    "</s:Envelope>\n");
            }*/
            else
                soap_error=1;
        }
        else if(!strcmp(req,"/cms_control"))    // ConnectionManager
        {
            print_http_hdrs(fp,http_mime_xml,0);

            if(!strcmp(soap_req_type,"GetCurrentConnectionInfo"))
            {
                fprintf(fp,
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
                    "   <s:Body>\n"
                    "      <u:GetCurrentConnectionInfoResponse xmlns:u=\"urn:schemas-upnp-org:service:ConnectionManager:1\">\n"
                    "         <ConnectionID>0</ConnectionID>\n"
                    "         <RcsID>-1</RcsID>\n"
                    "         <AVTransportID>-1</AVTransportID>\n"
                    "         <ProtocolInfo></ProtocolInfo>\n"
                    "         <PeerConnectionManager></PeerConnectionManager>\n"
                    "         <PeerConnectionID>-1</PeerConnectionID>\n"
                    "         <Direction>Output</Direction>\n"
                    "         <Status>OK</Status>\n"
                    "      </u:GetCurrentConnectionInfoResponse>\n"
                    "   </s:Body>\n"
                    "</s:Envelope>\n");
            }
            else if(!strcmp(soap_req_type,"GetProtocolInfo"))
            {
                fprintf(fp,
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
                    "   <s:Body>\n"
                    "      <u:GetProtocolInfoResponse xmlns:u=\"urn:schemas-upnp-org:service:ConnectionManager:1\">\n"
                    "         <Source>");

                for(int i=0;protocol_list[i];i++)
                {
                    if(i)
                        fprintf(fp,",");
                    fprintf(fp,"%s*",protocol_list[i]);
                }


                fprintf(fp,
                    "</Source>\n"
                    "         <Sink></Sink>\n"
                    "      </u:GetProtocolInfoResponse>\n"
                    "   </s:Body>\n"
                    "</s:Envelope>\n");

            }
            else if(!strcmp(soap_req_type,"GetCurrentConnectionIDs"))
            {
                fprintf(fp,
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
                    "   <s:Body>\n"
                    "      <u:GetCurrentConnectionIDsResponse xmlns:u=\"urn:schemas-upnp-org:service:ConnectionManager:1\">\n"
                    "         <ConnectionIDs></ConnectionIDs>\n"
                    "      </u:GetCurrentConnectionIDsResponse>\n"
                    "   </s:Body>\n"
                    "</s:Envelope>\n");
            }else
                soap_error=1;
        }
        else if(!strcmp(req,"/reload"))
        {
            int rc=kill(parent_pid,SIGUSR1);

            print_http_hdrs(fp,http_mime_text,0);
            fprintf(fp,"%s",rc?"FAIL":"OK");
        }
        else if(!strcmp(req,"/cds_event") || !strcmp(req,"/cms_event") || !strcmp(req,"/msr_event"))
        {
            print_http_hdrs_no_content(fp,1);

            if(method==m_subscribe)
            {
                if(!*sid)
                {
                    upnp::uuid_gen(sid);

                    fprintf(fp,"SID: uuid:%s\r\n",sid);
                }else
                    fprintf(fp,"SID: %s\r\n",sid);

                fprintf(fp,"TIMEOUT: %s\r\n",*timeout?timeout:"Second-1800");

            }

            fprintf(fp,"\r\n");
        }
        else if(!strncmp(req,ttag,sizeof(ttag)-1))
            tmpl::get_file(req+sizeof(ttag)-1,fp,1);
        else if(!strncmp(req,ptag,sizeof(ptag)-1))
            playlist_browse(req+sizeof(ptag)-1,fp);
        else if(!strncmp(req,prtag,sizeof(prtag)-1))
        {
            char* p1=req+(sizeof(prtag)-1);
            char* p2=strchr(p1,'/');

            int rc=-1;

            if(p2)
            {
                *p2=0;

                int object_id=strtol(p1,&p2,10);

                if(!*p2 && object_id>0)
                {
                    playlist_item* item=playlist_find_by_id(object_id);

                    if(item)
                    {
                        alarm(0);
                        rc=do_proxy(item,fp);
                    }
                }
            }

            if(rc)
                print_http_error_hdrs(fp,"404 Not found",0);

        }else
            tmpl::get_file(req,fp,0);

        if(soap_error)
        {
            fprintf(fp,
                "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
                "   <s:Body>\n"
                "      <s:Fault>\n"
                "         <faultcode>s:Client</faultcode>\n"
                "         <faultstring>UPnPError</faultstring>\n"
                "         <detail>\n"
                "            <u:UPnPError xmlns:u=\"urn:schemas-upnp-org:control-1-0\">\n"
                "               <u:errorCode>501</u:errorCode>\n"
                "               <u:errorDescription>Action Failed</u:errorDescription>\n"
                "            </u:UPnPError>\n"
                "         </detail>\n"
                "      </s:Fault>\n"
                "   </s:Body>\n"
                "</s:Envelope>\n");
        }

        fflush(fp);

        if(content)
            FREE(content);

        alarm(http_timeout);


    }while(0);

    alarm(0);

    return 0;
}


int pshare::parse_playlist(const char* name)
{
    char buf[512];

    int n=snprintf(buf,sizeof(buf),"%s/",name);
    if(n==-1 || n>=sizeof(buf))
        n=sizeof(buf)-1;

    if(n>1 && buf[n-2]=='/')
        n--;

    int m=sizeof(buf)-n;

    DIR* dfp=opendir(name);

    if(dfp)
    {
        dirent* d=0;

        while((d=readdir(dfp)))
        {
            if(*d->d_name!='.')
            {
                int nn=snprintf(buf+n,m,"%s",d->d_name);
                if(nn==-1 || nn>=m)
                    buf[sizeof(buf)-1]=0;

                parse_playlist_file(buf);
            }
        }

        closedir(dfp);
    }else
        parse_playlist_file(name);
    
    return 0;
}

int pshare::parse_playlist_file(const char* name)
{
    const char* ext=strrchr(name,'.');

    if(!ext || strcasecmp(ext+1,"m3u"))
        return -1;

    FILE* fp=fopen(name,"r");
    
    if(fp)
    {
        char playlist_name[64]="";

        {
            const char* fname=strrchr(name,'/');
            if(!fname)
                fname=name;
            else
                fname++;

            int n=ext-fname;

            if(n>=sizeof(playlist_name))
                n=sizeof(playlist_name)-1;
            memcpy(playlist_name,fname,n);
            playlist_name[n]=0;
        }
    
        if(verb_fp)
            fprintf(verb_fp,"playlist: '%s' -> %s\n",playlist_name,name);

        playlist_item* parent_item=playlist_add(playlist_root,playlist_name,0,&mime_type_folder,0,0);

        char track_name[64]="";
        char track_url[256]="";
        char logo_url[256]="";
        char file_type[64]="";
        char file_type_ext[256]="";
        const mime* type_info=0;

        char buf[256];
        while(fgets(buf,sizeof(buf),fp))
        {
            char* beg=buf;
            while(*beg && (*beg==' ' || *beg=='\t'))
                beg++;

            char* p=strpbrk(beg,"\r\n");
            if(p)
                *p=0;
            else
                p=beg+strlen(beg);

            while(p>beg && (p[-1]==' ' || p[-1]=='\t'))
                p--;
            *p=0;

            p=beg;

            if(!strncmp(p,"\xEF\xBB\xBF",3))    // skip BOM
                p+=3;

            if(!*p)
                continue;

            if(*p=='#')
            {
                p++;
                static const char tag[]="EXTINF:";
                static const char logo_tag[]="EXTLOGO:";
                static const char type_tag[]="EXTTYPE:";
                if(!strncmp(p,tag,sizeof(tag)-1))
                {
                    p+=sizeof(tag)-1;
                    while(*p && *p==' ')
                        p++;

                    char* p2=strchr(p,',');
                    if(p2)
                    {
                        *p2=0;
                        p2++;

                        int n=snprintf(track_name,sizeof(track_name),"%s",p2);
                        if(n==-1 || n>=sizeof(track_name))
                            track_name[sizeof(track_name)-1]=0;
                    }
                }else if(!strncmp(p,logo_tag,sizeof(logo_tag)-1))
                {
                    p+=sizeof(logo_tag)-1;
                    while(*p && *p==' ')
                        p++;

                    int n=snprintf(logo_url,sizeof(logo_url),"%s",p);
                    if(n==-1 || n>=sizeof(logo_url))
                        logo_url[sizeof(logo_url)-1]=0;
                }else if(!strncmp(p,type_tag,sizeof(type_tag)-1))
                {
                    p+=sizeof(type_tag)-1;
                    while(*p && *p==' ')
                        p++;

                    char* ext=strchr(p,',');
                    if(ext)
                    {
                        *ext=0;
                        ext++;
                    }else
                        ext=(char*)"";

                    int n=snprintf(file_type,sizeof(file_type),"%s",p);
                    if(n==-1 || n>=sizeof(file_type))
                        file_type[sizeof(file_type)-1]=0;

                    if(*ext)
                    {
                        n=snprintf(file_type_ext,sizeof(file_type_ext),"%s",ext);
                        if(n==-1 || n>=sizeof(file_type_ext))
                            file_type_ext[sizeof(file_type_ext)-1]=0;
                    }
                }
            }else
            {
                int n=snprintf(track_url,sizeof(track_url),"%s",p);
                if(n==-1 || n>=sizeof(track_url))
                    track_url[sizeof(track_url)-1]=0;


                const char* p=file_type;

                if(!*p)
                {
                    p=track_url+strlen(track_url)-1;
                    while(p>track_url && p[-1]!='/' && p[-1]!='.') p--;
                    if(p[-1]!='.')
                        p="";
                }

                if(*p)
                {
                    for(int i=0;mime_type_list[i].file_ext;i++)
                    {
                        if(!strcasecmp(p,mime_type_list[i].file_ext))
                        {
                            type_info=&mime_type_list[i];
                            break;
                        }
                    }
                }

                if(!type_info)
                    type_info=&mime_type_list[0];

                if(verb_fp && upnp::debug)
                    fprintf(verb_fp,"   name='%s', url='%s', mime='%s%s' (%s)\n",track_name,track_url,type_info->type,
                        *file_type_ext?file_type_ext:type_info->dlna_type_extras,type_info->upnp_class);

                playlist_item* item=playlist_add(parent_item,track_name,track_url,type_info,logo_url,file_type_ext);

                *track_name=0;
                *track_url=0;
                type_info=0;
                *logo_url=0;
                *file_type=0;
                *file_type_ext=0;
            }
        }


        fclose(fp);
    }

    return 0;
}

int pshare::upnp_print_item(FILE* fp,playlist_item* item)
{
    if(item->type_info->container)
        fprintf(fp,"&lt;container id=&quot;%i&quot; childCount=&quot;%i&quot; parentID=&quot;%i&quot; restricted=&quot;true&quot;&gt;",
            item->object_id,item->childs,item->parent_id);
    else
        fprintf(fp,"&lt;item id=&quot;%i&quot; parentID=&quot;%i&quot; restricted=&quot;true&quot;&gt;",item->object_id,item->parent_id);

    fprintf(fp,"&lt;dc:title&gt;");
    tmpl::print_to_xml2(item->name,fp);
    fprintf(fp,"&lt;/dc:title&gt;&lt;upnp:class&gt;%s&lt;/upnp:class&gt;",item->type_info->upnp_class);

    if(item->parent && *item->parent->name)
    {
        if(item->type_info->upnp_class==upnp_video)
        {
            fprintf(fp,"&lt;upnp:actor&gt;");
            tmpl::print_to_xml2(item->parent->name,fp);
            fprintf(fp,"&lt;/upnp:actor&gt;");
        }else if(item->type_info->upnp_class==upnp_audio)
        {
            fprintf(fp,"&lt;upnp:artist&gt;");                      // album
            tmpl::print_to_xml2(item->parent->name,fp);
            fprintf(fp,"&lt;/upnp:artist&gt;");
        }
    }

    if(item->track_number>0)
        fprintf(fp,"&lt;upnp:originalTrackNumber&gt;%i&lt;/upnp:originalTrackNumber&gt;",item->track_number);

    if(*item->logo_url)
    {
        if(*item->logo_dlna_profile)
            fprintf(fp,"&lt;upnp:albumArtURI dlna:profileID=&quot;%s&quot;&gt;",item->logo_dlna_profile);
        else
            fprintf(fp,"&lt;upnp:albumArtURI&gt;");

        tmpl::print_to_xml2(item->logo_url,fp);
        fprintf(fp,"&lt;/upnp:albumArtURI&gt;");
    }

    if(verb_fp && upnp::debug)
        fprintf(verb_fp," * '%s' ('%s')\n",item->name,*item->type_extras?item->type_extras:item->type_info->dlna_type_extras);


    if(!item->type_info->container)
    {
//        fprintf(fp,"&lt;res protocolInfo=&quot;%s%s&quot; size=&quot;0&quot; duration=&quot;0:00:00&quot;&gt;",item->type_info->type,*item->type_extras?item->type_extras:item->type_info->dlna_type_extras);
//        fprintf(fp,"&lt;res duration=\"24:00:00.000\" protocolInfo=&quot;%s%s&quot;&gt;",item->type_info->type,*item->type_extras?item->type_extras:item->type_info->dlna_type_extras);
        fprintf(fp,"&lt;res size=&quot;0&quot; protocolInfo=&quot;%s%s&quot;&gt;",item->type_info->type,*item->type_extras?item->type_extras:item->type_info->dlna_type_extras);

        if(!item->proxy)
            tmpl::print_to_xml2(item->url,fp);
        else
            fprintf(fp,"http://%s:%i/pr/%i/stream.%s",pshare::mcast_grp.interface,pshare::http_port,item->object_id,item->type_info->file_ext);

        fprintf(fp,"&lt;/res&gt;&lt;/item&gt;");
    }else
        fprintf(fp,"&lt;/container&gt;");


    return 0;
}

int pshare::upnp_browse(FILE* fp,int object_id,const char* flag,const char* filter,int index,int count)
{
    playlist_item def_item= { -1, object_id, 0, 0, (char*)"???", (char*)"", (char*)"", (char*)"", (char*)"", &mime_type_folder, 0, 0, 0 };

    int num=0;
    int total_num=0;

    fprintf(fp,
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
        "   <s:Body>\n"
        "      <u:BrowseResponse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">\n"
        "         <Result>&lt;DIDL-Lite xmlns=&quot;urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/&quot; xmlns:dc=&quot;http://purl.org/dc/elements/1.1/&quot; xmlns:upnp=&quot;urn:schemas-upnp-org:metadata-1-0/upnp/&quot;&gt;");


    if(!strcmp(flag,"BrowseMetadata"))
    {
        playlist_item* item=playlist_find_by_id(object_id);

        upnp_print_item(fp,item?item:&def_item);

//upnp_print_item(stdout,item?item:&def_item);

        num=total_num=1;
    }else
    {
        {
            playlist_item* ii=playlist_find_by_id(object_id);
            if(ii)
                total_num=ii->childs;
        }

        int n=0;
        for(playlist_item* item=playlist_beg;item;item=item->next)
        {
            if(item->parent_id==object_id)
            {
                if(n>=index)
                {
                    upnp_print_item(fp,item);
//upnp_print_item(stdout,item);
                    num++;

                    if(count>0 && num>=count)
                        break;
                }
                n++;
            }
        }
    }

    if(verb_fp)
        fprintf(verb_fp,"returned %i, total %i\n",num,total_num);

    fprintf(fp,"&lt;/DIDL-Lite&gt;</Result>\n"
        "         <NumberReturned>%i</NumberReturned>\n"
        "         <TotalMatches>%i</TotalMatches>\n"
        "         <UpdateID>%i</UpdateID>\n"
        "      </u:BrowseResponse>\n"
        "   </s:Body>\n"
        "</s:Envelope>\n",num,total_num,update_id);

    return 0;
}

const char* pshare::search_object_type(const char* exp)
{
    static const char class_tag[]     = "upnp:class";
    static const char derived_tag[]   = "derivedfrom";

    static const char video_tag[]     = "object.item.videoItem";
    static const char audio_tag[]     = "object.item.audioItem";
    static const char image_tag[]     = "object.item.imageItem";

    while(*exp && *exp==' ')
        exp++;

    if(!*exp || !strcmp(exp,"*"))
        return "*";

    const char* p=strstr(exp,class_tag);

    if(p)
    {
        p+=sizeof(class_tag)-1;

        while(*p && (*p==' ' || *p=='\t'))
            p++;

        int ok=1;
        
        if(!strncmp(p,derived_tag,sizeof(derived_tag)-1))
            p+=sizeof(derived_tag)-1;
        else if(*p=='=')
            p++;
        else
            ok=0;

        if(ok)
        {
            while(*p && (*p==' ' || *p=='\t'))
                p++;
            if(*p=='\"')
            {
                p++;

                const char* p2=strchr(p,'\"');

                if(p2)
                {
                    char tmp[64];

                    int n=p2-p;

                    if(n>=sizeof(tmp))
                        n=sizeof(tmp)-1;

                    strncpy(tmp,p,n);
                    tmp[n]=0;

                    if(!strncmp(tmp,video_tag,sizeof(video_tag)-1))
                        return upnp_video;
                    else if(!strncmp(tmp,audio_tag,sizeof(audio_tag)-1))
                        return upnp_audio;
                }
            }
        }
    }

    return "";
}


int pshare::upnp_search(FILE* fp,int object_id,const char* what,const char* filter,int index,int count)
{
    int num=0;
    int total_num=0;

    what=search_object_type(what);

    fprintf(fp,
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
        "   <s:Body>\n"
        "      <u:SearchResponse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">\n"
        "         <Result>&lt;DIDL-Lite xmlns=&quot;urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/&quot; xmlns:dc=&quot;http://purl.org/dc/elements/1.1/&quot; xmlns:upnp=&quot;urn:schemas-upnp-org:metadata-1-0/upnp/&quot;&gt;");


    playlist_item* beg=playlist_find_by_id(object_id);

    if(beg && beg->type_info->container)
    {
        int n=0;

        for(playlist_item* item=beg->next;item;item=item->next)
        {
            if(!item->type_info->container)
            {
                if(*what=='*' || item->type_info->upnp_class==what)
                {
                    if(n>=index)
                    {
                        if(!count || num<count)
                        {
                            upnp_print_item(fp,item);
                            num++;
                        }
                        total_num++;
                    }
                    n++;
                }
            }else
            {  
                playlist_item* p=0;
                for(p=item->parent;p;p=p->parent)
                {
                    if(p->object_id==object_id)
                        break;
                }
                if(!p)
                    break;
            }
        }
    }

    if(verb_fp)
        fprintf(verb_fp,"returned %i, total %i\n",num,total_num);

    fprintf(fp,"&lt;/DIDL-Lite&gt;</Result>\n"
        "         <NumberReturned>%i</NumberReturned>\n"
        "         <TotalMatches>%i</TotalMatches>\n"
        "         <UpdateID>%i</UpdateID>\n"
        "      </u:SearchResponse>\n"
        "   </s:Body>\n"
        "</s:Envelope>\n",num,total_num,update_id);

    return 0;
}

int pshare::playlist_browse(const char* req,FILE* fp)
{
    while(*req && *req=='/') req++;

    int object_id=atoi(req);

    print_http_hdrs(fp,http_mime_html,0);

    fprintf(fp,"<html><head><title>%s - Playlist</title></head>\n\n<body>\n",server_name);

    playlist_item* item=playlist_find_by_id(object_id);
    if(item && item->parent_id>=0)
        fprintf(fp,"<a href='/p/%i'>up</a><br>\n",item->parent_id);


    for(item=playlist_beg;item;item=item->next)
    {
        if(item->parent_id==object_id)
        {

            if(item->type_info->container)
                fprintf(fp,"<a href='/p/%i'>",item->object_id);
            else
            {
                if(!item->proxy)
                    fprintf(fp,"<a href='%s'>",item->url);
                else
                    fprintf(fp,"<a href='http://%s:%i/pr/%i/stream.%s'>",pshare::mcast_grp.interface,pshare::http_port,item->object_id,item->type_info->file_ext);
            }

            tmpl::print_to_xml(item->name,fp);
            fprintf(fp,"</a><br>\n");
        }
    }


    fprintf(fp,"</body>\n");

    return 0;
}
