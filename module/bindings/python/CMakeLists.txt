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
    #   PYTHONPATH=build/<preset>/module/bindings/python python -c "import flukenorma"
    #
    # Staging is its own always-run target rather than a POST_BUILD step on
    # norma_py: a POST_BUILD only fires when the extension relinks, so editing a
    # .py file alone would leave the staged copy stale and the next import would
    # silently test the previous version.
    set(norma_py_stage "$<TARGET_FILE_DIR:norma_py>/flukenorma")
    add_custom_target(norma_py_package ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${FlukeNorma_SOURCE_DIR}/python/flukenorma" "${norma_py_stage}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:norma_py>" "${norma_py_stage}/"
        DEPENDS norma_py
        COMMENT "Staging the flukenorma package next to the built module")
endif()
