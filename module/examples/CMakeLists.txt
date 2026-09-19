add_executable(norma_identify cpp/identify.cpp)
target_link_libraries(norma_identify PRIVATE FlukeNorma::core)

add_executable(norma_simple_power cpp/simple_power_measurement.cpp)
target_link_libraries(norma_simple_power PRIVATE FlukeNorma::core)
