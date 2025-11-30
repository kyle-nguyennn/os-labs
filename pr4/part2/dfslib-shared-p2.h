#ifndef PR4_DFSLIB_SHARED_H
#define PR4_DFSLIB_SHARED_H

#include <algorithm>
#include <cctype>
#include <locale>
#include <cstddef>
#include <iostream>
#include <map>
#include <sstream>
#include <fstream>
#include <string>
#include <thread>
#include <sys/stat.h>

#include "src/dfs-utils.h"
#include "proto-src/dfs-service.grpc.pb.h"


//
// STUDENT INSTRUCTION
//
// The following defines and methods must be left as-is for the
// structural components provided to work.
//
#define DFS_RESET_TIMEOUT 2000
#define DFS_I_EVENT_SIZE (sizeof(struct inotify_event))
#define DFS_I_BUFFER_SIZE (1024 * (DFS_I_EVENT_SIZE + 16))

/** A file descriptor type **/
typedef int FileDescriptor;

/** A watch descriptor type **/
typedef int WatchDescriptor;

/** An inotify callback method **/
typedef void (*InotifyCallback)(uint, const std::string&, void*);

/** The expected struct for the inotify setup in this project **/
struct NotifyStruct {
    FileDescriptor fd;
    WatchDescriptor wd;
    uint event_type;
    std::thread * thread;
    InotifyCallback callback;
};

/** The expected event type for the event method in this project **/
struct EventStruct {
    void* event;
    void* instance;
};

//
// STUDENT INSTRUCTION:
//
// Add any additional shared code here
//

struct dfs_file_info_t {
    std::string file_name;
    int mtime;
    uint32_t crc;
    bool deleted = false;
};

std::map<std::string, dfs_file_info_t> get_local_file_map(
    const std::string& mount_path, CRC::Table<std::uint32_t, 32>* crc_table
);

std::map<std::string, dfs_file_info_t> dfs_initialize_local_state(
    const std::string& mount_path, CRC::Table<std::uint32_t, 32>* crc_table
);

int64_t get_file_mtime(const std::string& filepath);


#endif

