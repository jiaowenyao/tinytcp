
project(tests)


include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)


set(LIBS
    tinytcp
)

my_add_excutable(test_send_package test_send_package.cc tinytcp "${LIBS}")
my_add_excutable(test_recv_package test_recv_package.cc tinytcp "${LIBS}")


