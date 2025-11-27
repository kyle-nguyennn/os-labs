#include <cstddef>
#include <map>
#include <chrono>
#include <cstdio>
#include <sstream>
#include <string>
#include <thread>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <grpcpp/grpcpp.h>

#include "proto-src/dfs-service.pb.h"
#include "src/dfs-utils.h"
#include "dfslib-shared-p1.h"
#include "dfslib-servernode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Status;
using grpc::Server;
using grpc::StatusCode;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerContext;
using grpc::ServerBuilder;

using dfs_service::DFSService;


//
// STUDENT INSTRUCTION:
//
// DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in the `dfs-service.proto` file.
//
// You should add your definition overrides here for the specific
// methods that you created for your GRPC service protocol. The
// gRPC tutorial described in the readme is a good place to get started
// when trying to understand how to implement this class.
//
// The method signatures generated can be found in `proto-src/dfs-service.grpc.pb.h` file.
//
// Look for the following section:
//
//      class Service : public ::grpc::Service {
//
// The methods returning grpc::Status are the methods you'll want to override.
//
// In C++, you'll want to use the `override` directive as well. For example,
// if you have a service method named MyMethod that takes a MyMessageType
// and a ServerWriter, you'll want to override it similar to the following:
//
//      Status MyMethod(ServerContext* context,
//                      const MyMessageType* request,
//                      ServerWriter<MySegmentType> *writer) override {
//
//          /** code implementation here **/
//      }
//
class DFSServiceImpl final : public DFSService::Service {

private:

    /** The mount path for the server **/
    std::string mount_path;

    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string &filepath) {
        return this->mount_path + filepath;
    }


public:

    DFSServiceImpl(const std::string &mount_path): mount_path(mount_path) {
    }

    ~DFSServiceImpl() {}

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // implementations of your protocol service methods
    //
    Status Store(ServerContext* context,
             ServerReader<dfs_service::FileChunk>* reader,
             dfs_service::StoreResponse* resp) override {
        dfs_service::FileChunk chunk;
        std::ofstream outfile;
        std::string filename;

        dfs_log(LL_DEBUG) << "Received Store from client " << context->peer();
        
        std::string path;
        size_t bytesReceived = 0;
        // Read the first chunk to get the filename
        if (reader->Read(&chunk)) {
            filename = chunk.file_name();
            path = WrapPath(filename);
            outfile.open(path, std::ios::binary);
            if (!outfile.is_open()) {
                return Status(StatusCode::NOT_FOUND, "cannot open file for writing");
            }
            outfile.write(chunk.data().data(), chunk.data().size());
            bytesReceived += chunk.data().size();
            dfs_log(LL_DEBUG) << "Storing: " << path << " Received: " << bytesReceived << " bytes";
        } else {
            return Status(StatusCode::CANCELLED, "no data received");
        }

        // Read the rest of the chunks
        while (reader->Read(&chunk)) {
            outfile.write(chunk.data().data(), chunk.data().size());
            bytesReceived += chunk.data().size();
            dfs_log(LL_DEBUG) << "Storing: " << path << " Received: " << bytesReceived << " bytes";
        }
        outfile.close();
        dfs_log(LL_DEBUG) << "Finished storing file: " << path << " Total bytes received: " << bytesReceived;

        if (context->IsCancelled()) {
            return Status(StatusCode::DEADLINE_EXCEEDED, "deadline");
        }
        resp->set_ok(true);
        resp->set_message("store ok");
        return Status::OK;
    }


    Status Fetch(ServerContext* context,
             const dfs_service::FetchRequest* request,
             ServerWriter<dfs_service::FileChunk> *writer) override {
        const std::string path = WrapPath(request->file_name());
        std::ifstream file(path, std::ios::binary);
        
        dfs_log(LL_DEBUG) << "Received Fetch: " << path << " from client " << context->peer();
        if (!file.is_open()) {
            return Status(StatusCode::NOT_FOUND, "missing file");
        }

        // TODO: this is for streaming later, no need for now
        dfs_service::FileChunk chunk;
        std::array<char, 4096> buffer{};
        while (file && context->IsCancelled() == false) {
            file.read(buffer.data(), buffer.size());
            std::streamsize read_bytes = file.gcount();
            if (read_bytes <= 0) break;

            chunk.set_data(buffer.data(), static_cast<size_t>(read_bytes));
            if (!writer->Write(chunk)) {
                return Status(StatusCode::CANCELLED, "stream broken");
            }
        }
        // std:std::ostringstream buffer;
        // buffer << file.rdbuf();
        // resp->set_file_content(buffer.str());
        if (context->IsCancelled()) {
            // Client cancelled due to deadline exceeded
            return Status(StatusCode::DEADLINE_EXCEEDED, "deadline");
        }
        return Status(StatusCode::OK, "fetch ok");
    }

    Status Delete(ServerContext* context,
             const dfs_service::DeleteRequest* request,
             dfs_service::DeleteResponse* resp) override {
        const std::string path = WrapPath(request->file_name());
        dfs_log(LL_DEBUG) << "Received Delete: " << path << " from client " << context->peer();

        if (std::remove(path.c_str()) != 0) {
            return Status(StatusCode::NOT_FOUND, "file not found");
        }

        if (context->IsCancelled()) {
            return Status(StatusCode::DEADLINE_EXCEEDED, "deadline");
        }
        resp->set_ok(true);
        resp->set_message("delete ok");
        return Status::OK;
    }

    Status List(ServerContext* context,
             const dfs_service::ListRequest* request,
             dfs_service::ListResponse* resp) override {
        const std::string path = WrapPath("");
        dfs_log(LL_DEBUG) << "Received List from client " << context->peer();

        DIR* dir = opendir(path.c_str());
        if (dir == nullptr) {
            return Status(StatusCode::NOT_FOUND, "missing directory");
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_REG) { // regular file
                std::string filepath = path + entry->d_name;
                struct stat file_stat;
                if (stat(filepath.c_str(), &file_stat) == 0) {
                    resp->mutable_file_info()->insert({entry->d_name, static_cast<int64_t>(file_stat.st_mtime)});
                }
            }
        }
        closedir(dir);
        if (context->IsCancelled()) {
            return Status(StatusCode::DEADLINE_EXCEEDED, "deadline");
        }
        return Status::OK;
    }
};

//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly,
// but be aware that the testing environment is expecting these three
// methods as-is.
//
/**
 * The main server node constructor
 *
 * @param server_address
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        std::function<void()> callback) :
    server_address(server_address), mount_path(mount_path), grader_callback(callback) {}

/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
    this->server->Shutdown();
}

/** Server start **/
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path);
    ServerBuilder builder;
    builder.AddListeningPort(this->server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    this->server = builder.BuildAndStart();
    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    this->server->Wait();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional DFSServerNode definitions here
//
