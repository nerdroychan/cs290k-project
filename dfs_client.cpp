#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

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
            std::cout << "getdir failed due to get_dir failure" << std::endl;
            return;
        }
        std::cout << "getdir succeed, returned " << res.content() << std::endl;
    }

    void cd(const string path) {
        ClientContext context;
        Str req;
        Bool res;
        req.set_content(path);
        Status status = this->_stub->change_dir(&context, req, &res);
        if (!status.ok() || res.value() == false) {
            std::cout << "changedir failed due to change_dir failure" \
                << std::endl;
            return;
        }
        std::cout << "change_dir succeed, returned " << res.value() \
                << std::endl;
    }

    void filecount() {
        ClientContext context;
        Void req;
        Int res;
        Status status = this->_stub->file_count(&context, req, &res);
        if (!status.ok() || res.value() == -1) {
            std::cout << "filecount failed due to file_count failure" \
                << std::endl;
            return;
        } 
        std::cout << "file_count succeed, returned " << res.value() \
            << std::endl;
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
            char buf[80];
            time_t tt = it->last_modified_time();
            struct tm  ts;
            ts = *localtime(&tt);
            strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);

            if (list) {
                std::cout << it->is_directory() << " " << it->size() << " " \
                    << buf << " "; 
            }
            std::cout << std::endl;
        }
    } 

    void ls(bool list) {
        ClientContext context;
        Void req;
        Str res;

        Status status = this->_stub->get_dir(&context, req, &res);
        if (!status.ok()) {
            std::cout << "ls failed due to get_dir failure" <<std::endl;
            return;
        }
        ls(list, res.content());
    }

    void ls(const string path) {
        this->ls(false, path);
    }

    void ls() {
        this->ls(false);
    }

    void put(const string localpath, const string remotepath) {
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        int fd = open(localpath.c_str(), O_RDONLY, mode);
        if (fd == -1) {
            std::cout << "put failed because cannot open local file" \
                << std::endl;
            return;
        }

        // invoke open_file_to_write
        ClientContext open_file_to_write_context;
        Str open_file_to_write_req;
        Bool open_file_to_write_res;

        open_file_to_write_req.set_content(remotepath);
        Status open_file_to_write_status = this->_stub->open_file_to_write(
                &open_file_to_write_context, open_file_to_write_req, \
                &open_file_to_write_res
                );
        if (!open_file_to_write_status.ok() \
                || !open_file_to_write_res.value()) {
            std::cout << "put failed due to open_file_to_write failure" \
                << std::endl;
            close(fd);
            return;
        }

        // invoke next_write
        WriteRequest next_write_req;
        Bool next_write_res;
        char buf[512];
        int bytes_read = 0;
        while (bytes_read >= 0) {
            bytes_read = read(fd, buf, 512);
            if (bytes_read == -1) {
                std::cout << "put failed because cannot read local file" \
                    << std::endl;
                close(fd);
                return;
            } else if (bytes_read == 0) break;
            next_write_req.mutable_block()->set_content(buf);
            next_write_req.set_size(bytes_read);
            ClientContext next_write_context;

            Status next_write_status = this->_stub->next_write(
                    &next_write_context, next_write_req, &next_write_res
                    );
            if (!next_write_status.ok() || !next_write_res.value()) {
                std::cout << "put failed due to next_write failure" \
                    << std::endl;
                close(fd);
                return;
            }
        }

        // invoke close_file
        ClientContext close_file_context;
        Void close_file_req;
        Bool close_file_res;

        Status close_file_status = this->_stub->close_file(
                &close_file_context, close_file_req, &close_file_res
                );
        if (!close_file_status.ok() || !close_file_res.value()) {
            std::cout << "put failed due to close_file failure" << std::endl;
            close(fd);
            return;
        }

        std::cout << "put succeed" << std::endl;
        close(fd);
    }

    void put(const string localpath) {
        string remotepath(localpath);
        put(localpath, remotepath);
    }

    void get(const string remotepath, const string localpath) {
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        int fd = open(localpath.c_str(), O_CREAT | O_WRONLY, mode);
        if (fd == -1) {
            std::cout << localpath.c_str() << " ";
            std::cout << "get failed because cannot open local file" \
                << std::endl;
            return;
        }

        // invoke open_file_to_read
        ClientContext open_file_to_read_context;
        Str open_file_to_read_req;
        Bool open_file_to_read_res;

        open_file_to_read_req.set_content(remotepath);
        Status open_file_to_read_status = this->_stub->open_file_to_read(
                &open_file_to_read_context, open_file_to_read_req, \
                &open_file_to_read_res
                );
        if (!open_file_to_read_status.ok() \
                || !open_file_to_read_res.value()) {
            std::cout << "get failed due to open_file_to_read failure" \
                << std::endl;
            close(fd);
            return;
        }

        // invoke next_read
        Void next_read_req;
        ReadResponse next_read_res;

        int bytes_read = 0;
        while (bytes_read >= 0) {
            ClientContext next_read_context;
            Status next_read_status = this->_stub->next_read(
                    &next_read_context, next_read_req, &next_read_res
                    );
            if (!next_read_status.ok() || next_read_res.size() == -1) {
                std::cout << "get failed due to next_read failure" << std::endl;
                close(fd);
                return;
            }
            bytes_read = next_read_res.size();
            if (bytes_read == 0) break;
            int ret = write(fd, next_read_res.block().content().c_str(), \
                   bytes_read);
            if (ret == -1) {
                std::cout << "get failed because cannot write local file" \
                    << std::endl;
                close(fd);
                return;
            }
        }

        // invoke close_file
        ClientContext close_file_context;
        Void close_file_req;
        Bool close_file_res;

        Status close_file_status = this->_stub->close_file(
                &close_file_context, close_file_req, &close_file_res
                );
        if (!close_file_status.ok() || !close_file_res.value()) {
            std::cout << "get failed due to close_file failure" << std::endl;
            close(fd);
            return;
        }
        
        std::cout << "get succeed" << std::endl;
        close(fd);
    }

    void get(const string remotepath) {
        string localpath(remotepath);
        get(remotepath, localpath);
    }

    void randomread(const string remotepath, int firstbyte, int numbytes) {
        if (numbytes > 512) {
            std::cout << "randomread failed because size over 512" << std::endl;
            return;
        }
        // invoke open_file_to_read
        ClientContext open_file_to_read_context;
        Str open_file_to_read_req;
        Bool open_file_to_read_res;

        open_file_to_read_req.set_content(remotepath);
        Status open_file_to_read_status = this->_stub->open_file_to_read(
                &open_file_to_read_context, open_file_to_read_req, \
                &open_file_to_read_res
                );
        if (!open_file_to_read_status.ok() \
                || !open_file_to_read_res.value()) {
            std::cout << "randomread failed due to open_file_to_read failure" \
                << std::endl;
            return;
        }

        // invoke random_read
        ClientContext random_read_context;
        RandomReadRequest random_read_req;
        ReadResponse random_read_res;

        random_read_req.set_offset(firstbyte);
        random_read_req.set_size(numbytes);

        Status random_read_status = this->_stub->random_read(
                &random_read_context, random_read_req, &random_read_res);

        if (!random_read_status.ok() || random_read_res.size() == -1) {
            std::cout << "randomread failed due to random_read failure" \
                << std::endl;
            return;
        }

        // invoke close_file
        ClientContext close_file_context;
        Void close_file_req;
        Bool close_file_res;

        Status close_file_status = this->_stub->close_file(
                &close_file_context, close_file_req, &close_file_res
                );
        if (!close_file_status.ok() || !close_file_res.value()) {
            std::cout << "randomread failed due to close_file failure" << std::endl;
            return;
        }

        char buf[513];
        strncpy(buf, random_read_res.block().content().c_str(), \
                random_read_res.size());
        buf[random_read_res.size()+1] = '\0';
        std::cout << buf << std::endl;
        
        std::cout << "randomread succeed" << std::endl;
    }

    void shell() {
        string command;
        while (true) {
            std::cout << "$ ";
            fflush(stdout);
            if (getline(std::cin, command)) {
                this->_dispatch(command);
                fflush(stdout);
            } else break;
        }
        std::cout << std::endl;
        fflush(stdout);
    }

private:
    std::unique_ptr<dfs::DFS::Stub> _stub;

    void _dispatch(string& command) {
        std::vector<string> cmdbuf;
        char* cmd;
        char buf[4096];
        strcpy(buf, command.c_str());
        cmd = strtok(buf, " ");
        while (cmd != NULL) {
            string s(cmd);
            cmdbuf.push_back(s);
            cmd = strtok(NULL, " ");
        }

        if (cmdbuf.size() == 0) return;

        if (cmdbuf[0].compare("getdir") == 0) {
            this->getdir();
        } else if (cmdbuf[0].compare("filecount") == 0) {
            this->filecount();
        } else if (cmdbuf[0].compare("cd") == 0) {
            if (cmdbuf.size() == 1) {
                std::cout << "Usage: cd dir" << std::endl;
            }
            this->cd(cmdbuf[1]);
        } else if (cmdbuf[0].compare("ls") == 0) {
            if (cmdbuf.size() == 1) {
                this->ls();
            } else if (cmdbuf.size() == 2) {
                if (cmdbuf[1].compare("-l") == 0) {
                    this->ls(true);
                } else {
                    this->ls(cmdbuf[1]);
                }
            } else if (cmdbuf.size() == 3){
                if (cmdbuf[1].compare("-l") == 0) {
                    this->ls(true, cmdbuf[2]);
                } else if (cmdbuf[2].compare("-l") == 0) {
                    this->ls(true, cmdbuf[1]);
                } else {
                    std::cout << "Usage: ls [-l] [directory_name]" << std::endl;
                }
            } else {
                std::cout << "Usage: ls [-l] [dir]" << std::endl;
            }
        } else if (cmdbuf[0].compare("put") == 0) {
            if (cmdbuf.size() == 2) {
                this->put(cmdbuf[1]);
            } else if (cmdbuf.size() == 3) {
                this->put(cmdbuf[1], cmdbuf[2]);
            } else {
                std::cout << "Usage: put localfile [remotefile]" << std::endl;
            }
        } else if (cmdbuf[0].compare("get") == 0) {
            if (cmdbuf.size() == 2) {
                this->get(cmdbuf[1]);
            } else if (cmdbuf.size() == 3) {
                this->get(cmdbuf[1], cmdbuf[2]);
            } else {
                std::cout << "Usage: get remotefile [localfile]" << std::endl;
            }
        } else if (cmdbuf[0].compare("randomread") == 0) {
            if (cmdbuf.size() == 4) {
                this->randomread(cmdbuf[1], std::stoi(cmdbuf[2]), \
                        std::stoi(cmdbuf[3]));
            } else {
                std::cout << "Usage: randomread remotefile firstbyte numbytes" \
                    << std::endl;
            }
        } else {
            std::cout << "dfs: command not found: " << cmdbuf[0] << std::endl;
        }
    }
};




int main(int argc, char** argv) {
    DFSClient client(
            grpc::CreateChannel("localhost:50051",
            grpc::InsecureChannelCredentials())
            );

    client.shell();

    return 0;
}
