// In case you want to implement the shared memory IPC as a library
// You may use this file. It is optional. It does help with code reuse
//

#include <mqueue.h>
#include <semaphore.h>
#include <fcntl.h>    // For O_CREAT, O_RDWR
#include <sys/stat.h> // For mode_t permissions
#include <sys/mman.h> // For shm_open
#include <unistd.h> // For ftruncate
#include <sys/mman.h> // For mmap
                      //
void* get_shm_map_for_thread(int thread_id);
