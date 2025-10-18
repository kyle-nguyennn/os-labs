# Project 3 README file

## Project Description

This project is an exercise in utilizing different IPC mechanisms introduced in the lecture, including message-passing and shared-memory-based approaches. It involves implementing a multi-threaded web proxy server that communicates with a separate cache server process to serve file requests from clients. The proxy and cache processes can communicate through any of the Linux IPC mechanisms. This deepens the understanding of different IPC mechanisms and how they can be used together to devise an efficient multi-process communication protocol.

## Project Design

The proxy server initializes `gfserver` with `n_threads`. Each thread calls `handle_with_cache` for each request from `gfclient`. Each thread communicates with the cache process through POSIX IPC mechanisms, including message queues, semaphores, and shared memory. The proxy server simply relays the response from the cache server back to the `gfclient`. The main design decisions are made in the proxy–cache communication protocol.

The communication protocol between the proxy server and the cache server has two phases: the first phase is the cache command and reply, and the second phase is transferring file content from the cache server to the proxy server.

### Proxy–Cache protocol

**Phase 1**

The cache server’s main thread opens a POSIX message queue, `/cache_command_queue`, to receive commands from proxy threads. It also starts a number of threads to handle proxy requests, following the **boss–worker pattern**.

Each proxy thread enqueues the path it requests onto `/cache_command_queue`, together with its *thread_id* in a `cache_command` struct, so that cache threads know the identity of the sender to reply to. On the cache server, all worker threads keep polling `/cache_command_queue` for new cache requests to serve. Because the POSIX message queue is thread-safe and `mq_receive()` is blocking, we do not need additional locking here.

Once dequeuing a `cache_command` from the proxy, a cache worker thread checks whether the file exists in the local `simplecache` backend and replies with a `cache_reply` by sending it onto the proxy thread’s dedicated message queue called `/cache_reply_queue_[thread_id]`. `cache_reply` includes `status` (`CACHE_OK`, `CACHE_FILE_NOT_FOUND`), which mirrors a subset of `gfstatus_t`, and `file_len` (`-1` if the file is not found).

If the file is not found, the lifecycle of the request ends here. There is no need for Phase 2. The cache thread moves on to the next request.

If the file is found, the proxy–cache communication moves on to Phase 2 for file content transfer on a separate data channel.

**Phase 2**

A cache worker thread opens the shared memory (shm) segment dedicated to the requesting proxy *thread_id*, `/cache_shm_segment_[thread_id]`, and starts reading the file descriptor’s content onto the shared memory pointer in chunks of `SHM_SEGMENT_SIZE` which is **8MB.**

A proxy thread can read only when the whole shm segment has been populated by the cache worker. The cache worker can start loading the next file data chunk onto the shm segment only when the current one has been fully consumed by the proxy thread. This is ensured by a **producer–consumer** pattern using POSIX semaphores. Specifically, `/cache_sem_c` is for the producer (cache worker) to signal to the consumer (proxy thread) that the shm segment is ready for consumption, and `/cache_sem_p` is for the consumer to signal to the producer that it has processed everything in the current buffer and is ready for the next data chunk. Each shm segment has its own pair of semaphores, suffixed with the *thread_id* it is attached to.

### Data structures and lifecycle management

To facilitate the above protocol, we need to maintain the following data structures for communication.

***Command channel***

Includes two message queues: `/cache_command_queue` and `/cache_reply_queue`. There is only one instance of `/cache_command_queue` in the whole system, while there are as many `/cache_reply_queue`s as the number of proxy threads. The payload on `/cache_command_queue` is of type `cache_command_t`, and the payload on `/cache_reply_queue` is of type `cache_reply_t`, as shown in the snippet below. Although the **POSIX mq API** only accepts payloads of type `char*` in `mq_send` and `mq_receive`, our payloads are **plain data objects** (fixed-size elements, no pointer members, etc.) and can be sent as bytes, then reinterpreted as their correct structs when received. `PATH_MAX` is a Linux system variable that limits the length of all paths that exist in a Linux file system.

```c
typedef struct cache_command {
    int thread_id;
    char path[PATH_MAX];
} cache_command_t;

typedef struct cache_reply {
    cache_status_t status;
    ssize_t file_len;
} cache_reply_t;
```

The `/cache_command_queue` lifecycle is managed by the cache server’s main thread. It is created when the cache process starts up and cleaned up when the cache process exits (on **SIGTERM** or **SIGINT**). The proxy server manages the lifecycle of `/cache_reply_queue` because only it knows the specifics about each thread at startup. When the proxy server starts and initializes the worker threads, it also creates the cache reply queues and attaches one to each thread, suffixing the queue name with the *thread_id*, i.e., `/cache_reply_queue_[thread_id]`. This set of queues is cleaned up when the proxy server exits (on **SIGTERM** or **SIGINT**).

***Data channel***

Includes the set of shared memory segments and two sets of semaphores, one producer set and one consumer set, for data flow control. The size of each set above is the same as the number of proxy threads. Hence each proxy thread is associated with one dedicated shared memory segment for data transfer and a pair of consumer/producer semaphores. The names of these structures are defined in `cache-student.h` as below. The runtime names of the relevant data structures are constructed by concatenating the prefix with the corresponding *thread_id*. This acts as the contract between the producer (cache server) and the consumer (proxy server) for how to identify the correct memory segment to communicate on.

```c
#define SEM_P_PREFIX "/cache_sem_p"
#define SEM_C_PREFIX "/cache_sem_c"
#define SHM_SEGMENT_PREFIX "/cache_shm_segment"
#define SHM_SEGMENT_SIZE 8192
```

Similar to `/cache_reply_queue_[thread_id]`, each element in the data channel is specific to a proxy thread, so its lifecycle should be managed by the proxy server. The shared memory segment and its guarding semaphores are created when the proxy server starts up and are cleaned up properly when the proxy server exits on **SIGTERM** or **SIGINT**.

### Trade-offs

The shared memory segment size per thread is designed to be a fixed-size, one-slot buffer of 8192 bytes to balance the trade-off between the overhead of kernel-space crossings and the memory footprint of the proxy server. The proxy needs to support up to 200 concurrent threads, which means it could create up to 200 shared memory segments at maximum capacity, consuming up to around 1.6 GB of memory. This ensures the system functions within resource limits.

This decision also simplifies implementation and testing, as a one-slot buffer can only be in two states, empty or full, making synchronization more straightforward and less error-prone. Since there is exactly one producer and one consumer per buffer, multiple slots are unnecessary.

## Testing

- The system is tested with the help of the `gfclient_download` and `gfclient_measure` binaries.
- `workload.txt` includes a mix of files that exist in the local cache and ones that do not, to test response correctness.
- Tests with varying numbers of `nthreads` and `nrequests` on `gfclient`, `webproxy`, and `simplecached` processes to test performance and load.
- Verify that the system runs correctly regardless of the order of starting up different processes. The client can start sending requests as long as both `webproxy` and `simplecached` are up.
- Verify that the `webproxy` still serves correctly even when there is only one `simplecached` thread serving multiple concurrent requests, though performance degrades.
- Verify that performance is better when running `simplecached` with multiple threads.

## References

- [https://www.hackthissite.org/articles/read/1078](https://www.hackthissite.org/articles/read/1078)
- Linux man pages for POSIX message queues, semaphores, and shared memory

## Suggestion

Another criterion that can be used to evaluate the protocol is how easy it is to scale the proxy server horizontally. That is, “Is it seamless to spin up another independent proxy server and start load balancing client requests?” In other words, this tests whether the cache server can function correctly when there are requests coming from different clients/processes.

**Why is this test suitable for an IPC exercise?**

Since the proxy server is stateless, there is no need for state synchronization between different instances of the proxy server. The focus is on the correctness of the cache server and the IPC protocol between the proxy server and the cache server. The cache server should not distinguish between requests that come from threads of the same process versus different processes. They should be treated the same and behave correctly in either case.

A simple test is to start two proxy servers independently, alternate `gfclient` requests between these two servers, and verify that it works as if there were only one proxy server.

With the proposed “lazy” protocol above, this would not work, because it depends on system-wide names for the IPC data structures and only differentiates by *thread_id*, which is local to a proxy server process. This bakes in the assumption that only one `webproxy` instance is up at a time.

A simple approach to fix the above protocol is to make the structure names also depend on the process ID, or add more information in `cache_reply` to instruct the proxy thread where to read the content. This shifts ownership of the shared memory lifecycle and synchronization entirely to the cache server. This design also gives the flexibility to resize the mapped memory depending on the requested file size and system load at runtime.

Hence, adding this test will drive student implementations toward a more proper and robust protocol design.