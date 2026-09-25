add_executable(norma_tests
    test_scpi_client.cpp
    test_scpi_client_protocol.cpp
    test_instrument.cpp
    test_command_conformance.cpp
    test_simulator.cpp
    test_integration_sim.cpp
    test_integration_tcp.cpp)

target_link_libraries(norma_tests PRIVATE
    FlukeNorma::core
    FlukeNorma::simulator
    Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(norma_tests)

# ---------------------------------------------------------------------------
# Instrument-level tests: the real TcpTransport against a real socket.
#
# One source file, two executables, because the same assertions are worth running
# against two endpoints (selected by NORMA_HOST):
#
#   norma_simulated_tests   NORMA_HOST=simulator — the in-process simulator on
#                           loopback. Runs on every `ctest`, so if the simulator
#                           drifts away from what a NORMA does, the suite that
#                           validates real hardware fails here first.
#   norma_hardware_tests    NORMA_HOST=<ip> — a real NORMA. Labelled "hardware"
#                           and SKIPped when NORMA_HOST is unset, so a plain
#                           `ctest` run needs no instrument. RUN_SERIAL, because
#                           the instrument accepts only one TCP client at a time.
#
#   NORMA_HOST=192.168.1.100 ctest -L hardware --output-on-failure
#
# (Two targets rather than two catch_discover_tests() calls on one: Catch2 writes
# a single discovery file per target, so registering a target twice would make the
# second call replace the first.)
# ---------------------------------------------------------------------------
add_executable(norma_hardware_tests test_hardware_norma.cpp)
add_executable(norma_simulated_tests test_hardware_norma.cpp)

foreach(target IN ITEMS norma_hardware_tests norma_simulated_tests)
    target_link_libraries(${target} PRIVATE
        FlukeNorma::core
        FlukeNorma::simulator
        Catch2::Catch2WithMain)
endforeach()

catch_discover_tests(norma_hardware_tests
    SKIP_RETURN_CODE 4
    PROPERTIES
        LABELS "hardware"
        RUN_SERIAL TRUE)

catch_discover_tests(norma_simulated_tests
    TEST_PREFIX "sim:"
    PROPERTIES
        LABELS "simulator"
        ENVIRONMENT "NORMA_HOST=simulator")

if(MSVC)
    set(norma_test_warnings /W4)
else()
    set(norma_test_warnings -Wall -Wextra -Wpedantic)
endif()
foreach(target IN ITEMS norma_tests norma_hardware_tests norma_simulated_tests)
    target_compile_options(${target} PRIVATE ${norma_test_warnings})
endforeach()
