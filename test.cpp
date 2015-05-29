#include "upnp.h"
#include <getopt.h>
#include <stdio.h>
#include <string.h>


int main(int argc,char** argv)
{
    upnp::verb_fp=stderr;

    upnp::mcast_grp grp("239.255.255.250:1900","eth0",1,1);
//    upnp::mcast_grp grp("239.255.255.250:1900","",1,1);

    const char* cmd="";

    if(argc>1)
        cmd=argv[1];

    if(!strcmp(cmd,"send"))
    {
        int sock_up=grp.upstream();

        if(sock_up!=-1)
        {
            const char ss[]=
"M-SEARCH * HTTP/1.1\r\n"
"HOST: 239.255.255.250:1900\r\n"
"ST: urn:schemas-upnp-org:device:MediaServer:1\r\n"
//"ST: uuid:742ee682-2e3b-4198-a62d-d1c15dd1232d\r\n"
//"ST: urn:schemas-upnp-org:service:ContentDirectory:1\r\n"
"MAN: \"ssdp:discover\"\r\n"
"MX: 2\r\n"
"X-AV-Client-Info: av=5.0; cn=\"Sony Computer Entertainment Inc.\"; mn=\"PLAYSTATION 3\"; mv=\"1.0\";\r\n\r\n";

            grp.send(sock_up,ss,sizeof(ss)-1);

            char tmp[1024];
            int n=grp.recv(sock_up,tmp,sizeof(tmp)-1,0);
            if(n>0)
            {
                tmp[n]=0;
                printf("%s\n",tmp);
            }


            grp.close(sock_up);
        }
    }else if(!strcmp(cmd,"recv"))
    {
        int sock_down=grp.join();

        if(sock_down!=-1)
        {
            while(1)
            {
                char tmp[1024];
                int n=grp.recv(sock_down,tmp,sizeof(tmp)-1,0);

                if(!n || n==-1)
                    break;

                tmp[n]=0;
                printf("%s\n",tmp);
            }


            grp.leave(sock_down);
            grp.close(sock_down);
        }
    }

    return 0;
}

