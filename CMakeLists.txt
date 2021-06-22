cmake_minimum_required(VERSION 3.5)
# set the project name
project (LIRS VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# default using release mode
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE release) # RelWithDebInfo
  message(STATUS "build type set to release")
endif()
string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE)
message(STATUS "build type: ${CMAKE_BUILD_TYPE}")



# add the binary tree to the search path for include files
# In GCC, this will invoke the "-I" command
include_directories(
  "${PROJECT_SOURCE_DIR}/src"
  "${PROJECT_SOURCE_DIR}/src/threadpool"
)

# add system local lib
link_directories(/usr/local/lib)

# For the static library:
include_directories(/usr/local/include)

# add pthread support
find_package(Threads REQUIRED)
SET(CMAKE_CXX_FLAGS -pthread)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
# add boost library
find_package(Boost 1.50 COMPONENTS system) 
include_directories(${Boost_INCLUDE_DIR} )

# all the binary will be in the build folder
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# The add_subdirectory will add CMakeLists.txt under src and tests into your project.
add_subdirectory(src)