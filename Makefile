PROTOS_PATH = ./protos

HOST_SYSTEM = $(shell uname | cut -f 1 -d_)
SYSTEM ?= $(HOST_SYSTEM)
CXX = g++
CPPFLAGS += `pkg-config --cflags protobuf grpc`
CXXFLAGS += -std=c++11
LDFLAGS += -I.
ifeq ($(SYSTEM),Darwin)
	LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf grpc++ grpc`\
			   -pthread\
			   -lgrpc++_reflection\
			   -ldl
else
	LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf grpc++ grpc`\
			   -pthread\
			   -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed\
			   -ldl
endif

PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

all : proto dfs_server dfs_client 

dfs_server : ${PROTOS_PATH}/dfs.pb.cc ${PROTOS_PATH}/dfs.grpc.pb.cc dfs_server.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ 

dfs_client : ${PROTOS_PATH}/dfs.pb.cc ${PROTOS_PATH}/dfs.grpc.pb.cc dfs_client.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

proto : ${PROTOS_PATH}/dfs.proto
	protoc --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` ${PROTOS_PATH}/dfs.proto
	protoc --cpp_out=. ${PROTOS_PATH}/dfs.proto

clean :
	-@rm *.o a.out dfs_server dfs_client ${PROTOS_PATH}/*.pb.* 2>/dev/null || true
