cmake_minimum_required(VERSION 3.24)

project(FlukeNorma
    VERSION 0.1.0
    DESCRIPTION "C++ wrapper for the Fluke NORMA 4000/5000 TCP/SCPI remote-control API"
    HOMEPAGE_URL "https://github.com/example/fluke-norma"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(NORMA_BUILD_PYTHON   "Build the pybind11 Python bindings"          ON)
option(NORMA_BUILD_C_API    "Build the C ABI shared library (for FFI)"    ON)
option(NORMA_BUILD_EXAMPLES "Build the C++ examples"                      ON)
option(NORMA_BUILD_TESTS    "Build the unit tests"                        ON)
option(NORMA_BUILD_SIMULATOR
       "Build the offline instrument simulator (FlukeNorma::simulator, norma_sim)"
       ON)
option(NORMA_INSTALL        "Generate install/export targets"             ON)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Building through scikit-build-core (pip install .): only the Python module.
if(SKBUILD)
    set(NORMA_BUILD_PYTHON ON)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/module/cmake")
include(Dependencies)

add_subdirectory(module/src)

if(NORMA_BUILD_C_API)
    add_subdirectory(module/capi)
endif()

if(NORMA_BUILD_PYTHON)
    add_subdirectory(module/bindings/python)
endif()

if(NORMA_BUILD_EXAMPLES)
    add_subdirectory(module/examples)
endif()

# The tests drive the library against the simulator, so it is required for them.
if(NORMA_BUILD_TESTS AND NOT NORMA_BUILD_SIMULATOR)
    message(STATUS "NORMA_BUILD_TESTS is ON: enabling NORMA_BUILD_SIMULATOR, which the tests need")
    set(NORMA_BUILD_SIMULATOR ON)
endif()

if(NORMA_BUILD_SIMULATOR)
    add_subdirectory(module/simulator)
endif()

if(NORMA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(module/tests)
endif()

# ---------------------------------------------------------------------------
# Install & package export: consumers can use find_package(FlukeNorma)
# and link FlukeNorma::core / FlukeNorma::c.
# ---------------------------------------------------------------------------
if(NORMA_INSTALL AND NOT SKBUILD)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS norma_core
        EXPORT FlukeNormaTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY module/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    if(NORMA_BUILD_C_API)
        install(TARGETS norma_c
            EXPORT FlukeNormaTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        install(DIRECTORY module/capi/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    endif()

    install(EXPORT FlukeNormaTargets
        FILE FlukeNormaTargets.cmake
        NAMESPACE FlukeNorma::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FlukeNorma)

    configure_package_config_file(
        module/cmake/FlukeNormaConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/FlukeNormaConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FlukeNorma)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/FlukeNormaConfigVersion.cmake"
        COMPATIBILITY SameMajorVersion)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/FlukeNormaConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/FlukeNormaConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FlukeNorma)
endif()
