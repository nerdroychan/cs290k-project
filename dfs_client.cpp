#include <iostream>
#include <vector>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include "protos/dfs.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;

using dfs::Void;
using dfs::Str;
using dfs::Bool;
using dfs::Int;
using dfs::Dentry;
using dfs::Block;
using dfs::WriteRequest;
using dfs::ReadResponse;
using dfs::RandomReadRequest;

using std::string;

class DFSClient {
public:
    DFSClient(std::shared_ptr<Channel> channel) : \
        _stub(dfs::DFS::NewStub(channel)) {}

    void getdir() {
        ClientContext context;
        Void req;
        Str res;
        Status status = this->_stub->get_dir(&context, req, &res);
        if (!status.ok()) {
            std::cout << "get_cwd failed" << std::endl;
        } else {
            std::cout << "get_cwd succeed, returned " << res.content() << std::endl;
        }
    }

    void cd(const string path) {
        ClientContext context;
        Str req;
        Bool res;
        req.set_content(path);
        Status status = this->_stub->change_dir(&context, req, &res);
        if (!status.ok() || res.value() == false) {
            std::cout << "change_dir failed" << std::endl;
        } else {
            std::cout << "change_dir succeed, returned " << res.value() \
                << std::endl;
        }
    }

    void filecount() {
        ClientContext context;
        Void req;
        Int res;
        Status status = this->_stub->file_count(&context, req, &res);
        if (!status.ok() || res.value() == -1) {
            std::cout << "file_count failed" << std::endl;
        } else {
            std::cout << "file_count succeed, returned " << res.value() \
                << std::endl;
        }
    }

    void ls(bool list, const string path) {
        // invoke open_list
        ClientContext open_list_context;
        Str open_list_req;
        Bool open_list_res;

        open_list_req.set_content(path);
        Status open_list_status = this->_stub->open_list(&open_list_context, \
                open_list_req, &open_list_res);
        if (!open_list_status.ok() || !open_list_res.value()) {
            std::cout << "ls failed due to open_list failure" << std::endl;
            return;
        }
        
        // invoke next_list
        std::vector<Dentry> files;
        Void next_list_req;
        Dentry next_list_res;

        while(next_list_res.size() >= 0) {
            ClientContext next_list_context;
            Status next_list_status = this->_stub->next_list(
                    &next_list_context, next_list_req, &next_list_res);
            if (!next_list_status.ok() || next_list_res.size() == -1) {
                std::cout << "ls failed due to next_list failue" << std::endl;
                return;
            }
            if (next_list_res.size() > 0) files.push_back(next_list_res);
            else break;
        }

        // invoke close_list
        ClientContext close_list_context;
        Void close_list_req;
        Bool close_list_res;

        Status close_list_status = this->_stub->close_list(
                &close_list_context, close_list_req, &close_list_res
                );
        if (!close_list_status.ok() || !close_list_res.value()) {
            std::cout << "ls failed due to close_list failure" << std::endl;
            return;
        }

        for (std::vector<Dentry>::iterator it = files.begin();
                it != files.end(); it++) {
            std::cout << it->name() << " ";
            if (list) {
                std::cout << it->is_directory() << " " << it->size() << " " \
                    << it->last_modified_time() << " " << std::endl;
            }
        }
    } 

    void ls(bool list) {
        ClientContext context;
        Void req;
        Str res;
        Status status = this->_stub->get_dir(&context, req, &res);
        if (!status.ok()) {
            std::cout << "ls failed due to get_dir failure" <<std::endl;
        } else {
            ls(list, res.content());
        }
    }

    void put(const string localpath, const string remotepath) {
    }

    void put(const string localpath) {
    }

    void get(const string remotepath, const string localpath) {
    }

    void get(const string remotepath) {
    }

    void randomread(const string remotepath, int firstbyte, int numbytes) {
    }

    void shell() {
        this->ls(true);
    }

private:
    std::unique_ptr<dfs::DFS::Stub> _stub;
};


int main(int argc, char** argv) {
    DFSClient client(
            grpc::CreateChannel("localhost:50051",
            grpc::InsecureChannelCredentials())
            );

    client.shell();

    return 0;
}
