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
    timer.cc
    net/protocol_stack.cc
    net/net.cc
    net/memblock.cc
    net/network.cc
    net/pktbuf.cc
    net/ipaddr.cc
    net/netif.cc
    net/link_layer.cc
    net/arp.cc
    net/ip.cc
    net/icmp.cc
    net/sock.cc
    net/raw.cc
)

set(API_SRC
    api/net_api.cc
)

add_library(tinytcp SHARED ${LIB_SRC})
add_library(sysapi  SHARED ${API_SRC})

target_link_libraries(tinytcp pcap pthread yaml-cpp)

