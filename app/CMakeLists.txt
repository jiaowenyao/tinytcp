project(app)


include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)


set(LIBS
    tinytcp
    gtest
    pthread
    sysapi
)


set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin/app)
my_add_excutable(myping ping/ping.cc tinytcp "${LIBS}")
my_add_excutable(sysapiping ping/myping.cc tinytcp "${LIBS}")

my_add_excutable(udp_echo_client echo/udp_echo_client.cc tinytcp "${LIBS}")
my_add_excutable(udp_echo_server echo/udp_echo_server.cc tinytcp "${LIBS}")

my_add_excutable(tcp_echo_client echo/tcp_echo_client.cc tinytcp "${LIBS}")
my_add_excutable(tcp_echo_server echo/tcp_echo_server.cc tinytcp "${LIBS}")

my_add_excutable(download test/download.cc tinytcp "${LIBS}")



