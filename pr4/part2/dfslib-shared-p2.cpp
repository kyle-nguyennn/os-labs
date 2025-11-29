#include <string>
#include <iostream>
#include <fstream>
#include <cstddef>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>

#include "dfslib-shared-p2.h"
#include "proto-src/dfs-service.grpc.pb.h"

// Global log level used throughout the system
// Note: this may be adjusted from the CLI in
// both the client and server executables.
// This shouldn't be changed at the file level.
dfs_log_level_e DFS_LOG_LEVEL = LL_ERROR;

//
// STUDENT INSTRUCTION:
//
// Add your additional code here. You may use
// the shared files for anything you need to add outside
// of the main structure, but you don't have to use them.
//
// Just be aware they are always submitted, so they should
// be compilable.
//

std::map<std::string, dfs_file_info_t> get_local_file_map(
    const std::string& mount_path, CRC::Table<std::uint32_t, 32>* crc_table
) {
    std::map<std::string, dfs_file_info_t> local_file_map;

    DIR* dir = opendir(mount_path.c_str());
    if (!dir) {
        dfs_log(LL_ERROR) << "Failed to open mount path: " << mount_path;
        return local_file_map;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_REG) continue; // ignore non-regular files, i.e, directories, symlinks, sockets
        std::string abs_path = mount_path + entry->d_name;

        struct stat st{};
        if (stat(abs_path.c_str(), &st) != 0) continue;

        dfs_file_info_t info;
        info.file_name = entry->d_name;
        info.mtime = static_cast<int>(st.st_mtime);
        info.crc = dfs_file_checksum(abs_path, crc_table);
        local_file_map[info.file_name] = info;
    }
    closedir(dir);

    return local_file_map;
}

std::map<std::string, short> dfs_reconcile_file_lists(
    const std::map<std::string,dfs_file_info_t>& left,
    const std::map<std::string,dfs_file_info_t>& right
) {
    std::map<std::string, short> reconcile_map;

    // Check for files in local map
    for (const auto& entry : left) {
        const std::string& filename = entry.first;

        auto server_it = right.find(filename);
        if (server_it == right.end()) {
            // File exists on left but not on right
            reconcile_map[filename] = -1;
        } else {
            reconcile_map[filename] = 0; // exists on both
        }
    }

    // Check for files in server map that are missing locally
    for (const auto& entry : right) {
        const std::string& filename = entry.first;
        if (left.find(filename) == left.end()) {
            // File exists on right but not on left
            reconcile_map[filename] = 1;
        }
    }

    return reconcile_map;
}

int64_t get_file_mtime(const std::string& filepath) {
    struct stat st;
    if (lstat(filepath.c_str(), &st) != 0) {
        return 0;
    }
    return static_cast<int64_t>(st.st_mtime);
}