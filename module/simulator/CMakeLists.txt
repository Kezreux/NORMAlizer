# Offline Fluke NORMA: FlukeNorma::simulator
#
# A stateful SCPI instrument model plus two ways to reach it: an in-process
# Transport (fast, deterministic, fault-injecting) and a loopback TCP server
# (which is what gives TcpTransport real coverage). norma_sim serves it as a
# standalone process so an application can be developed without an instrument.
add_library(norma_simulator STATIC
    src/simulator_support.cpp
    src/norma_simulator.cpp
    src/simulator_handlers.cpp
    src/simulator_calculate.cpp
    src/simulator_measurement.cpp
    src/simulator_server.cpp)
add_library(FlukeNorma::simulator ALIAS norma_simulator)

target_include_directories(norma_simulator PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

# The instrument model only needs the library's types; the server needs Asio,
# which stays an implementation detail behind the pimpl.
target_link_libraries(norma_simulator
    PUBLIC FlukeNorma::core
    PRIVATE $<BUILD_INTERFACE:asio::asio>)

set_target_properties(norma_simulator PROPERTIES
    OUTPUT_NAME flukenorma_simulator
    POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(norma_simulator PRIVATE /W4)
else()
    target_compile_options(norma_simulator PRIVATE -Wall -Wextra -Wpedantic)
endif()

# The standalone simulator process.
add_executable(norma_sim src/main.cpp)
target_link_libraries(norma_sim PRIVATE FlukeNorma::simulator)
