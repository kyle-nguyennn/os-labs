
# Project README file

## Project Description

Our implementation splits responsibility between a stateful gRPC server and thin clients. The server exposes synchronous RPCs for Store/Fetch/List/Delete plus asynchronous CallbackList streams. Every file change flows through a write-lock, updates an in-memory metadata cache, bumps an event counter, and wakes any waiting callbacks so clients see a consistent timeline of operations. Clients keep their own cache that mirrors the server snapshot and reconcile differences automatically by issuing Fetch/Store/Delete calls, which makes mounting mostly hands-off once the daemon is running.

In practice this design let us reason about correctness entirely via the cache diff: if a filename differed in CRC or deletion status, the client reacted according to the policy outlined below. Debugging focused on ensuring each path (inotify-triggered store, manual delete, remote change) updated the cache and event counter exactly once so callbacks never missed a mutation.


### High-level design
- **Metadata cache + event timeline:** Both the server and each client keep a `std::map<string, dfs_file_info_t>` cache keyed by filename. The server seeds it from disk at startup (`dfs_initialize_local_state`), protects it with `fs_change_mutex`, and bumps an `fs_event_time` counter every time a store/delete updates the cache. Clients lazily initialize the same structure from their mounts and keep it consistent after every local mutation.
- **Callback-based sync:** `CallbackList` now returns the full cache as a protobuf map plus the event epoch. The server snapshots the cache under the mutex and releases the lock before serializing so callbacks never block writes. Clients reconcile by iterating over the union of local + server filenames and following the required policy: identical CRC ⇒ no-op, server newer/equal ⇒ Fetch, client newer ⇒ Store, server tombstone ⇒ delete locally.
- **Write-lock enforcement:** Before mutating files, clients request a lock via the provided RPCs. The server tracks `filename → holder` in `write_locks`, and the client ensures the lock is released even on error. Stores now stream chunks immediately after the metadata frame and the server only skips the write when the CRC already matches, which prevents the “server file is newer” false positives seen earlier.
- **File transfer optimizations:** Fetch short-circuits when CRCs match, and Store skips rewriting the same content. Both sides recompute CRCs using the shared `CRC::Table` helper, keeping hash calculations O(1) over chunks.

### Notable challenges & lessons
- **Callback correctness:** The hardest bug was gRPC context lifetime. We learned not to touch `ServerContext` (especially `IsCancelled`) inside the condition-variable predicate because the completion queue may free it. Snapshotting the event counter before waiting fixed the crashes.
- **Concurrency:** Ensuring the async callback thread and inotify-triggered operations do not race required routing all DFS mutations through the same `sync_mutex`. This serialized local state changes and simplified reasoning about deadlocks.
- **Testing wishes:** The provided suite doesn’t stress simultaneous tombstones + recreations. I’d add a test where two clients delete/recreate the same file in rapid succession to ensure the cache correctly distinguishes between “deleted” and “re-uploaded” states.
- **Documentation gap:** The starter readme implies synchronous RPCs run on a single thread. In practice gRPC spins one thread per incoming RPC, so we documented internally that every handler must be thread-safe even without async completion queues.

### Future improvements
- Garbage-collect stale cache entries once every client has observed the tombstone for a while to avoid unbounded map growth.
- Batch callback payloads by sending only deltas instead of the entire cache to reduce bandwidth when a directory contains thousands of files.

## Known Bugs/Issues/Limitations
- **Server restart rescan:** When the server restarts it rebuilds the cache from disk but emits the entire snapshot at epoch 1, so clients briefly redo work they already performed before the restart. Persisting the event counter could eliminate this blip.
- **Temporary files:** The client still emits warnings when `.tmp` files appear during Fetch. These are harmless but noise in the logs. A more polished solution would track temporary extensions and auto-delete them silently.
- **No bandwidth throttling:** Large concurrent Stores are unthrottled and can starve Fetch responses. A future iteration would limit outstanding writers per client or add a streaming window.

## References
- gRPC C++ async/sync API reference: https://grpc.io/docs/languages/cpp/
- Google Protocol Buffers language guide (proto3): https://protobuf.dev/programming-guides/proto3/
- Andrew File System (AFS): https://en.wikipedia.org/wiki/Andrew_File_System
