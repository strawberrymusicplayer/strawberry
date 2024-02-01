INCLUDE(FindProtobuf)
FIND_PACKAGE(Protobuf REQUIRED)
protobuf_generate_cpp(PROTO_SRC PROTO_HEADER RemoteMessages.proto)
ADD_LIBRARY(proto ${PROTO_HEADER} ${PROTO_SRC})

target_include_directories(proto PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

