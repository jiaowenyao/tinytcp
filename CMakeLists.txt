cmake_minimum_required(VERSION 3.16)
project(tinytcp)

# debug
set(CMAKE_BUILD_TYPE Debug)
add_compile_options(-g3)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_MODULE_PATH}/cmake)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# plat
add_definitions(-DSYS_PLAT_LINUX)
# debug
add_definitions(-DDEBUG_INETIF)

include_directories(
    ${PROJECT_SOURCE_DIR}
)

add_subdirectory(src)
add_subdirectory(tests)
# add_subdirectory(app)




