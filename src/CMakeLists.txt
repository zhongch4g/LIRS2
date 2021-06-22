# add static library
set(LIRS_SRCS
    "replace_lirs_base.cc"
    "replace_lirs_base.h"
    "replace_lirs2.cc"
    "replace_lirs2.h"
    "replace_arc.cc"
    "replace_arc.h"
    "replace_lru.cc"
    "replace_lru.h"
    "replace_opt.cc"
    "replace_opt.h"
    "replace.cc"
    "replace.h"
    "trace_handle.cc"
    "trace_handle.h"
)
# Declare the library
add_library(llirs STATIC
    ${LIRS_SRCS}
)

# Specify here the include directories exported by this library
target_include_directories(llirs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# all the binary will be in the build folder
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# add the executable
add_executable(replace main.cpp)
target_link_libraries(replace ${GTEST_LIBRARY} ${PROJECT_LINK_LIBS} gflags llirs boost_thread boost_system)
