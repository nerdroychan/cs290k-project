#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <iostream>
#include <stack>

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

#define CTIME _get_time()

string _get_time() {
    time_t rawtime;
    struct tm timeinfo;
    char buf[80];
    time(&rawtime);
    timeinfo = *localtime(&rawtime);
    strftime(buf, 80, "[%Y/%m/%d %H:%M:%S]",&timeinfo);
    string ret(buf);

    return ret;
}

enum RWType {
    T_READ,
    T_WRITE
};

struct OpenedFile {
    int fd;
    enum RWType type;
    char* path;
};

struct OpenedDir {
    DIR* dirp;
    char* path;
};

class DFSImpl final : public dfs::DFS::Service {
public:
    explicit DFSImpl() {
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);
        realpath(cwd, this->_original_cwd);
        std::cout << CTIME << " Server instance init " << std::endl;
    }

    ~DFSImpl() {
        this->_close_dir();
        this->_close_file();
    }

    Status get_dir(ServerContext* context, const Void* req, Str* res) {
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);
        std::string cwd_string(cwd);
        res->set_content(cwd_string);

        std::cout << CTIME << " Req: get_dir, " << "Res: " << cwd << std::endl;

        return Status::OK;
    }

    Status change_dir(ServerContext* context, const Str* req, Bool* res) {
        std::string path = req->content();
        if (!this->_has_permission(path.c_str())) {
            res->set_value(false);
        } else {
            res->set_value((chdir(path.c_str()) == 0) ? true : false);
        }

        std::cout <<CTIME << " Req: change_dir to " << path << ", Res: " << \
            res->value() << std::endl;

        return Status::OK;
    }

    Status file_count(ServerContext* context, const Void* req, Int* res) {
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);
        int count = 0;
        bool ret = this->_open_dir(cwd);
        if (!ret) {
            res->set_value(-1);
        } else {
            struct dirent* entry;
            while ((entry = readdir(this->_opened_dir->dirp)) != NULL) {
                count++;
            }
        }
        ret = this->_close_dir();
        if (!ret) {
            res->set_value(-1);
        } else {
            res->set_value(count);
        }

        std::cout << CTIME << " Req: file_count, Res: " << res->value() \
            << std::endl;

        return Status::OK;
    }

    Status open_list(ServerContext* context, const Str* req, Bool* res) {
        bool ret = this->_open_dir(req->content().c_str());
        res->set_value(ret ? true : false);

        std::cout << CTIME << " Req: open_list on " << req->content() \
            << ", Res: " << res->value() << std::endl;

        return Status::OK;
    }

    Status next_list(ServerContext* context, const Void*, Dentry* res) {
        if (!this->_has_opened_dir()) {
            res->set_size(-1);
        } else {
            struct dirent* entry = readdir(this->_opened_dir->dirp);
            if (entry == NULL) {
                res->set_size(0);
            } else {
                res->set_name(entry->d_name);
                string path(this->_opened_dir->path);
                path.append("/");
                path.append(entry->d_name);
                struct stat fileinfo;
                int ret = stat(path.c_str(), &fileinfo);
                if (ret != 0) {
                    res->set_size(-1);
                } else {
                    res->set_is_directory((S_ISDIR(fileinfo.st_mode)
                                        != 0) ? true : false);
                    res->set_size(fileinfo.st_size);
                    res->set_last_modified_time((int)fileinfo.st_mtime);
                }
            }
        }

        std::cout << CTIME << " Req: next_list, Res: " << res->name() << ", " \
            << res->is_directory() << ", " << res->size() << ", " \
            << res->last_modified_time() << std::endl;

        return Status::OK;
    }

    Status close_list(ServerContext* context, const Void* req, Bool* res) {
        bool ret = this->_close_dir();
        res->set_value(ret ? true : false);

        std::cout << CTIME << " Req: close_list, Res: " << res->value() \
            << std::endl;

        return Status::OK;
    }

    Status open_file_to_write(ServerContext* context, const Str* req,
            Bool* res) {
        bool ret = this->_open_file(req->content().c_str(), T_WRITE);
        res->set_value(ret ? true : false);

        std::cout << CTIME << " Req: open_file_to_write on " << req->content() \
            << ", Res: " << res->value() << std::endl;

        return Status::OK;
    }

    Status next_write(ServerContext* context, const WriteRequest* req, 
            Bool* res) {
        if (!this->_has_opened_file()) {
            res->set_value(false);
        } else {
            string content = req->block().content();
            size_t size = req->size();
            ssize_t ret = write(this->_opened_file->fd, content.c_str(), size);
            if (ret == -1) {
                res->set_value(false);
            } else {
                res->set_value(true);
            }
        }

        std::cout << CTIME << " Req: next_write (content hidden), Res: " \
            << res->value() << std::endl;

        return Status::OK;
    }

    Status open_file_to_read(ServerContext* context, const Str* req, 
            Bool* res) {
        bool ret = this->_open_file(req->content().c_str(), T_READ);
        res->set_value(ret ? true : false);

        std::cout << CTIME << " Req: open_file_to_read on " << req->content() \
            << ", Res: " << res->value() << std::endl;

        return Status::OK;
    }

    Status next_read(ServerContext* context, const Void*, ReadResponse* res) {
        if (!this->_has_opened_file()) {
            res->set_size(-1);
        } else {
            char buf[512];
            ssize_t ret = read(this->_opened_file->fd, (void*)buf, 512);
            if (ret == -1) {
                res->set_size(-1);
            } else {
                res->set_size(ret);
                res->mutable_block()->set_content(buf);
            }
        }

        std::cout << CTIME << " Req: next_read, Res: (content hidden), " \
            << res->size() << std::endl;

        return Status::OK;
    }

    Status random_read(ServerContext* context, const RandomReadRequest* req, 
            ReadResponse* res) {
        if (!this->_has_opened_file()) {
            res->set_size(-1);
        } else {
            char buf[512];
            int offset = req->offset();
            int size = req->size();
            off_t _offset = lseek(this->_opened_file->fd, offset, SEEK_SET);
            if (_offset == (off_t)-1 || size > 512) {
                res->set_size(-1);
            } else {
                ssize_t ret = read(this->_opened_file->fd, (void*)buf, size);
                if (ret == -1) {
                    res->set_size(-1);
                } else {
                    res->set_size(ret);
                    res->mutable_block()->set_content(buf);
                }
            }
        }

        std::cout << CTIME << " Req: random_read at offset " << req->offset() \
            << "with size " << req->size() << ", Res: (content hidden), " \
            << res->size() << std::endl;

        return Status::OK;
    }

    Status close_file(ServerContext* context, const Void* req, Bool* res) {
        bool ret = this->_close_file();
        res->set_value(ret ? true : false);

        std::cout << CTIME << " Req: close_file, Res: " << res->value() \
            << std::endl;

        return Status::OK;
    }

private:
    char _original_cwd[PATH_MAX];
    struct OpenedFile* _opened_file;
    struct OpenedDir*  _opened_dir;

    char* _realpath(const char* path) {
        //std::cout << "pcheck on " << path << std::endl;
        fflush(stdout);
        char buf[PATH_MAX];
        if (strlen(path) == 0) return NULL;
        else if (path[0] == '/') {
            strncpy(buf, path, strlen(path)); 
        } else {
            strncpy(buf+2, path, strlen(path));
            buf[0] = '.';
            buf[1] = '/';
        }
        char* _path = realpath(path, NULL);
        std::stack<string> comp;
        while (_path == NULL) {
            char* last_slash = strrchr(buf, '/');
            if (last_slash == NULL) break;
            if (strlen(last_slash) == 1) continue;
            string s(last_slash+1);
            comp.push(s);
            *last_slash = '\0';
            _path = realpath(buf, NULL);
        }
        //std::cout << "middle result " << _path << std::endl;
        if (_path != NULL) {
            string __path(_path);
            while (!comp.empty()) {
                __path.append("/" + comp.top());
                comp.pop();
            }
            free(_path);
            _path = (char*)malloc(sizeof(char)*(strlen(__path.c_str())+1));
            strncpy(_path, __path.c_str(), strlen(__path.c_str())+1);
            _path[strlen(__path.c_str())] = '\0';
        }
        //std::cout << "final result " << _path << std::endl;
        return _path;
    }

    bool _has_permission(const char* path) {
        char* _path = this->_realpath(path);
        if (_path == NULL) {
            return false;
        }
        if (strncmp(_path, this->_original_cwd, strlen(this->_original_cwd)) 
                == 0) {
            return true;
        }
        free(_path);

        return false;
    }

    bool _has_opened_file() {
        return (this->_opened_file != NULL);
    }

    bool _open_file(const char* path, enum RWType type) {
        if (!this->_has_permission(path) || this->_has_opened_file()) {
            return false;
        }
        char* _path = this->_realpath(path);
        int flags = 0;
        flags |= ((type == T_READ) ? O_RDONLY : O_WRONLY);
        if (type == T_WRITE) flags |= O_CREAT;

        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

        int fd = open(_path, flags, mode);
        if (fd == -1) {
            free(_path);
            return false;
        }

        this->_opened_file = (struct OpenedFile*)malloc(
                sizeof(struct OpenedFile)
                );
        this->_opened_file->fd = fd;
        this->_opened_file->type = type;
        this->_opened_file->path = _path;
        
        return true;
    }

    bool _close_file() {
        if (!this->_has_opened_file()) {
            return false;
        }
        close(this->_opened_file->fd);
        free(this->_opened_file->path);
        free(this->_opened_file);
        this->_opened_file = NULL;
        return true;
    }

    bool _has_opened_dir() {
        return (this->_opened_dir != NULL);
    }

    bool _open_dir(const char* path) {
        if (!this->_has_permission(path) || this->_has_opened_file()) {
            return false;
        }
        char* _path = this->_realpath(path);
        if (_path == NULL) return false;
        DIR* dirp;
        dirp = opendir(_path);
        if (dirp == NULL) {
            return false;
        }

        this->_opened_dir = (struct OpenedDir*)malloc(sizeof(struct OpenedDir));
        this->_opened_dir->dirp = dirp;        
        this->_opened_dir->path = _path;

        return true;
    }

    bool _close_dir() {
        if (!this->_has_opened_dir()) {
            return false;
        }
        closedir(this->_opened_dir->dirp);
        free(this->_opened_dir->path);
        free(this->_opened_dir);
        this->_opened_dir = NULL;

        return true;
    }
};


void run_server() {
  std::string server_address("0.0.0.0:50051");
  DFSImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << CTIME << " Server listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char** argv) {
    run_server();
    return 0;
}
