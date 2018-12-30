set(SOURCES
    Device.cpp
    Device.hpp
    HardwareMetrics.cpp
    HardwareMetrics.hpp
    Miner.cpp
    Miner.hpp
    NullWorker.cpp
    NullWorker.hpp
    SecureTcpTransport.cpp
    SecureTcpTransport.hpp
    Solution.cpp
    Solution.hpp
    Statistics.cpp
    Statistics.hpp
    StratumWorker.cpp
    StratumWorker.hpp
    TcpAsyncTransport.cpp
    TcpAsyncTransport.hpp
    Transport.cpp
    Transport.hpp
    Work.cpp
    Worker.cpp
    Worker.hpp
    Work.hpp
    Farm.cpp
    Farm.hpp
    Service.cpp
    Service.hpp
)

if (EQUIHASHCL)
    set(SOURCES
        ${SOURCES}
        ClMiner.cpp
        ClMiner.hpp
    )
endif()

if (EQUIHASHCUDA)
    set(SOURCES
        ${SOURCES}
        CudaMiner.cpp
        CudaMiner.hpp
    )
endif()

add_library(core STATIC ${SOURCES})
target_link_libraries(core PRIVATE base)
target_include_directories(core PRIVATE ..)

