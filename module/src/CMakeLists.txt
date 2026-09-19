# Core C++ wrapper library: FlukeNorma::core
add_library(norma_core
    tcp_transport.cpp
    scpi_client.cpp
    instrument.cpp)
add_library(FlukeNorma::core ALIAS norma_core)

# Public headers live in module/include (one level above this directory).
target_include_directories(norma_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>)

target_compile_features(norma_core PUBLIC cxx_std_17)

set_target_properties(norma_core PROPERTIES
    OUTPUT_NAME flukenorma
    POSITION_INDEPENDENT_CODE ON) # required when linked into the Python module / C DLL

# Asio is an implementation detail (pimpl) — consumers never see its headers,
# so it is linked only in the build tree and left out of the install export.
target_link_libraries(norma_core PRIVATE $<BUILD_INTERFACE:asio::asio>)

# What consumers of the static library DO need at final link time:
find_package(Threads REQUIRED)
target_link_libraries(norma_core PUBLIC
    Threads::Threads
    $<$<PLATFORM_ID:Windows>:ws2_32>
    $<$<PLATFORM_ID:Windows>:mswsock>)

if(MSVC)
    target_compile_options(norma_core PRIVATE /W4)
else()
    target_compile_options(norma_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
