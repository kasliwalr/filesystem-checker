#ifndef _FILE_IO_H_
#define _FILE_IO_H_

// fd: file descriptor, must be RD_ONLY
// return: if successful return pointer to start of file, if error, prints error and return (void*) -1
void* get_memory_map(int fd, int *);

#endif
