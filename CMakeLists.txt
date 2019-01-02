cmake_minimum_required(VERSION 3.1)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_REVISION 0)

project(beam-cuda-miner VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION})
set(TARGET_NAME beam-cuda-miner)

message("BEAM_VERSION: ${PROJECT_VERSION}")

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, defaulting to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif ()

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

if(MSVC)
    include_directories($ENV{OPENSSL_ROOT_DIR}/include)

    set(OPENSSL_LIBCRYPTO $ENV{OPENSSL_ROOT_DIR}/lib/VC/static/libcrypto64MT.lib crypt32.lib)
    set(OPENSSL_LIBSSL $ENV{OPENSSL_ROOT_DIR}/lib/VC/static/libssl64MT.lib)

    if(MSVC_VERSION GREATER_EQUAL 1900)
        set(OPENSSL_LIBCRYPTO ${OPENSSL_LIBCRYPTO} legacy_stdio_definitions.lib)
    endif()

    # use static runtime
    foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
        flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)

        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif(${flag_var} MATCHES "/MD")
    endforeach(flag_var)
else()
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
    if(MSVC)
        find_package(CUDA 10.0 REQUIRED)
    else()
        find_package(CUDA REQUIRED)
    endif()
    include_directories(${CUDA_INCLUDE_DIRS})
    add_subdirectory(equihash/cuda)
endif ()

add_subdirectory(beam)
add_subdirectory(base)
add_subdirectory(core)

add_subdirectory(equihash)
