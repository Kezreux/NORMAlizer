# pybind11 Python extension: flukenorma._core
pybind11_add_module(norma_py MODULE src/module.cpp)
set_target_properties(norma_py PROPERTIES OUTPUT_NAME _core)
target_link_libraries(norma_py PRIVATE norma_core)

if(SKBUILD)
    # pip install . — the extension goes into the wheel's flukenorma/ package
    # (the pure-Python part is picked up via wheel.packages in pyproject.toml).
    install(TARGETS norma_py DESTINATION flukenorma)
else()
    # Developer convenience: assemble an importable package in the build tree,
    # e.g.  PYTHONPATH=build/windows-msvc/module/bindings/python/Release python -c "import flukenorma"
    add_custom_command(TARGET norma_py POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${FlukeNorma_SOURCE_DIR}/python/flukenorma"
                "$<TARGET_FILE_DIR:norma_py>/flukenorma"
        COMMAND ${CMAKE_COMMAND} -E copy
                "$<TARGET_FILE:norma_py>"
                "$<TARGET_FILE_DIR:norma_py>/flukenorma/"
        COMMENT "Staging flukenorma package next to the built module")
endif()
