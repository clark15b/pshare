#include "tmpl.h"
#include "pshare.h"
#include <stdlib.h>
#include <string.h>

namespace tmpl
{
    int validate_file_name(const char* src)
    {
        if(strstr(src,"../") || strchr(src,'\\'))
            return -1;
        return 0;
    }

    const char* get_mime_type(const char* src,int* xml)
    {
        if(xml)
            *xml=0;

        const char* p=strrchr(src,'.');

        if(p)
        {
            p++;

            if(!strcasecmp(p,"xml")) { if(xml) *xml=1; return "text/xml"; }
            else if(!strcasecmp(p,"html") || !strcasecmp(p,"htm")) { if(xml) *xml=1; return "text/html"; }
            else if(!strcasecmp(p,"txt")) return "text/plain";
            else if(!strcasecmp(p,"jpeg") || !strcasecmp(p,"jpg")) return "image/jpeg";
            else if(!strcasecmp(p,"png")) return "image/png";
        }

        return "application/x-octet-stream";
    }

    int get_file_env(FILE* src,FILE* dst,int xml)
    {
        int st=0;

        char var[64]="";
        int nvar=0;

        for(;;)
        {
            int ch=fgetc(src);
            if(ch==EOF)
                break;

            switch(st)
            {
            case 0:
                if(ch=='#')
                    st=1;
                else
                    fputc(ch,dst);
                break;
            case 1:
                if(ch=='#')
                {
                    var[nvar]=0;
                    const char* p=getenv(var);
                    if(!p)
                        p="(null)";

                    if(!xml)
                        fprintf(dst,"%s",p);
                    else
                        print_to_xml(p,dst);
                    nvar=0;
                    st=0;
                }else
                {
                    if(nvar<sizeof(var)-1)
                        var[nvar++]=ch;
                }
                break;
            }
        }

        return 0;
    }

    int get_file_plain(FILE* src,FILE* dst)
    {
        char tmp[512];

        int n=0;

        while((n=fread(tmp,1,sizeof(tmp),src))>0)
        {
            if(!fwrite(tmp,n,1,dst))
                break;
        }


        return 0;
    }


    int get_file(const char* filename,FILE* dst,int tmpl)
    {
        while(*filename && *filename=='/')
            filename++;

        FILE* fp=0;

        if(validate_file_name(filename) || !(fp=fopen(filename,"rb")))
        {
            pshare::print_http_error_hdrs(dst,"404 Not found",0);
            return -1;
        }

        int xml=0;
        pshare::print_http_hdrs(dst,get_mime_type(filename,&xml),0);

        int rc;

        if(tmpl)
            rc=get_file_env(fp,dst,xml);
        else
            rc=get_file_plain(fp,dst);

        fclose(fp);
        
        return rc;
    }

    int print_to_xml(const char* s,FILE* fp)
    {
        for(const char* p=s;*p;p++)
        {
            switch(*p)
            {               
            case '&': fprintf(fp,"&amp;"); break;
            case '<': fprintf(fp,"&lt;"); break;
            case '>': fprintf(fp,"&gt;"); break;
            case '\'': fprintf(fp,"&apos;"); break;
            case '\"': fprintf(fp,"&quot;"); break;
            default: fputc(*p,fp); break;
            }
        }
        return 0;
    }

    int print_to_xml2(const char* s,FILE* fp)
    {
        for(const char* p=s;*p;p++)
        {
            switch(*p)
            {               
            case '&': fprintf(fp,"&amp;amp;"); break;
            case '<': fprintf(fp,"&amp;lt;"); break;
            case '>': fprintf(fp,"&amp;gt;"); break;
            case '\'': fprintf(fp,"&amp;apos;"); break;
            case '\"': fprintf(fp,"&amp;quot;"); break;
            default: fputc(*p,fp); break;
            }
        }
        return 0;
    }
}
