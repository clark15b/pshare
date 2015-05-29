#include "mem.h"

#ifdef DEBUG

#include <stdlib.h>

namespace mem
{
    size_t mem_total=0;
}

void* mem::malloc(size_t size)
{
    char* p=(char*)::malloc(size+sizeof(size_t));

    *((u_int32_t*)p)=size;

    mem_total+=size;

    return p+sizeof(size_t);
}
void mem::free(void* ptr)
{
    char* p=(char*)ptr;

    p-=sizeof(size_t);

    mem_total-=*((u_int32_t*)p);

    ::free(p);
}


#endif /* DEBUG */
