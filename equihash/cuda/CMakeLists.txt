find_package(CUDA REQUIRED)

set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS};--ptxas-options=-v;-use_fast_math;-maxrregcount=100)
list(APPEND CUDA_NVCC_FLAGS "--disable-warnings")

list(APPEND CUDA_NVCC_FLAGS_RELEASE -O3)
list(APPEND CUDA_NVCC_FLAGS_DEBUG -G)

if(COMPUTE AND (COMPUTE GREATER 0))
    list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_${COMPUTE},code=sm_${COMPUTE}")
else()
    set(
        CUDA_NVCC_FLAGS
        ${CUDA_NVCC_FLAGS}
#        "-gencode arch=compute_30,code=sm_30"
#        "-gencode arch=compute_35,code=sm_35"
#        "-gencode arch=compute_50,code=sm_50"
#        "-gencode arch=compute_52,code=sm_52"
#        "-gencode arch=compute_53,code=sm_53"
        "-gencode arch=compute_60,code=sm_60"
#        "-gencode arch=compute_61,code=sm_61"
#        "-gencode arch=compute_62,code=sm_62"
#        "-gencode arch=compute_70,code=sm_70"
    )
endif()

message("------------------------------------------------------------------------")
message("-- CUDA nvcc flags ${CUDA_NVCC_FLAGS}")
message("-- CUDA host compiler ${CUDA_HOST_COMPILER}")
message("-- Compute ${COMPUTE}")
message("-- CUDA include dirs ${CUDA_INCLUDE_DIRS}")
message("------------------------------------------------------------------------")

set(SOURCES
    blake2b.cuh
    CudaSolver.cu
    CudaSolver-150-5.cuh
    CudaSolver.hpp
    EquihashCudaMiner.cpp
    EquihashCudaMiner.hpp
)

cuda_add_library(equihash-cuda STATIC ${SOURCES})
target_include_directories(equihash-cuda PUBLIC ${CUDA_INCLUDE_DIRS})
target_include_directories(equihash-cuda PRIVATE .. ../.. ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(equihash-cuda ${CUDA_LIBRARIES})
