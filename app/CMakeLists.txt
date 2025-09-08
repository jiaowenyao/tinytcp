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

