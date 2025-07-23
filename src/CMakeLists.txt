project(src)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

set(LIB_SRC
    ${CMAKE_SOURCE_DIR}/plat/sys_plat.cc
    config.cc
    log.cc
    mutex.cc
    thread.cc
    util.cc
)

add_library(tinytcp SHARED ${LIB_SRC})
target_link_libraries(tinytcp pcap pthread yaml-cpp)

