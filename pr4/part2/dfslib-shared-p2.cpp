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

std::map<std::string, dfs_file_info_t> dfs_reconcile_file_lists(
    const std::map<std::string,dfs_file_info_t>& server_file_map,
    const std::map<std::string,dfs_file_info_t>& local_file_map
) {
    std::map<std::string, dfs_file_info_t> reconcile_map;

    // Check for files in local map
    for (const auto& entry : local_file_map) {
        const std::string& filename = entry.first;
        const dfs_file_info_t& local_info = entry.second;

        auto server_it = server_file_map.find(filename);
        if (server_it == server_file_map.end()) {
            // File exists locally but not on server
            reconcile_map[filename] = local_info;
        } else {
            const dfs_file_info_t& server_info = server_it->second;
            if (local_info.mtime != server_info.mtime || local_info.crc != server_info.crc) {
                // File exists on both but differs
                reconcile_map[filename] = local_info;
            }
        }
    }

    // Check for files in server map that are missing locally
    for (const auto& entry : server_file_map) {
        const std::string& filename = entry.first;
        if (local_file_map.find(filename) == local_file_map.end()) {
            // File exists on server but not locally
            reconcile_map[filename] = entry.second;
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