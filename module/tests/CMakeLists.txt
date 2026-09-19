add_executable(norma_tests
    test_scpi_client.cpp
    test_instrument.cpp)

target_link_libraries(norma_tests PRIVATE
    FlukeNorma::core
    Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(norma_tests)
