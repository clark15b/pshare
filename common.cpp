#include "pshare.h"
#include <stdio.h>
#include <time.h>

namespace pshare
{
    int get_gmt_date(char* dst,int ndst)
    {
        time_t timestamp=time(0);
    
        tm* t=gmtime(&timestamp);
        
        static const char* wd[7]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        static const char* m[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
                
        return snprintf(dst,ndst,"%s, %i %s %.4i %.2i:%.2i:%.2i GMT",wd[t->tm_wday],t->tm_mday,m[t->tm_mon],t->tm_year+1900,
            t->tm_hour,t->tm_min,t->tm_sec);
    }

//    const char xbox360_extra_hdrs[]="X-User-Agent: redsonic\r\n";

    int print_http_hdrs(FILE* fp,const char* content_type,int extras)
    {
        char date[64];
        get_gmt_date(date,sizeof(date));

        fprintf(fp,
            "HTTP/1.1 200 OK\r\nPragma: no-cache\r\nCache-control: no-cache\r\nDate: %s\r\nServer: %s\r\nAccept-Ranges: none\r\n"
                    "Connection: close\r\nContent-Type: %s\r\nEXT:\r\n",
                        date,server_name,content_type);

//        if(xbox360)
//            fprintf(fp,"%s",xbox360_extra_hdrs);

        if(!extras)
            fprintf(fp,"\r\n");

        return 0;
    }

    int print_http_hdrs_no_content(FILE* fp,int extras)
    {
        char date[64];
        get_gmt_date(date,sizeof(date));

        fprintf(fp,
            "HTTP/1.1 200 OK\r\nPragma: no-cache\r\nCache-control: no-cache\r\nDate: %s\r\nServer: %s\r\nAccept-Ranges: none\r\n"
                    "Connection: close\r\nContent-Length: 0\r\nEXT:\r\n",date,server_name);

//        if(xbox360)
//            fprintf(fp,"%s",xbox360_extra_hdrs);

        if(!extras)
            fprintf(fp,"\r\n");

        return 0;
    }

    int print_http_error_hdrs(FILE* fp,const char* status,int extras)
    {
        char date[64];
        get_gmt_date(date,sizeof(date));

        fprintf(fp,"HTTP/1.1 %s\r\nPragma: no-cache\r\nCache-control: no-cache\r\nDate: %s\r\nServer: %s\r\nConnection: close\r\nEXT:\r\n",
            status,date,server_name);

//        if(xbox360)
//            fprintf(fp,"%s",xbox360_extra_hdrs);

        if(!extras)
            fprintf(fp,"\r\n");

        return 0;
    }
}
