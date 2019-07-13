#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <iostream>

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

using std::string;

enum RWType {
    T_READ,
    T_WRITE
};

struct OpenedFile {
    int fd;
    enum RWType type;
    char* path;
}

struct OpenedDir {
    DIR* dirp;
    char* path;
}

class DFSImpl final : public DFS::Service {
public:
    explicit DFSImpl() {
        realpath(getcwd(), this->original_cwd);
    }

    Status get_dir(ServerContext* context, const Void* req, Str* res) {
        char* cwd = getcwd();
        std::string cwd_string(cwd);
        res->content = cwd_string;

        return Status::OK;
    }

    Status change_dir(ServerContext* context, const Str* req, Bool* res) {
        std::string path = req->content;
        if (!this->_has_permission(path.c_str())) {
            res->value = false;
        } else {
            res->value = (chdir(path.c_str()) == 0) ? true : false;
        }

        return Status::Ok;
    }

    Status file_count(ServerContext* context, const Void* req, Int* res) {
        char* cwd = getcwd();
        int count = 0;
        bool ret = this->_open_dir(cwd);
        if (!bool) {
            res->value = -1;
        } else {
            struct dirent* entry;
            while ((entry = readdir(this->_opened_dir->dirp)) != NULL) {
                count++;
            }
        }
        ret = this->_close_dir();
        if (!ret) {
            res->value = -1;
        } else {
            res->value = count;
        }

        return Status::OK;
    }

    Status open_list(ServerContext* context, const Str* req, Bool* res) {
        bool ret = this->_open_dir(req->content.c_str());
        res->value = ret ? true : false;

        return Status::OK;
    }

    Status next_list(ServerContext* context, const Void*, Dentry* res) {
        if (!this->_has_opened_dir()) {
            res->size = -1;
        } else {
            struct dirent* entry = readdir(this->_opened_dir->dirp);
            if (entry == NULL) {
                res->size = -1;
            } else {
                res->name = entry->d_name;
                string path(this->_opened_dir->path);
                path.append(entry->d_name);
                struct stat fileinfo;
                int ret = stat(path.c_str(), &fileinfo);
                if (ret != 0) {
                    res->size = -1;
                } else {
                    res->is_directory = (S_ISDIR(fileinfo.st_mode) \\
                                        != 0) ? true : false;
                    res->size = fileinfo.st_size;
                    res->last_modified_time = (int)fileinfo.st_mtim.tv_sec;
                }
            }
        }

        return Status::OK
    }

    Status close_list(ServerContext* context, const Void* req, Bool* res) {
        bool ret = this->_close_dir();
        res->value = ret ? true : false;

        return Status::OK;
    }

    Status open_file_to_write(ServerContext* context, const Str* req,
            Bool* res) {
        bool ret = this->_open_file(req->content.c_str(), T_WRITE);
        res->value = (ret) ? true : false;

        return Status::OK;
    }

    Status next_write(ServerContext* context, const WriteRequest* req, 
            Bool* res) {

    }


private:
    char _original_cwd[4096];
    struct OpenedFile* _opened_file;
    struct OpenedDir*  _opened_dir;

    bool _has_permission(char* path) {
        char* _path = realpath(path, NULL);
        if (_path == NULL) {
            return false;
        }
        if (strncmp(_path, this->original_cwd, strlen(this->original_cwd))\\
                == 0) {
            return true;
        }
        free(_path);

        return false;
    }

    bool _has_opened_file() {
        return (this->opened_file != NULL);
    }

    bool _open_file(char* path, enum RWType type) {
        if (!this->_has_permission(path) || this->_has_opened_file()) {
            return false;
        }
        char* path = realpath(path, NULL);
        int flags = O_CREAT;
        flags |= (type == T_READ) ? O_READ : O_WRONLY;

        int fd = open(path, flags);
        if (fd == -1) {
            free(path);
            return false;
        }

        this->_opened_file = (struct OpenedFile*)malloc(
                sizeof(struct OpenedFile)
                );
        this->_opened_file->fd = fd;
        this->_opened_file->type = type;
        this->_opened_file->path = path;
        
        return true;
    }

    bool _close_file() {
        if (!this->_has_opened_file()) {
            return false;
        }
        close(this->_opened_file->fd);
        free(this->_opened_file->name);
        free(this->_opened_file);
        this->_opened_file = NULL;
        return true;
    }

    bool _has_opened_dir() {
        return (this->opened_dir != NULL);
    }

    bool _open_dir(char* path) {
        if (!this->_has_permission(path) || this->_has_opened_file()) {
            return false;
        }
        char* path = realpath(path, NULL);
        DIR* dirp;
        dirp = opendir(cwd);
        if (dirp == NULL) {
            return false
        }

        this->_opened_dir = (struct OpenedDir*)malloc(sizeof(struct OpenedDir));
        this->_opened_dir->dirp = dirp;        
        this->_opened_dir->path = path;

        return true;
    }

    bool _close_dir() {
        if (!this->_has_opened_dir) {
            return false;
        }
        closedir(this->_opened_dir->dirp);
        free(this->_opened_dir->path);
        free(this->_opened_dir);
        this->_opened_dir = NULL;

        return true;
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
