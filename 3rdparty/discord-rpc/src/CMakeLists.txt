set(DISCORD_RPC_SOURCES
  ../include/discord_rpc.h
  ../include/discord_register.h
  discord_rpc.cpp
  rpc_connection.h
  rpc_connection.cpp
  serialization.h
  serialization.cpp
  connection.h
  backoff.h
  msg_queue.h
)

if(UNIX)
  list(APPEND DISCORD_RPC_SOURCES connection_unix.cpp)
  if(APPLE)
    list(APPEND DISCORD_RPC_SOURCES discord_register_osx.m)
    add_definitions(-DDISCORD_OSX)
  else()
    list(APPEND DISCORD_RPC_SOURCES discord_register_linux.cpp)
    add_definitions(-DDISCORD_LINUX)
  endif()
endif()

if(WIN32)
  list(APPEND DISCORD_RPC_SOURCES connection_win.cpp discord_register_win.cpp)
  add_definitions(-DDISCORD_WINDOWS)
endif()

add_library(discord-rpc STATIC ${DISCORD_RPC_SOURCES})

if(APPLE)
  target_link_libraries(discord-rpc PRIVATE "-framework AppKit")
endif()

if(WIN32)
  target_link_libraries(discord-rpc PRIVATE psapi advapi32)
endif()

target_include_directories(discord-rpc SYSTEM PRIVATE ${RapidJSON_INCLUDE_DIRS})
target_include_directories(discord-rpc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
