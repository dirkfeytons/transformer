cmake_minimum_required(VERSION 2.8)

project(transformerlibs C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

find_package(LIBLUA REQUIRED)
find_package(Threads REQUIRED)

add_definitions(-Wall -Wextra -Werror -std=gnu99)

# in debug builds (CMAKE_BUILD_TYPE=Debug) set a define that enables extra traces
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DENABLE_DEBUG")

include_directories(lib/api/ ${LIBLUA_INCLUDE_DIR})

# lasync
set(ASYNC_SOURCES )
set(SOURCES
  tch_async.c
  async.c
  execute.c
)
foreach(src ${SOURCES})
  set(ASYNC_SOURCES ${ASYNC_SOURCES} lib/src/tch_async/${src})
endforeach(src)
add_library(lasync MODULE ${ASYNC_SOURCES})
target_link_libraries(lasync ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(lasync PROPERTIES PREFIX "")
set_source_files_properties(${ASYNC_SOURCES}
  PROPERTIES COMPILE_FLAGS "-fvisibility=hidden")
install(TARGETS lasync
        LIBRARY DESTINATION lib/lua)

# syslog
add_library(syslog MODULE lib/src/syslog/lsyslog.c)
set_target_properties(syslog PROPERTIES  PREFIX "")
install(TARGETS syslog
        LIBRARY DESTINATION lib/lua)

# libtransformer       
add_library(transformer SHARED lib/src/transformer/libtransformer.c)
set_target_properties(transformer PROPERTIES
  LINK_FLAGS_TEST --coverage
)
install(TARGETS transformer
        LIBRARY DESTINATION lib)
install(DIRECTORY lib/api/ DESTINATION include/)

# libtransformer examples
# we build these just to make sure they compile properly
add_executable(libtransformer_example1 doc/libtransformer_example1.c)
target_link_libraries(libtransformer_example1 transformer)

add_executable(libtransformer_example2 doc/libtransformer_example2.c)
target_link_libraries(libtransformer_example2 transformer)

# install the Transformer code
install(DIRECTORY transformer DESTINATION lib/lua)
