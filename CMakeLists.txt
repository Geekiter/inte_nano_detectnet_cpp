# require CMake 2.8 or greater
cmake_minimum_required(VERSION 3.16)

# declare my-recognition project
project(detectnet-socket)
#SET(CMAKE_BUILD_TYPE "Debug")


# import jetson-inference and jetson-utils packages.
# note that if you didn't do "sudo make install"
# while building jetson-inference, this will error.
find_package(jetson-utils)
find_package(jetson-inference)
file(GLOB detectnetSources *.cpp)
file(GLOB detectnetIncludes *.h )
# CUDA is required
find_package(CUDA)

# add directory for libnvbuf-utils to program
link_directories(/usr/lib/aarch64-linux-gnu/tegra)

# compile the my-recognition program
#cuda_add_executable(socket-example socket_test.cpp
#        cJSON.cpp
#        cJSON.h)

cuda_add_executable(detectnet-socket detectnet.cpp
        cJSON.cpp
        cJSON.h
        ${detectnetSources}
        detectnet.cpp
)

# link my-recognition to jetson-inference library
target_link_libraries(detectnet-socket jetson-inference)

#target_link_libraries(detectnet-socket tcmalloc)


install(TARGETS detectnet-socket DESTINATION bin)

# 找到tcMalloc库
#find_library(TCMALLOC_LIBRARY NAMES tcmalloc_minimal PATHS /usr/lib /usr/local/lib)
#
#if(TCMALLOC_LIBRARY)
#    message(STATUS "tcMalloc library found at ${TCMALLOC_LIBRARY}")
#    # 链接tcMalloc库
#    target_link_libraries(detectnet-socket ${TCMALLOC_LIBRARY})
#else()
#    message(FATAL_ERROR "tcMalloc library not found")
#endif()