# cmake .. -DCMAKE_C_COMPILER=gcc-6 -DCMAKE_CXX_COMPILER=g++-6 -DCUDA_TOOLKIT_ROOT_DIR=/usr/local/cuda-9.0 -DAMDAPPSDKROOT=/opt/AMDAPPSDK-3.0

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

option(EQUIHASHCL "Build with OpenCL mining" ON)
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
#    add_definitions(-DCONTINUOUS_MINER=1)
endfunction()

configureProject()

find_package(Threads REQUIRED)

message("------------------------------------------------------------------------")
message("-- CMake ${CMAKE_VERSION}")
message("-- Build ${CMAKE_BUILD_TYPE} / ${CMAKE_SYSTEM_NAME}")
message("------------------------------------------------------------- components")
message("-- EQUIHASHCL       Build OpenCL components                  ${EQUIHASHCL}")
message("-- EQUIHASHCUDA     Build CUDA components                    ${EQUIHASHCUDA}")
message("-- BINKERN          Install AMD binary kernels               ${BINKERN}")
message("------------------------------------------------------------------------")
message("")

include(ProjectCompilerSettings)
if(UNIX AND NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
endif()

include_directories(equihash)

if (EQUIHASHCL)
    unset(OPENCL_LIBRARIES CACHE)
    set(OPENCL_INCLUDE_DIRS ${AMDAPPSDKROOT}/include/)
    mark_as_advanced(OPENCL_INCLUDE_DIRS)
    set(OPENCL_LIBRARIES "-L${AMDAPPSDKROOT}/lib/x86_64/sdk -lOpenCL -ldl")
    mark_as_advanced(OPENCL_LIBRARIES)

    include_directories(${OPENCL_INCLUDE_DIRS})

    add_subdirectory(equihash/cl)
#    if (BINKERN)
#        add_subdirectory(equihash/cl/kernels)
#    endif()
endif()
if (EQUIHASHCUDA)
    find_package(CUDA REQUIRED)
    include_directories(${CUDA_INCLUDE_DIRS})
    add_subdirectory(equihash/cuda)
endif ()

add_subdirectory(base)
add_subdirectory(core)
add_subdirectory(beam)

add_subdirectory(equihash)
