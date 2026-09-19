# C ABI shared library: FlukeNorma::c — for use from C#, LabVIEW, Rust, Java, ...
add_library(norma_c SHARED src/fluke_norma_c.cpp)
add_library(FlukeNorma::c ALIAS norma_c)

target_include_directories(norma_c PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_link_libraries(norma_c PRIVATE norma_core)
target_compile_definitions(norma_c PRIVATE FLUKE_NORMA_C_BUILD)

set_target_properties(norma_c PROPERTIES
    OUTPUT_NAME flukenorma_c
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

if(MSVC)
    target_compile_options(norma_c PRIVATE /W4)
else()
    target_compile_options(norma_c PRIVATE -Wall -Wextra -Wpedantic)
endif()
