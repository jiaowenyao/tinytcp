project(src)

include_directories(
    ${CMAKE_SOURCE_DIR}
)

set(LIB_SRC
    tcp.cc
)

add_library(tinytcp SHARED ${LIB_SRC})
# target_link_libraries(tinytcp )

