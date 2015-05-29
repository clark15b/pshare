#include <stdio.h>
#include <string.h>

int main(void)
{
    char t[256];
    memset(t,0,sizeof(t));


    for(int i=65;i<91;i++)
        t[i]=1;
    for(int i=97;i<123;i++)
        t[i]=1;
    for(int i=48;i<58;i++)
        t[i]=2;
    t[9]=4;
    t[10]=4;
    t[13]=4;
    t[32]=4;

    for(int i=33;i<48;i++)
        t[i]=8;
    for(int i=58;i<65;i++)
        t[i]=8;
    for(int i=91;i<97;i++)
        t[i]=8;
    for(int i=123;i<127;i++)
        t[i]=8;

    for(int i=0;i<sizeof(t)/sizeof(*t);i++)
    {
        printf("0x%.2x,",t[i]);
        if(!((i+1)%16))
            printf("\n");
    }

    
    return 0;
}
