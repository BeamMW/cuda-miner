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

add_executable(beam-equihash ${SOURCES})

target_include_directories(beam-equihash PRIVATE .. ../beam)
target_link_libraries(beam-equihash PRIVATE base core beam ${OPENSSL_LIBSSL} ${OPENSSL_LIBCRYPTO})

if (EQUIHASHCL)
    target_link_libraries(beam-equihash PRIVATE equihash-cl)
endif()

if (EQUIHASHCUDA)
    if(MSVC)
        target_link_libraries(beam-equihash PRIVATE equihash-cuda)
    else()
        target_link_libraries(beam-equihash PRIVATE equihash-cuda -L${CUDA_LIB_PATH} -lcuda)
    endif()
endif()
