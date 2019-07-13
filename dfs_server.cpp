#include <unistd.h>


#include <string>

#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>

#include "protos/dfs.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
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


class DFSImpl final : public DFS::Service {
    Status get_dir(ServerContext* context, const Void* req, Str* res) {
        char* cwd = getcwd();
        std::string cwd_string(cwd);
        res->content = cwd_string;

        return Status::OK;
    }
}


void run_server() {
  std::string server_address("0.0.0.0:50051");
  DFSImpl service();

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char** argv) {
    run_server();
    return 0;
}
