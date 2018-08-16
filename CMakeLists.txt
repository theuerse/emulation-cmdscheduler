cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

set(EXECUTABLE_OUTPUT_PATH "build")
add_executable(cmdScheduler src/cmdScheduler.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(Boost)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIR})
    set (Boost_USE_STATIC_LIBS OFF) # enable dynamic linking
    find_package (Boost COMPONENTS date_time system)
    target_link_libraries (cmdScheduler Boost::system Boost::date_time Threads::Threads)
endif()

install(TARGETS cmdScheduler DESTINATION bin)
