#include <regex>
#include <mutex>
#include <sys/stat.h>
#include <vector>
#include <set>
#include <string>
#include <thread>
#include <cstdio>
#include <chrono>
#include <ctime>
#include <errno.h>
#include <csignal>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <sys/inotify.h>
#include <grpcpp/grpcpp.h>
#include <utime.h>

#include "src/dfs-utils.h"
#include "src/dfslibx-clientnode-p2.h"
#include "dfslib-shared-p2.h"
#include "dfslib-clientnode-p2.h"
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Status;
using grpc::Channel;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::ClientContext;

extern dfs_log_level_e DFS_LOG_LEVEL;

//
// STUDENT INSTRUCTION:
//
// Change these "using" aliases to the specific
// message types you are using to indicate
// a file request and a listing of files from the server.
//
using FileRequestType = dfs_service::CallbackListRequest;
using FileListResponseType = dfs_service::CallbackListResponse;

DFSClientNodeP2::DFSClientNodeP2() : DFSClientNode() {}
DFSClientNodeP2::~DFSClientNodeP2() {}

grpc::StatusCode DFSClientNodeP2::RequestWriteAccess(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to obtain a write lock here when trying to store a file.
    // This method should request a write lock for the given file at the server,
    // so that the current client becomes the sole creator/writer. If the server
    // responds with a RESOURCE_EXHAUSTED response, the client should cancel
    // the current file storage
    //
    // The StatusCode response should be:
    //
    // OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //
    ClientContext context;
    auto deadline = std::chrono::system_clock::now() +
               std::chrono::milliseconds(this->deadline_timeout);
    context.set_deadline(deadline);

    dfs_service::LockRequest lockReq;
    lockReq.set_file_name(filename);
    lockReq.set_client_id(this->client_id);
    dfs_service::LockResponse resp;
    dfs_log(LL_DEBUG) << "Acquire Lock for file : " << filename;
    Status rpc_status = this->service_stub->AcquireWriteLock(&context, lockReq, &resp);
    dfs_log(LL_DEBUG) << "Acquire Lock Response status: " << rpc_status.error_message();
    dfs_log(LL_DEBUG) << "Acquire Lock Response: granted=" << resp.granted() << " holder=" << resp.holder();
    if (resp.granted()) {
        // lock acquired - rpc_status should be OK too
        // but writing this is more explicit
        return StatusCode::OK;
    } else {
        return rpc_status.error_code();
    }

}

grpc::StatusCode DFSClientNodeP2::ReleaseWriteAccess(const std::string &filename) {
    ClientContext context;
    auto deadline = std::chrono::system_clock::now() +
               std::chrono::milliseconds(this->deadline_timeout);
    context.set_deadline(deadline);

    dfs_service::LockRequest lockReq;
    lockReq.set_file_name(filename);
    lockReq.set_client_id(this->client_id);
    dfs_service::LockResponse resp;
    dfs_log(LL_DEBUG) << "Release Lock for file : " << filename;
    Status rpc_status = this->service_stub->ReleaseWriteLock(&context, lockReq, &resp);
    dfs_log(LL_DEBUG) << "Release Lock Response status: " << rpc_status.error_message();
    dfs_log(LL_DEBUG) << "Release Lock Response: granted=" << resp.granted() << " holder=" << resp.holder();
    return rpc_status.error_code();
}

StatusCode DFSClientNodeP2::Store(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to store a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept and store a file.
    //
    // When working with files in gRPC you'll need to stream
    // the file contents, so consider the use of gRPC's ClientWriter.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the client
    // StatusCode::CANCELLED otherwise
    //

    // Acquiring local lock before performing DF mutation operations
    std::lock_guard<std::mutex> lock(this->sync_mutex);
    EnsureLocalStateInitialized();

    StatusCode writeLockStatusCode = this->RequestWriteAccess(filename);
    if (writeLockStatusCode != StatusCode::OK) {
        dfs_log(LL_DEBUG) << "Lock not acquired. statusCode=" << writeLockStatusCode;
        return StatusCode::CANCELLED;
    } else {
        dfs_log(LL_DEBUG) << "Write lock acquired for file: " << filename;
    }
    const std::string path = WrapPath(filename);
    dfs_log(LL_DEBUG) << "Storing: " << path;

    ClientContext context;
    auto deadline = std::chrono::system_clock::now() +
               std::chrono::milliseconds(this->deadline_timeout);
    context.set_deadline(deadline);

    dfs_service::StoreResponse resp;
    std::unique_ptr<ClientWriter<dfs_service::FileChunk>> writer(
        this->service_stub->Store(&context, &resp));
    std::ifstream file(path, std::ios::binary | std::ios::in);
    if (!file) {
        dfs_log(LL_ERROR) << "Unable to open file: " << path;
        return StatusCode::CANCELLED;
    }

    const size_t chunk_size = 64 * 1024; // 64 MB
    std::string buffer;
    buffer.resize(chunk_size);
    // int64_t offset = 0;

    // Send the first chunk containing the file metadata for the server to check mtime and crc
    dfs_service::FileChunk metadata_chunk;
    metadata_chunk.set_client_id(this->client_id);
    metadata_chunk.set_file_name(filename);
    metadata_chunk.set_mtime(get_file_mtime(path));
    metadata_chunk.set_crc(dfs_file_checksum(path, &this->crc_table));
    writer->Write(metadata_chunk);

    while (file) {
        file.read(&buffer[0], chunk_size);
        std::streamsize bytesRead = file.gcount();
        dfs_log(LL_DEBUG) << "Streaming " << bytesRead << " bytes from file.";
        if (bytesRead <= 0) break; // done

        dfs_service::FileChunk chunk;
        chunk.set_client_id(this->client_id);
        chunk.set_file_name(filename);
        chunk.set_data(buffer.data(), static_cast<size_t>(bytesRead));

        if (!writer->Write(chunk)){
            dfs_log(LL_ERROR) << "Server closed stream unexpectedly";
            break;
        }
    }
    writer->WritesDone();
    dfs_log(LL_DEBUG) << "Finished sending file data, waiting for server response...";
    Status rpcStatus = writer->Finish();
    if (!rpcStatus.ok()) {
        dfs_log(LL_ERROR) << "Store failed: status=" << rpcStatus.error_code() 
                                << " message=" << rpcStatus.error_message();
    } else {
        dfs_log(LL_DEBUG) << "Store done: status=" << resp.ok() << ". message=" << resp.message();
        dfs_file_info_t info = BuildLocalFileInfo(filename);
        update_local_state(filename, info);
    }

    // Release the write lock after storing
    grpc::StatusCode releaseStatusCode = this->ReleaseWriteAccess(filename);
    if (releaseStatusCode != StatusCode::OK) {
        dfs_log(LL_ERROR) << "Failed to release write lock for file: " << filename 
                            << " statusCode=" << releaseStatusCode;
    } else {
        dfs_log(LL_DEBUG) << "Released write lock for file: " << filename;
    }

    return rpcStatus.error_code();
}


StatusCode DFSClientNodeP2::Fetch(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to fetch a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept a file request and return the contents
    // of a file from the service.
    //
    // As with the store function, you'll need to stream the
    // contents, so consider the use of gRPC's ClientReader.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //

    // Acquiring local lock before performing DF mutation operations
    std::lock_guard<std::mutex> lock(this->sync_mutex);
    EnsureLocalStateInitialized();

    ClientContext context;
    auto deadline = std::chrono::system_clock::now() +
               std::chrono::milliseconds(this->deadline_timeout);
    context.set_deadline(deadline);

    const std::string path = WrapPath(filename);
    struct stat st{};
    dfs_service::FetchRequest fetchReq;

    fetchReq.set_client_id(this->client_id);
    fetchReq.set_file_name(filename);
    if (stat(path.c_str(), &st) == 0) { // stat returns no error -> file exists
        dfs_log(LL_DEBUG) << "Local file exists. " << path << ". Attempt to overwrite with file from server.";
        fetchReq.set_crc(dfs_file_checksum(path, &this->crc_table));
        fetchReq.set_mtime(get_file_mtime(WrapPath(filename)));
    }

    std::unique_ptr<grpc::ClientReader<dfs_service::FileChunk>> reader(
            this->service_stub->Fetch(&context, fetchReq));

    std::ofstream outfile;
    bool file_opened = false;
    std::string tmp_path = path + ".tmp";
    dfs_service::FileChunk chunk;
    size_t total_bytes = 0;

    dfs_log(LL_DEBUG) << "Fetch: " << path;

    while (reader->Read(&chunk)) {
        if (!file_opened) {
            outfile.open(tmp_path, std::ios::binary | std::ios::out | std::ios::trunc);
            if (!outfile) {
                dfs_log(LL_ERROR) << "Unable to open output " << tmp_path;
                reader->Finish();
                return StatusCode::CANCELLED;
            }
            file_opened = true;
        }
        outfile.write(chunk.data().data(), chunk.data().size());
        total_bytes += chunk.data().size();
        dfs_log(LL_DEBUG) << "Received: " << chunk.data().size() << " bytes. Total: " << total_bytes;
    }
    dfs_log(LL_DEBUG) << "Finished receiving file data. Total bytes: " << total_bytes;
    if (file_opened) {
        outfile.close();
        if (total_bytes > 0) {
            std::rename(tmp_path.c_str(), path.c_str());
        } else {
            std::remove(tmp_path.c_str());
        }
    }

    Status rpcStatus = reader->Finish();
    StatusCode status_code = rpcStatus.error_code();
    if (status_code == StatusCode::OK) {
        dfs_file_info_t info = BuildLocalFileInfo(filename);
        update_local_state(filename, info);
    }
    dfs_log(LL_DEBUG) << "Fetch done: " << "status=" << status_code 
                                << " message=" << rpcStatus.error_message();
    return status_code;
}

StatusCode DFSClientNodeP2::Delete(const std::string& filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to delete a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //

    // Acquiring local lock before performing DF mutation operations
    std::lock_guard<std::mutex> lock(this->sync_mutex);
    EnsureLocalStateInitialized();

    StatusCode writeLockStatusCode = this->RequestWriteAccess(filename);
    if (writeLockStatusCode != StatusCode::OK) {
        dfs_log(LL_DEBUG) << "Lock not acquired. statusCode=" << writeLockStatusCode;
        return StatusCode::CANCELLED;
    } else {
        dfs_log(LL_DEBUG) << "Write lock acquired for file: " << filename;
    }
    const std::string path = WrapPath(filename);
    dfs_log(LL_DEBUG) << "Deleting: " << path;

    ClientContext context;
    auto deadline = std::chrono::system_clock::now() +
               std::chrono::milliseconds(this->deadline_timeout);
    context.set_deadline(deadline);

    dfs_service::DeleteRequest deleteReq;
    deleteReq.set_client_id(this->client_id);
    deleteReq.set_file_name(filename);
    dfs_service::DeleteResponse resp;
    dfs_log(LL_DEBUG) << "Delete: " << filename;
    Status rpc_status = this->service_stub->Delete(&context, deleteReq, &resp);
    dfs_log(LL_DEBUG) << "Delete Response status: " << resp.message();

    if (rpc_status.ok()) {
        std::remove(path.c_str());
        dfs_file_info_t info;
        info.file_name = filename;
        info.deleted = true;
        info.crc = 0;
        info.mtime = static_cast<int>(std::time(nullptr));
        update_local_state(filename, info);
    }


    // Release the write lock after deleting
    grpc::StatusCode releaseStatusCode = this->ReleaseWriteAccess(filename);
    if (releaseStatusCode != StatusCode::OK) {
        dfs_log(LL_ERROR) << "Failed to release write lock for file: " << filename 
                            << " statusCode=" << releaseStatusCode;
    } else {
        dfs_log(LL_DEBUG) << "Released write lock for file: " << filename;
    }

    return rpc_status.error_code();
}

StatusCode DFSClientNodeP2::List(std::map<std::string,int>* file_map, bool display) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to list all files here. This method
    // should connect to your service's list method and return
    // a list of files using the message type you created.
    //
    // The file_map parameter is a simple map of files. You should fill
    // the file_map with the list of files you receive with keys as the
    // file name and values as the modified time (mtime) of the file
    // received from the server.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::CANCELLED otherwise
    //
    
    ClientContext context;
    auto deadline = std::chrono::system_clock::now() +
               std::chrono::milliseconds(this->deadline_timeout);
    context.set_deadline(deadline);

    dfs_service::ListRequest listReq;
    listReq.set_client_id(this->client_id);

    dfs_service::ListResponse resp;

    dfs_log(LL_DEBUG) << "List: ";
    Status rpc_status = this->service_stub->List(&context, listReq, &resp);
    dfs_log(LL_DEBUG) << "List Response status: " << rpc_status.error_message();
    dfs_log(LL_DEBUG) << "List Response file_info len: " << resp.file_info().size();
    for (auto& entry: resp.file_info()) {
        (*file_map)[entry.first] = static_cast<int>(entry.second);
        dfs_log(LL_DEBUG) << "File map entry: " << entry.first << " -> " << entry.second;
    }
    return rpc_status.error_code();
}

grpc::StatusCode DFSClientNodeP2::Stat(const std::string &filename, void* file_status) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation and add any additional
    // status details that would be useful to your solution.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //
    return StatusCode::OK;
}

void DFSClientNodeP2::InotifyWatcherCallback(std::function<void()> callback) {

    //
    // STUDENT INSTRUCTION:
    //
    // This method gets called each time inotify signals a change
    // to a file on the file system. That is every time a file is
    // modified or created.
    //
    // You may want to consider how this section will affect
    // concurrent actions between the inotify watcher and the
    // asynchronous callbacks associated with the server.
    //
    // The callback method shown must be called here, but you may surround it with
    // whatever structures you feel are necessary to ensure proper coordination
    // between the async and watcher threads.
    //
    // Hint: how can you prevent race conditions between this thread and
    // the async thread when a file event has been signaled?
    //

    // Acquiring local lock before performing DF mutation operations
    // std::lock_guard<std::mutex> lock(this->sync_mutex);
    // Core calback logic (check file status, store to server, etc) already handled in dfs-client-p2.cpp
    callback();

}

//
// STUDENT INSTRUCTION:
//
// This method handles the gRPC asynchronous callbacks from the server.
// We've provided the base structure for you, but you should review
// the hints provided in the STUDENT INSTRUCTION sections below
// in order to complete this method.
//
void DFSClientNodeP2::HandleCallbackList() {

    void* tag;

    bool ok = false;

    //
    // STUDENT INSTRUCTION:
    //
    // Add your file list synchronization code here.
    //
    // When the server responds to an asynchronous request for the CallbackList,
    // this method is called. You should then synchronize the
    // files between the server and the client based on the goals
    // described in the readme.
    //
    // In addition to synchronizing the files, you'll also need to ensure
    // that the async thread and the file watcher thread are cooperating. These
    // two threads could easily get into a race condition where both are trying
    // to write or fetch over top of each other. So, you'll need to determine
    // what type of locking/guarding is necessary to ensure the threads are
    // properly coordinated.
    //

    // Block until the next result is available in the completion queue.
    while (completion_queue.Next(&tag, &ok)) {
        {
            //
            // STUDENT INSTRUCTION:
            //
            // Consider adding a critical section or RAII style lock here
            //

            // The tag is the memory location of the call_data object
            AsyncClientData<FileListResponseType> *call_data = static_cast<AsyncClientData<FileListResponseType> *>(tag);

            dfs_log(LL_DEBUG2) << "Received completion queue callback";

            // Verify that the request was completed successfully. Note that "ok"
            // corresponds solely to the request for updates introduced by Finish().
            // GPR_ASSERT(ok);
            if (!ok) {
                dfs_log(LL_ERROR) << "Completion queue callback not ok.";
            }

            if (ok && call_data->status.ok()) {

                dfs_log(LL_DEBUG3) << "Handling async callback ";

                //
                // STUDENT INSTRUCTION:
                //
                // Add your handling of the asynchronous event calls here.
                // For example, based on the file listing returned from the server,
                // how should the client respond to this updated information?
                // Should it retrieve an updated version of the file?
                // Send an update to the server?
                // Do nothing?
                //
                
                // Reconcile the file list from the server with the local file system
                uint64_t new_event_time = call_data->reply.event_time();
                dfs_log(LL_DEBUG) << "Event time from server: " << new_event_time;
                std::map<std::string,dfs_file_info_t> server_file_map;
                for (const auto& entry : call_data->reply.files()) {
                    dfs_file_info_t info;
                    info.file_name = entry.first;
                    info.mtime = static_cast<int>(entry.second.mtime());
                    info.crc = entry.second.crc();
                    info.deleted = entry.second.deleted();
                    server_file_map[info.file_name] = info;
                }

                std::map<std::string,dfs_file_info_t> local_snapshot;
                {
                    std::unique_lock<std::mutex> lock(this->sync_mutex);
                    EnsureLocalStateInitialized();
                    local_snapshot = this->local_state;
                }

                std::set<std::string> filenames;
                for (const auto &entry : server_file_map) {
                    filenames.insert(entry.first);
                }
                for (const auto &entry : local_snapshot) {
                    filenames.insert(entry.first);
                }

                dfs_log(LL_DEBUG) << "Reconciling " << filenames.size() << " files against server snapshot";
                for (const auto &filename : filenames) {
                    dfs_file_info_t server_info;
                    dfs_file_info_t client_info;

                    auto server_it = server_file_map.find(filename);
                    if (server_it != server_file_map.end()) {
                        server_info = server_it->second;
                    } else {
                        server_info.file_name = filename;
                        server_info.mtime = 0;
                        server_info.crc = 0;
                        server_info.deleted = false;
                    }

                    auto client_it = local_snapshot.find(filename);
                    if (client_it != local_snapshot.end()) {
                        client_info = client_it->second;
                    } else {
                        client_info.file_name = filename;
                        client_info.mtime = 0;
                        client_info.crc = 0;
                        client_info.deleted = true;
                    }

                    if (server_info.deleted && client_info.deleted) {
                        continue;
                    }

                    if (server_info.deleted && !client_info.deleted) {
                        dfs_log(LL_DEBUG) << "Server tombstone detected for " << filename << ", removing local copy";
                        const std::string path = WrapPath(filename);
                        std::remove(path.c_str());
                        dfs_file_info_t tombstone = client_info;
                        tombstone.deleted = true;
                        tombstone.crc = 0;
                        tombstone.mtime = server_info.mtime;
                        {
                            std::lock_guard<std::mutex> state_lock(this->sync_mutex);
                            update_local_state(filename, tombstone);
                        }
                        continue;
                    }

                    if (!server_info.deleted && client_info.deleted) {
                        dfs_log(LL_DEBUG) << "Missing locally, fetching: " << filename;
                        StatusCode status = this->Fetch(filename);
                        if (status == StatusCode::OK) {
                            dfs_log(LL_DEBUG) << "Fetched file from server: " << filename;
                        } else if (status != StatusCode::ALREADY_EXISTS) {
                            dfs_log(LL_ERROR) << "Failed to fetch file from server: " << filename << " status=" << status;
                        }
                        continue;
                    }

                    if (server_info.crc == client_info.crc) {
                        dfs_log(LL_DEBUG3) << "CRC match for " << filename << ", no action.";
                        continue;
                    }

                    if (server_info.mtime >= client_info.mtime) {
                        dfs_log(LL_DEBUG) << "Server copy newer/equal, fetching: " << filename;
                        StatusCode status = this->Fetch(filename);
                        if (status == StatusCode::OK) {
                            dfs_log(LL_DEBUG) << "Fetched file from server: " << filename;
                        } else if (status != StatusCode::ALREADY_EXISTS) {
                            dfs_log(LL_ERROR) << "Failed to fetch file from server: " << filename << " status=" << status;
                        }
                    } else {
                        dfs_log(LL_DEBUG) << "Client copy newer, storing: " << filename;
                        StatusCode status = this->Store(filename);
                        if (status != StatusCode::OK) {
                            dfs_log(LL_ERROR) << "Failed to store file to server: " << filename << " status=" << status;
                        }
                    }
                }
                // Progress local event_time after update all files to this snapshot
                this->event_time = new_event_time;
            } else {
                dfs_log(LL_ERROR) << "Status was not ok. Will try again in " << DFS_RESET_TIMEOUT << " milliseconds.";
                dfs_log(LL_ERROR) << call_data->status.error_message();
                std::this_thread::sleep_for(std::chrono::milliseconds(DFS_RESET_TIMEOUT));
            }
            // Once we're complete, deallocate the call_data object.
            delete call_data;

            //
            // STUDENT INSTRUCTION:
            //
            // Add any additional syncing/locking mechanisms you may need here

        }


        // Start the process over and wait for the next callback response
        dfs_log(LL_DEBUG3) << "Calling InitCallbackList";
        InitCallbackList();

    }
}

/**
 * This method will start the callback request to the server, requesting
 * an update whenever the server sees that files have been modified.
 *
 * We're making use of a template function here, so that we can keep some
 * of the more intricate workings of the async process out of the way, and
 * give you a chance to focus more on the project's requirements.
 */
void DFSClientNodeP2::InitCallbackList() {
    dfs_log(LL_DEBUG) << "Send CallbackList";
    CallbackList<FileRequestType, FileListResponseType>();
}

void DFSClientNodeP2::EnsureLocalStateInitialized() {
    if (this->local_state_initialized) {
        return;
    }
    this->local_state = dfs_initialize_local_state(this->mount_path, &this->crc_table);
    this->local_state_initialized = true;
}

dfs_file_info_t DFSClientNodeP2::BuildLocalFileInfo(const std::string &filename) {
    dfs_file_info_t info;
    info.file_name = filename;
    const std::string path = WrapPath(filename);
    struct stat st{};
    if (stat(path.c_str(), &st) == 0) {
        info.mtime = static_cast<int>(st.st_mtime);
        info.crc = dfs_file_checksum(path, &this->crc_table);
        info.deleted = false;
    } else {
        info.mtime = static_cast<int>(std::time(nullptr));
        info.crc = 0;
        info.deleted = true;
    }
    return info;
}

bool DFSClientNodeP2::update_local_state(const std::string &filename, const dfs_file_info_t &info) {
    this->local_state[filename] = info;
    return true;
}

//
// STUDENT INSTRUCTION:
//
// Add any additional code you need to here
//

