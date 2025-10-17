// In case you want to implement the shared memory IPC as a library
// This is optional but may help with code reuse
//
#include <stdio.h>
#include "shm_channel.h"
#include "cache-student.h"

void* get_shm_map_for_thread(int thread_id) {
    int shm_fd;
    char shm_name[50];
    sprintf(shm_name, "%s_%d", SHM_SEGMENT_PREFIX, thread_id);
    if ((shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666)) < 0) {
      perror("get_shm_map_for_thread: shm_open");
    }
    if (ftruncate(shm_fd, SHM_SEGMENT_SIZE) == -1) {
      perror("get_shm_map_for_thread: ftruncate");
    }
    void* ptr;
    ptr = mmap(0, SHM_SEGMENT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
      perror("get_shm_map_for_thread: mmap");
    }
    return ptr;
}
