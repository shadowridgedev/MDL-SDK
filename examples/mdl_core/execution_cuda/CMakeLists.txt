# name of the target and the resulting example
set(PROJECT_NAME examples-mdl_core-execution_cuda)

# -------------------------------------------------------------------------------------------------
# Main Target
# -------------------------------------------------------------------------------------------------

# collect sources
set(PROJECT_SOURCES
    "example_execution_cuda.cpp"
    )

# create target from template
create_from_base_preset(
    TARGET ${PROJECT_NAME}
    TYPE EXECUTABLE
    NAMESPACE mdl_core
    OUTPUT_NAME "execution_cuda"
    SOURCES ${PROJECT_SOURCES}
)

# add dependencies
target_add_dependencies(TARGET ${PROJECT_NAME}
    DEPENDS
        mdl::mdl_core
        mdl_core::shared
        opengl
        imgui
        cuda
        freeimage
        )

# creates a user settings file to setup the debugger (visual studio only, otherwise this is a no-op)
target_create_vs_user_settings(TARGET ${PROJECT_NAME})

# -------------------------------------------------------------------------------------------------
# PTX Target
# -------------------------------------------------------------------------------------------------
target_add_cuda_ptx_rule(TARGET ${PROJECT_NAME}
    DEPENDS 
        mdl::mdl_core
        mdl_core::shared
    CUDA_SOURCES
        "example_execution_cuda.cu"
        "example_execution_cuda_derivatives.cu"
    )

# add tests if available
add_tests()
