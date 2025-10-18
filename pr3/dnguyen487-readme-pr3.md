# Project 3 README file

## Project Description
- Your discussion of your understanding of the project.

## Project Design
- Proxy server init gfserver with n_threads, each will call handle_with_cache, for each request from gfclient

Each thread will communicate with the cache process through POSIX message queue for command and receive data through shared-mem segment dedicated for that thread.

Cache process main thread open a receive queue to receive command from proxy threads. It also starts [a number of] threads to handle proxy requests.

Each proxy thread send the path it requests, together with the name of the ~~queue it expects the cache process to reply to~~ thread_id, in order to construct the shared mem name and also deduce semaphore name, on the cache request queue. Since POSIX message queue is thread-safe we don’t need to care about locking here.

Cache worker thread block on mq_receive() on the command queue. On receiving a command from the proxy, cache worker thread will check the file from the simplecache backend, and send header back on proxy thread dedicated reply queue. Header include status (OK, FILE_NOT_FOUND), and file length, ~~shm name, and semaphore name~~ if data exist.

If file not found, just send back FILE_NOT_FOUND on the ~~reply queue~~ shared mem segment and signal semaphore to be done with the request.

If file is found, buffer the file content from fd and put it on the shm segment.

For each chunk sent, cache thread will signal proxy thread through a cross-process semaphore to start reading. Proxy then signal back when it’s done reading so cache can produce the next chunk onto the shared memory segment.

Shared memory segment size per thread is designed to be a fixed size buffer of 8192 bytes for easier initial impl and testing. I can be improved by allocating depending on how much memory is left available and the actual size of the file being transferred.

- Your explanation of the trade-offs that you considered, the choices you made, and _why_ you made those choices.
- A description of the flow of control within your submission. Pictures are helpful here.
- How you implemented your code. This should be a high level description, not a rehash of your code.

## Testing
- How you _tested_ your code. Remember, gradescope isn't a test suite. Tell us about the tests you developed. Explain any tests you _used_ but did not develop.

## References
- https://www.hackthissite.org/articles/read/1078
- man page of posix message queue, semaphore, shared memory

## Suggestion

In addition, you have an opportunity to earn extra credit. To do so, we want to see something that
adds value to the project. Observing that there is a problem or issue _is not enough_. We want
something that is easily actioned. Examples of this include:

- Suggestions for additional tests, along with an explanation of _how_ you envision it being tested
- Suggested revisions to the instructions, code, comments, etc. It's not enough to say "I found this confusing" - we want you to tell us what you would have said _instead_.

While we do award extra credit, we do so sparingly.
