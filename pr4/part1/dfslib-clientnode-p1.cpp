#include <cstddef>
#include <grpcpp/impl/codegen/status.h>
#include <grpcpp/impl/codegen/status_code_enum.h>
#include <regex>
#include <vector>
#include <string>
#include <thread>
#include <cstdio>
#include <chrono>
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

#include "dfslib-shared-p1.h"
#include "dfslib-clientnode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"
#include "proto-src/dfs-service.pb.h"

using grpc::Status;
using grpc::Channel;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::ClientContext;

//
// STUDENT INSTRUCTION:
//
// You may want to add aliases to your namespaced service methods here.
// All of the methods will be under the `dfs_service` namespace.
//
// For example, if you have a method named MyMethod, add
// the following:
//
//      using dfs_service::MyMethod
//

#define DEADLINE_TIMEOUT 100

DFSClientNodeP1::DFSClientNodeP1() : DFSClientNode() {}

DFSClientNodeP1::~DFSClientNodeP1() noexcept {}

StatusCode DFSClientNodeP1::Store(const std::string &filename) {

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

    ClientContext context;
    auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(DEADLINE_TIMEOUT);
    context.set_deadline(deadline);

    dfs_service::StoreResponse resp;

    std::unique_ptr<ClientWriter<dfs_service::FileChunk>> writer(
        this->service_stub->Store(&context, &resp));
    const std::string path = WrapPath(filename);
    std::ifstream file(path, std::ios::binary | std::ios::in);
    if (!file) {
        dfs_log(LL_ERROR) << "Unable to open file: " << path;
        return StatusCode::CANCELLED;
    }

    const size_t chunk_size = 64 * 1024; // 64 MB
    dfs_log(LL_DEBUG) << "Storing: " << path;
    std::string buffer;
    buffer.resize(chunk_size);
    // int64_t offset = 0;

    while (file) {
        file.read(&buffer[0], chunk_size);
        std::streamsize bytesRead = file.gcount();
        dfs_log(LL_DEBUG) << "Streaming " << bytesRead << " bytes from file.";
        if (bytesRead <= 0) break; // done

        dfs_service::FileChunk chunk;
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
        dfs_log(LL_ERROR) << "Store failed: " << rpcStatus.error_message();
    } else {
        dfs_log(LL_DEBUG) << "Store done: status=" << resp.ok() << ". message=" << resp.message();
    }
    return rpcStatus.error_code();
}


StatusCode DFSClientNodeP1::Fetch(const std::string &filename) {

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
    //
    ClientContext context;
    auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(DEADLINE_TIMEOUT);
    context.set_deadline(deadline);

    dfs_service::FetchRequest fetchReq;
    fetchReq.set_file_name(filename);

    std::unique_ptr<grpc::ClientReader<dfs_service::FileChunk>> reader(
            this->service_stub->Fetch(&context, fetchReq));

    const std::string path = WrapPath(filename);
    std::ofstream outfile(path, std::ios::binary | std::ios::out);
    if (!outfile) {
        std::cerr << "Unable to open output " << path << "\n";
        return StatusCode::CANCELLED;
    }

    dfs_service::FileChunk chunk;

    dfs_log(LL_DEBUG) << "Fetch: " << path;
    size_t total_bytes = 0;
    while (reader->Read(&chunk)) {
        outfile.write(chunk.data().data(), chunk.data().size());
        total_bytes += chunk.data().size();
        dfs_log(LL_DEBUG) << "Received: " << chunk.data().size() << " bytes. Total: " << total_bytes;
    }
    dfs_log(LL_DEBUG) << "Finished receiving file data. Total bytes: " << total_bytes;
    outfile.close();
    Status rpcStatus = reader->Finish();
    if (!rpcStatus.ok()) {
        dfs_log(LL_ERROR) << "Fetch failed: " << rpcStatus.error_message();
    } else {
        dfs_log(LL_DEBUG) << "Fetch done: " << rpcStatus.error_code();
    }
    return rpcStatus.error_code();
}

StatusCode DFSClientNodeP1::Delete(const std::string& filename) {

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

    ClientContext context;
    auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(DEADLINE_TIMEOUT);
    context.set_deadline(deadline);

    dfs_service::DeleteRequest deleteReq;
    deleteReq.set_file_name(filename);
    dfs_service::DeleteResponse resp;
    dfs_log(LL_DEBUG) << "Delete: " << filename;
    Status rpc_status = this->service_stub->Delete(&context, deleteReq, &resp);
    dfs_log(LL_DEBUG) << "Delete Response status: " << resp.message();
    return rpc_status.error_code();
}

StatusCode DFSClientNodeP1::List(std::map<std::string,int>* file_map, bool display) {

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
    auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(DEADLINE_TIMEOUT);
    context.set_deadline(deadline);

    dfs_service::ListRequest listReq;

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

StatusCode DFSClientNodeP1::Stat(const std::string &filename, void* file_status) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. This method should
    // retrieve the status of a file on the server. Note that you won't be
    // tested on this method, but you will most likely find that you need
    // a way to get the status of a file in order to synchronize later.
    //
    // The status might include data such as name, size, mtime, crc, etc.
    //
    // The file_status is left as a void* so that you can use it to pass
    // a message type that you defined. For instance, may want to use that message
    // type after calling Stat from another method.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //

    return StatusCode::OK;
}

//
// STUDENT INSTRUCTION:
//
// Add your additional code here, including
// implementations of your client methods
//


