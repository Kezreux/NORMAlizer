add_executable(norma_tests
    test_scpi_client.cpp
    test_instrument.cpp)

target_link_libraries(norma_tests PRIVATE
    FlukeNorma::core
    Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(norma_tests)

# ---------------------------------------------------------------------------
# Hardware-in-the-loop tests: real TcpTransport against a real Fluke NORMA.
# They SKIP themselves (ctest shows "Skipped") unless NORMA_HOST is set, so a
# plain `ctest` run needs no instrument. RUN_SERIAL because the instrument
# accepts only one TCP client at a time.
#
#   NORMA_HOST=<ip> ctest -L hardware --output-on-failure
# ---------------------------------------------------------------------------
add_executable(norma_hardware_tests
    test_hardware_norma.cpp)

target_link_libraries(norma_hardware_tests PRIVATE
    FlukeNorma::core
    Catch2::Catch2WithMain)

catch_discover_tests(norma_hardware_tests
    SKIP_RETURN_CODE 4
    PROPERTIES
        LABELS "hardware"
        RUN_SERIAL TRUE)
