#include <map>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <grpcpp/grpcpp.h>

#include "proto-src/dfs-service.grpc.pb.h"
#include "src/dfslibx-call-data.h"
#include "src/dfslibx-service-runner.h"
#include "dfslib-shared-p2.h"
#include "dfslib-servernode-p2.h"

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
// Change these "using" aliases to the specific
// message types you are using in your `dfs-service.proto` file
// to indicate a file request and a listing of files from the server
//
using FileRequestType = dfs_service::CallbackListRequest;
using FileListResponseType = dfs_service::CallbackListResponse;

extern dfs_log_level_e DFS_LOG_LEVEL;

//
// STUDENT INSTRUCTION:
//
// As with Part 1, the DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in your `dfs-service.proto` file.
//
// You may start with your Part 1 implementations of each service method.
//
// Elements to consider for Part 2:
//
// - How will you implement the write lock at the server level?
// - How will you keep track of which client has a write lock for a file?
//      - Note that we've provided a preset client_id in DFSClientNode that generates
//        a client id for you. You can pass that to the server to identify the current client.
// - How will you release the write lock?
// - How will you handle a store request for a client that doesn't have a write lock?
// - When matching files to determine similarity, you should use the `file_checksum` method we've provided.
//      - Both the client and server have a pre-made `crc_table` variable to speed things up.
//      - Use the `file_checksum` method to compare two files, similar to the following:
//
//          std::uint32_t server_crc = dfs_file_checksum(filepath, &this->crc_table);
//
//      - Hint: as the crc checksum is a simple integer, you can pass it around inside your message types.
//
class DFSServiceImpl final :
    public DFSService::WithAsyncMethod_CallbackList<DFSService::Service>,
        public DFSCallDataManager<FileRequestType , FileListResponseType> {

private:

    /** The runner service used to start the service and manage asynchronicity **/
    DFSServiceRunner<FileRequestType, FileListResponseType> runner;

    /** The mount path for the server **/
    std::string mount_path;

    /** Mutex for managing the queue requests **/
    std::mutex queue_mutex;

    /** The vector of queued tags used to manage asynchronous requests **/
    std::vector<QueueRequest<FileRequestType, FileListResponseType>> queued_tags;


    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string &filepath) {
        return this->mount_path + filepath;
    }

    /** CRC Table kept in memory for faster calculations **/
    CRC::Table<std::uint32_t, 32> crc_table;

public:

    DFSServiceImpl(const std::string& mount_path, const std::string& server_address, int num_async_threads):
        mount_path(mount_path), crc_table(CRC::CRC_32()) {

        this->runner.SetService(this);
        this->runner.SetAddress(server_address);
        this->runner.SetNumThreads(num_async_threads);
        this->runner.SetQueuedRequestsCallback([&]{ this->ProcessQueuedRequests(); });

    }

    ~DFSServiceImpl() {
        this->runner.Shutdown();
    }

    void Run() {
        this->runner.Run();
    }

    /**
     * Request callback for asynchronous requests
     *
     * This method is called by the DFSCallData class during
     * an asynchronous request call from the client.
     *
     * Students should not need to adjust this.
     *
     * @param context
     * @param request
     * @param response
     * @param cq
     * @param tag
     */
    void RequestCallback(grpc::ServerContext* context,
                         FileRequestType* request,
                         grpc::ServerAsyncResponseWriter<FileListResponseType>* response,
                         grpc::ServerCompletionQueue* cq,
                         void* tag) {

        std::lock_guard<std::mutex> lock(queue_mutex);
        this->queued_tags.emplace_back(context, request, response, cq, tag);

    }

    /**
     * Process a callback request
     *
     * This method is called by the DFSCallData class when
     * a requested callback can be processed. You should use this method
     * to manage the CallbackList RPC call and respond as needed.
     *
     * See the STUDENT INSTRUCTION for more details.
     *
     * @param context
     * @param request
     * @param response
     */
    void ProcessCallback(ServerContext* context, FileRequestType* request, FileListResponseType* response) {

        //
        // STUDENT INSTRUCTION:
        //
        // You should add your code here to respond to any CallbackList requests from a client.
        // This function is called each time an asynchronous request is made from the client.
        //
        // The client should receive a list of files or modifications that represent the changes this service
        // is aware of. The client will then need to make the appropriate calls based on those changes.
        //

    }

    /**
     * Processes the queued requests in the queue thread
     */
    void ProcessQueuedRequests() {
        while(true) {

            //
            // STUDENT INSTRUCTION:
            //
            // You should add any synchronization mechanisms you may need here in
            // addition to the queue management. For example, modified files checks.
            //
            // Note: you will need to leave the basic queue structure as-is, but you
            // may add any additional code you feel is necessary.
            //


            // Guarded section for queue
            {
                dfs_log(LL_DEBUG2) << "Waiting for queue guard";
                std::lock_guard<std::mutex> lock(queue_mutex);


                for(QueueRequest<FileRequestType, FileListResponseType>& queue_request : this->queued_tags) {
                    this->RequestCallbackList(queue_request.context, queue_request.request,
                        queue_request.response, queue_request.cq, queue_request.cq, queue_request.tag);
                    queue_request.finished = true;
                }

                // any finished tags first
                this->queued_tags.erase(std::remove_if(
                    this->queued_tags.begin(),
                    this->queued_tags.end(),
                    [](QueueRequest<FileRequestType, FileListResponseType>& queue_request) { return queue_request.finished; }
                ), this->queued_tags.end());

            }
        }
    }

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // the implementations of your rpc protocol methods.
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

        struct stat st {};
        if (stat(path.c_str(), &st) != 0) {
            return Status(StatusCode::NOT_FOUND, "missing file");
        }
        const off_t file_size = st.st_size;
        dfs_log(LL_DEBUG) << "File size: " << file_size;

        size_t bytesSent = 0;
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
            bytesSent += static_cast<size_t>(read_bytes);
            dfs_log(LL_DEBUG) << path << " Sent: " << bytesSent << "/" << file_size << " bytes";
        }
        // std:std::ostringstream buffer;
        // buffer << file.rdbuf();
        // resp->set_file_content(buffer.str());
        if (context->IsCancelled()) {
            // Client cancelled due to deadline exceeded
            dfs_log(LL_DEBUG) << "Fetch cancelled by client";
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
// structure. You may add additional methods or change these slightly
// to add additional startup/shutdown routines inside, but be aware that
// the basic structure should stay the same as the testing environment
// will be expected this structure.
//
/**
 * The main server node constructor
 *
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        int num_async_threads,
        std::function<void()> callback) :
        server_address(server_address),
        mount_path(mount_path),
        num_async_threads(num_async_threads),
        grader_callback(callback) {}
/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
}

/**
 * Start the DFSServerNode server
 */
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path, this->server_address, this->num_async_threads);


    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    service.Run();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional definitions here
//
