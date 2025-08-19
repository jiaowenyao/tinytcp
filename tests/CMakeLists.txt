
project(tests)


include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)


set(LIBS
    tinytcp
    gtest
    pthread
)

my_add_excutable(test_send_package test_send_package.cc tinytcp "${LIBS}")
my_add_excutable(test_recv_package test_recv_package.cc tinytcp "${LIBS}")
my_add_excutable(test_lock_free_ring_queue test_lock_free_ring_queue.cc tinytcp "${LIBS}")
my_add_excutable(test_net_start test_net_start.cc tinytcp "${LIBS}")
my_add_excutable(test_network test_network.cc tinytcp "${LIBS}")


