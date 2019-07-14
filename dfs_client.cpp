#include <iostream>

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

    bool get_dir() {
        ClientContext context;
        Void req;
        Str res;
        Status status = this->_stub->get_dir(&context, req, &res);
        if (!status.ok()) {
            std::cout << "get_cwd failed" << std::endl;
        } else {
            std::cout << "get_cwd succeed, returned " << res.content() << std::endl;
        }
        return true;
    }

private:
    std::unique_ptr<dfs::DFS::Stub> _stub;
};


int main(int argc, char** argv) {
    DFSClient client(
            grpc::CreateChannel("localhost:50051",
            grpc::InsecureChannelCredentials())
            );
    client.get_dir();

    return 0;
}
