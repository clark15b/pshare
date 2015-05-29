#include "pshare.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// TODO: PS3 Radio (MP3) - 'TransferMode.DLNA.ORG: Streaming' HTTP header from remote server
// TODO: select() to HTTP proxy (O_NONBLOCK,fread,EAGAIN,select())
// TODO: my udpxy with extend HTTP headers ('GET /udp/234.5.2.1:20000?staff=staff HTTP/1.0')

/*
Bravia has four arrow buttons to change position: left/right – to fast forward/reverse and up/down – to moving cursor and set time position (something like goto).
If DLNA.ORG_OP=11, then left/rght keys uses range header, and up/down uses TimeSeekRange.DLNA.ORG header
If DLNA.ORG_OP=10, then left/rght and up/down keys uses TimeSeekRange.DLNA.ORG header
If DLNA.ORG_OP=01, then left/rght keys uses range header, and up/down keys are disabled
and if DLNA.ORG_OP=00, then all keys are disabled
So, Range header is used to fast forward/reverse, but sometimes it is very slow. TimeSeekRange.DLNA.ORG header doesn’t work with directly steramed video files (PMS returns movie from beginning):
*/

/* contentFeatures.dlna.org: DLNA.ORG_PN=MPEG_TS_SD_NA;DLNA.ORG_OP=00;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=017000 00000000000000000000000000 */

/*
GET /get/0$0$1$0/00000.MTS HTTP/1.1
TimeSeekRange.dlna.org: npt=1790.044-
getcontentFeatures.dlna.org: 1
Pragma: getIfoFileURI.dlna.org
transferMode.dlna.org: Streaming
Host: 192.168.1.101:5001

HTTP/1.1 200 OK
Accept-Ranges: bytes
Connection: keep-alive
Content-Length: 1252804608
Content-Type: video/mpeg
ContentFeatures.DLNA.ORG: DLNA.ORG_PN=AVC_TS_HD_50_AC3;DLNA.ORG_OP=11;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01700000000000000000000000000000
Server: Windows_XP-x86-5.1, UPnP/1.0, PMS/1.20
TransferMode.DLNA.ORG: Streaming


Audio:

HTTP/1.1 200 OK
Content-Type: audio/mpeg
Content-Length: 5471488
Date: Sun, 14 Nov 2010 12:35:13 GMT
Pragma: no-cache
Cache-control: no-cache
Last-Modified: Sun, 11 Oct 2009 13:31:09 GMT
Accept-Ranges: bytes
Connection: close
transferMode.dlna.org: Streaming
EXT:
Server: Linux/2.x.x, UPnP/1.0, pvConnect UPnP SDK/1.0

------
Content-Type
transferMode.dlna.org
EXT
Accept-Ranges: none
------


Video:

HTTP/1.1 200 OK
Content-Type: video/x-msvideo
Content-Length: 63924224
Date: Sun, 14 Nov 2010 12:38:29 GMT
Pragma: no-cache
Cache-control: no-cache
Last-Modified: Fri, 14 Nov 2008 20:22:28 GMT
Accept-Ranges: bytes
Connection: close
EXT:
Server: Linux/2.x.x, UPnP/1.0, pvConnect UPnP SDK/1.0
*/


int pshare::do_proxy(playlist_item* item,FILE* fp)
{
    print_http_hdrs(fp,item->type_info->http_mime_type,1);

    if(item->type_info->upnp_class==upnp_audio)
        fprintf(fp,"transferMode.dlna.org: Streaming\r\n");

    fprintf(fp,"\r\n");

    fflush(fp);

    int fd=fileno(fp);

    static const char udp_tag[] ="udp://@";
    static const char rtp_tag[] ="rtp://@";
    static const char http_tag[]="http://";

    if(!strncmp(item->url,http_tag,sizeof(http_tag)-1))
    {
        do_http_proxy(item->url+sizeof(http_tag)-1,fd);
        return 0;
    }

    if(verb_fp)
        fprintf(verb_fp,"unknown protocol type: %s\n",item->url);

    return -1;
}

int pshare::do_http_proxy(const char* url,int fd)
{
    sockaddr_in sin;
    memset((char*)&sin,0,sizeof(sin));
    sin.sin_family=AF_INET;

    char temp[4096];
    int n=snprintf(temp,sizeof(temp),"%s",url);
    if(n==-1 || n>=sizeof(temp))
        temp[sizeof(temp)-1]=0;

    char* host=temp;

    char* res=strchr(host,'/');
    if(res)
        { *res=0; res++; }
    else
        res=(char*)"";

    char* port=strchr(host,':');
    if(port)
        { *port=0; port++; }
    else
        port=(char*)"80";

    sin.sin_port=htons(atoi(port));
    sin.sin_addr.s_addr=inet_addr(host);

    if(sin.sin_addr.s_addr==INADDR_NONE)
    {
        hostent* he=gethostbyname(host);
        if(!he)
        {
            if(verb_fp)
                fprintf(verb_fp,"host is not found: %s\n",host);
            return -2;
        }
        memcpy((char*)&sin.sin_addr.s_addr,he->h_addr,sizeof(sin.sin_addr.s_addr));
    }

    int sock=socket(PF_INET,SOCK_STREAM,0);

    if(sock==-1)
    {
        if(verb_fp)
            fprintf(verb_fp,"socket() fail\n");
        return -3;
    }

    if(connect(sock,(sockaddr*)&sin,sizeof(sin)))
    {
        if(verb_fp)
            fprintf(verb_fp,"can`t connect to %s:%s\n",host,port);
        close(sock);
        return -4;
    }

    int on=1;
    setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,&on,sizeof(on));

    FILE* sfd=fdopen(sock,"a+");

    if(!sfd)
    {
        if(verb_fp)
            fprintf(verb_fp,"fdopen() fail\n");
        close(sock);
    }

    if(verb_fp)
        fprintf(verb_fp,"connected to %s:%s\n",host,port);

    fprintf(sfd,"GET /%s HTTP/1.1\r\nServer: %s:%s\r\nConnection: close\r\n\r\n",res,host,port);
    fflush(sfd);

    int lineno=0;

    int status_code=-1;

    while(fgets(temp,sizeof(temp),sfd))
    {
        char* p=strpbrk(temp,"\r\n");
        if(p)
            *p=0;

        if(!*temp)
            break;

        if(!lineno)
        {
            p=strchr(temp,' ');
            if(p)
            {
                while(*p && *p==' ')
                    p++;
                char* p2=strchr(p,' ');
                if(p2)
                {
                    *p2=0;
                    status_code=atoi(p);
                }
            }
        }

        lineno++;
    }

    if(status_code!=200)
    {
        if(verb_fp)
            fprintf(verb_fp,"status code: %i\n",status_code);

        fclose(sfd);

        return -5;
    }

    if(verb_fp)
        fprintf(verb_fp,"streaming http://%s\n",url);

    int brk=0;

    size_t len;

    while(!brk && (len=fread(temp,1,sizeof(temp),sfd))>0)
    {
        size_t l=0;

        while(!brk && l<len)
        {
            ssize_t nn=send(fd,temp+l,len-l,0);

            if(nn==-1)
            {
                if(verb_fp)
                    fprintf(verb_fp,"client gone, break streaming\n");
                brk=1;
            }else
                l+=nn;
        }
    }

    fclose(sfd);

    if(verb_fp)
        fprintf(verb_fp,"disconnected\n");

    return 0;
}
