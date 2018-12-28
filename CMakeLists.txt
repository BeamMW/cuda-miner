# cmake .. -DCMAKE_C_COMPILER=gcc-6 -DCMAKE_CXX_COMPILER=g++-6

cmake_minimum_required(VERSION 3.5)

project(beam-equihash)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, defaulting to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif ()

set(module_dir ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
if(EXISTS ${module_dir})
    list(APPEND CMAKE_MODULE_PATH ${module_dir})
endif()

option(EQUIHASHCL "Build with OpenCL mining" OFF)
option(EQUIHASHCUDA "Build with CUDA mining" ON)
option(BINKERN "Install AMD binary kernels" OFF)

# propagates CMake configuration options to the compiler
function(configureProject)
    if (EQUIHASHCL)
        add_definitions(-DUSE_OPENCL=1)
    endif()
    if (EQUIHASHCUDA)
        add_definitions(-DUSE_CUDA=1)
    endif()
    if (BINKERN)
        add_definitions(-DBIN_KERN=1)
    endif()
endfunction()

configureProject()

find_package(Threads REQUIRED)

find_package(OpenSSL REQUIRED)

if( OpenSSL_FOUND )
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    link_directories(${OPENSSL_LIBRARIES})
    include_directories(${OPENSSL_INCLUDE_DIR})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")

    # cmake can't find static lib on macos, harcoded
    set(OPENSSL_LIBCRYPTO /usr/lib/x86_64-linux-gnu/libcrypto.a)
    set(OPENSSL_LIBSSL /usr/lib/x86_64-linux-gnu/libssl.a)
else()
    message(FATAL_ERROR "error, OpenSSL not found, please install: 'apt-get install libssl-dev' on Ubuntu")
endif()

message("------------------------------------------------------------------------")
message("-- CMake ${CMAKE_VERSION}")
message("-- Build ${CMAKE_BUILD_TYPE} / ${CMAKE_SYSTEM_NAME}")
message("------------------------------------------------------------- components")
message("-- EQUIHASHCL       Build OpenCL components                  ${EQUIHASHCL}")
message("-- EQUIHASHCUDA     Build CUDA components                    ${EQUIHASHCUDA}")
message("-- BINKERN          Install AMD binary kernels               ${BINKERN}")
message("------------------------------------------------------------------------")
message("")

if(UNIX AND NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
endif()

include_directories(equihash)

if (EQUIHASHCUDA)
    find_package(CUDA REQUIRED)
    include_directories(${CUDA_INCLUDE_DIRS})
    add_subdirectory(equihash/cuda)
endif ()

add_subdirectory(beam)
add_subdirectory(base)
add_subdirectory(core)

add_subdirectory(equihash)
