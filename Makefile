proto : dfs.proto
	protoc --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` dfs.proto
	protoc --cpp_out=. dfs.proto
