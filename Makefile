PROTO_DIR=./protos
CC="gcc-9"

all :
	echo 1

proto : ${PROTO_DIR}/dfs.proto
	protoc --grpc_out=${PROTO_DIR} --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` ${PROTO_DIR}/dfs.proto
	protoc --cpp_out=${PROTO_DIR} ${PROTO_DIR}/dfs.proto

clean :
	rm *.o &> /dev/null
