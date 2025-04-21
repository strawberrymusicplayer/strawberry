set(DISCORD_RPC_SOURCES
  discord_rpc.h
  discord_register.h
  discord_rpc.cpp
  discord_rpc_connection.h
  discord_rpc_connection.cpp
  discord_serialization.h
  discord_serialization.cpp
  discord_connection.h
  discord_backoff.h
  discord_msg_queue.h
)

if(UNIX)
  list(APPEND DISCORD_RPC_SOURCES discord_connection_unix.cpp)
  if(APPLE)
    list(APPEND DISCORD_RPC_SOURCES discord_register_osx.m)
    add_definitions(-DDISCORD_OSX)
  else()
    list(APPEND DISCORD_RPC_SOURCES discord_register_linux.cpp)
    add_definitions(-DDISCORD_LINUX)
  endif()
endif()

if(WIN32)
  list(APPEND DISCORD_RPC_SOURCES discord_connection_win.cpp discord_register_win.cpp)
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
target_include_directories(discord-rpc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
