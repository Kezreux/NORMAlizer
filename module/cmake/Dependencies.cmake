# ---------------------------------------------------------------------------
# Third-party dependencies.
#
# Every dependency is first looked up with find_package() (so vcpkg/Conan or a
# system install is used when available) and otherwise fetched pinned via
# FetchContent, so a plain `cmake -S . -B build` works with no setup.
# ---------------------------------------------------------------------------
include(FetchContent)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW) # use extraction time as file timestamp
endif()

# --- Asio (standalone, header-only) — TCP transport --------------------------
find_package(asio CONFIG QUIET)
if(NOT TARGET asio::asio)
    FetchContent_Declare(asio
        URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-30-2.tar.gz)
    FetchContent_MakeAvailable(asio)

    add_library(asio INTERFACE)
    add_library(asio::asio ALIAS asio)
    target_include_directories(asio INTERFACE "${asio_SOURCE_DIR}/asio/include")
    target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
    find_package(Threads REQUIRED)
    target_link_libraries(asio INTERFACE Threads::Threads)
    if(WIN32)
        target_compile_definitions(asio INTERFACE _WIN32_WINNT=0x0A00)
        target_link_libraries(asio INTERFACE ws2_32 mswsock)
    endif()
endif()

# --- pybind11 — Python bindings ----------------------------------------------
if(NORMA_BUILD_PYTHON)
    FetchContent_Declare(pybind11
        URL https://github.com/pybind/pybind11/archive/refs/tags/v3.0.1.tar.gz
        FIND_PACKAGE_ARGS CONFIG)
    FetchContent_MakeAvailable(pybind11)
endif()

# --- Catch2 — unit tests ------------------------------------------------------
if(NORMA_BUILD_TESTS)
    FetchContent_Declare(Catch2
        URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.8.0.tar.gz
        FIND_PACKAGE_ARGS 3 CONFIG)
    FetchContent_MakeAvailable(Catch2)
    if(DEFINED catch2_SOURCE_DIR AND EXISTS "${catch2_SOURCE_DIR}/extras")
        list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    endif()
endif()
