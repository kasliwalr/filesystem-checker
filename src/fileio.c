#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "fileio.h"


void* get_memory_map(int fd, int* p_sz){

    FILE *fp = fdopen(fd, "r");

    if (fseek(fp, 0, SEEK_END) == -1)
    {
        printf("error occured in fseek\n");
        return (void*) -1;
    }

    int file_sz = ftell(fp);
    *p_sz = file_sz;
    void *filemap = mmap(NULL, file_sz, PROT_READ, MAP_PRIVATE, fd, 0);

    if (filemap == MAP_FAILED)
        printf("map failed: %s\n", strerror(errno));

    return filemap;
}