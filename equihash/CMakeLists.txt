set(SOURCES
    Api.cpp
    Api.hpp
    BeamSolution.cpp
    BeamSolution.hpp
    BeamWork.cpp
    BeamWork.hpp
    equihash-miner.cpp
    EquihashStratumWorker.cpp
    EquihashStratumWorker.hpp
    pch.cpp
    pch.hpp
    Solver.hpp
    EquihashFarm.cpp
    EquihashFarm.hpp
    blake2/blake2bx.cpp
)

set(CUDA_LIB_PATH ${CUDA_TOOLKIT_ROOT_DIR}/lib64/stubs/)

add_executable(${TARGET_NAME} ${SOURCES})

target_include_directories(${TARGET_NAME} PRIVATE .. ../beam)
target_link_libraries(${TARGET_NAME} PRIVATE base core beam ${OPENSSL_LIBSSL} ${OPENSSL_LIBCRYPTO})

if (EQUIHASHCUDA)
    if(MSVC)
        target_link_libraries(${TARGET_NAME} PRIVATE equihash-cuda)
    else()
        target_link_libraries(${TARGET_NAME} PRIVATE equihash-cuda -L${CUDA_LIB_PATH} -lcuda)
    endif()
endif()
