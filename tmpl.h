#ifndef __TMPL_H
#define __TMPL_H

#include <stdio.h>

namespace tmpl
{
    int print_to_xml(const char* s,FILE* fp);
    int print_to_xml2(const char* s,FILE* fp);
    int get_file(const char* filename,FILE* dst,int tmpl);
}


#endif
