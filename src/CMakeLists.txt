project(src)

include_directories(
    ${CMAKE_SOURCE_DIR}
)

set(LIB_SRC
    ${CMAKE_SOURCE_DIR}/plat/sys_plat.cc
    # tcp.cc
)

add_library(tinytcp SHARED ${LIB_SRC})
target_link_libraries(tinytcp pcap pthread)

