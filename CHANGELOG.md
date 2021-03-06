Change Log
==========

MDL SDK 2018.1.2 (312200.1281): 11 Dec 2018
-----------------------------------------------

ABI compatible with the MDL SDK 2018.1.2 (312200.1281)
binary release (see [https://developer.nvidia.com/mdl-sdk](https://developer.nvidia.com/mdl-sdk))

**Added and Changed Features**

- MDL 1.5 Language Specification
    - A first pre-release draft of the NVIDIA Material Definition Language 1.5: Appendix E - Internationalization has been added to the documentation set.

- General
    - Support for the internationalization of MDL string annotations has been added. See the MDL 1.5 Language Specification for details.
    - A new API component `mi::neuraylib::IMdl_i18n_configuration` has been added, which can be used to query and change MDL internationalization settings.
    - A new standalone tool to create XLIFF files has been added. See `i18n`.
    - Calling `mi::neuraylib::ITransaction::remove()` on an MDL module will cause the module and all its definitions and other dependencies to be removed from the database as soon as it is no longer referenced by another module, material instance or function call. The actual removal is triggered by calling `mi::neuraylib::ITransaction::commit()`.
    - A new API component `mi::neuraylib::Mdl_compatibility_api` has been added which allows to test archives and modules for compatibility.
    - A new standalone tool to manage MDL archives has been added. See `mdlm`.
    - A new API class `mi::neuraylib::IMdl_execution_context` intended to pass options to and receive messages from the MDL compiler has been added.
    - A new API class `mi::neuraylib::IMessage` intended to propagate MDL compiler and SDK messages has been added.
    - A new API function `mi::neuraylib::IMdl_factory::create_execution_context` has been added.
    - The signatures of the API functions
        - `mi::neuraylib::IMaterial_instance::create_compiled_material()`
        - `mi::neuraylib::IMdl_compiler::load_module()`
        - `mi::neuraylib::IMdl_compiler::load_module_from_string()`
        - `mi::neuraylib::IMdl_compiler::export_module()`
        - `mi::neuraylib::IMdl_compiler::export_module_to_string()`
        - `mi::neuraylib::IMdl_backend::translate_environment()`
        - `mi::neuraylib::IMdl_backend::translate_material_expression()`
        - `mi::neuraylib::IMdl_backend::translate_material_df()`
        - `mi::neuraylib::IMdl_backend::translate_material()`
        - `mi::neuraylib::IMdl_backend::create_link_unit()`
        - `mi::neuraylib::IMdl_backend::translate_link_unit()`
        - `mi::neuraylib::ILink_unit::add_environment()`
        - `mi::neuraylib::ILink_unit::add_material_expression()`
        - `mi::neuraylib::ILink_unit::add_material_df()`
        - `mi::neuraylib::ILink_unit::add_material()`
      
      have been changed to use the new class `mi::neuraylib::IMdl_execution_context`.
      The old versions have been deprecated and prefixed with `deprecated_`. They can
      be restored to their original names by setting the preprocessor define
      `MI_NEURAYLIB_DEPRECATED_9_1`.
    - The API functions
        - `mi::neuraylib::IMdl_backend::translate_material_expression_uniform_state()`
        - `mi::neuraylib::IMdl_backend::translate_material_expressions()`
      
      have been deprecated and prefixed with `deprecated_`. They can be restored to
      their original names by setting the preprocessor define `MI_NEURAYLIB_DEPRECATED_9_1`.
    - The utility classes
        - `mi::neuraylib::Definition_wrapper` and
        - `mi::neuraylib::Argument_editor`
      
      have been extended to provide member access functions.

- MDL Compiler and Backends
    - Support for automatic derivatives for 2D texture lookups has been added to the PTX,
  Native x86 and LLVM IR backends. This feature can be enabled via the new backend option "texture_runtime_with_derivs". Please refer to the "Example for Texture Filtering with Automatic Derivatives" documentation for more details.
    - Measured EDFs and BSDFs can now be translated to PTX, Native x86 and LLVM IR. Note that the texture runtime needs to be extended with utility functions that enable runtime access to the data.
    - Spot EDFs can now be translated to PTX, Native x86 and LLVM IR.
    - The `nvidia::df` module has been removed.

- MDL SDK examples
    - Support for automatic derivatives has been added to the `example_execution_native`,`example_execution_cuda` and `example_df_cuda` examples, which can be enabled via a command line option.
    - The `example_execution_native` example has been extended to allow to specify materials on the command line. It is now also possible to enable the user-defined texture runtime via a command line switch.
    - The CUDA example texture runtime has been extended with support for measured EDF and BSDF data.
    - The MDL Browser is now available as a QT QML Module which can also be integrated in non-qt based applications.
    - Initial support for class compiled parameters of type `Texture`, `Light_profile`, and `Bsdf_measurement` has been added to `example_df_cuda`. So far, it is only possible to switch between all loaded resources, new resources cannot be added.

**Fixed Bugs**

- General
    - An error when exporting presets where MDL definitions used in the arguments require a different version than the prototype definition has been fixed.

- MDL Compiler and Backends
    - A missing check for validity of refracted directions has been added to the generated code for the evaluation of microfacet BSDFs.
    - Incorrect code generation for `math::length()` with the atomic types `float` and `double` has been fixed.
    - The computation of the minimum correction pattern in the MDL compiler has been fixed.
    - The compilation of || and && inside DAG IR has been fixed.
    - Pre and post increment/decrement operators when inlined into DAG-IR have been fixed.
    - Previously missing mixed vector/atomic versions of `math::min()` and `math::max()`
  have been added.
    - The handling of (wrong) function references inside array constructor and init constructor has been fixed, producing better MDL compiler error messages.
    - The hash computation of lambda functions with parameters has been fixed.
    - If an absolute file url is given for a module to be resolved AND this module exists in the module cache, the module cache is used to determine its file name. This can speed up file resolution and allows the creation of presets even if the original module is not in the module path anymore.
    - A memory leak in the JIT backend has been fixed.
    - The generated names of passed expressions for code generation have been fixed.

**Known Restrictions**

- When generating code for distribution functions, the parameter `global_distribution` on spot and measured EDF's is currently ignored and assumed to be false.

MDL SDK 2018.1.1 (307800.2890): 15 Sep 2018
-----------------------------------------------

ABI compatible with the MDL SDK 2018.1.1
binary release (see [https://developer.nvidia.com/mdl-sdk](https://developer.nvidia.com/mdl-sdk))

**Added and Changed Features**

- General
    -  A new API function `mi::neuraylib::ILink_unit::add_material` has been added to
       translate multiple distribution functions and expressions of a material at once.
    -  A new API function `mi::neuraylib::IMdl_backend::translate_material` has been added to
       translate multiple distribution functions and expressions of a material at once.
    -  A new function `mi::neuraylib::ICompiled_material::get_connected_function_db_name`
       has been added.
    -  A new function `IImage_api::create_mipmaps` has been added.
    -  A new parameter has been added to the functions `mi::neuraylib::ITarget_code::execute*`
       to allow passing in user-defined texture access functions.

- MDL Compiler and Backends
    - Diffuse EDFs can now be translated to PTX, native x86 and LLVM IR.
    - Support for passing custom texture access functions has been added to the Native backend.
      The built-in texture handler can be disabled via the new backend option
      `"use_builtin_resource_handler"`.

- MDL SDK examples
    - The `example_df_cuda` example now features simple path tracing inside the sphere to
      enable rendering of transmitting BSDFs.
    - To allow loading of multiple materials within a module, a wildcard suffix "*" is now
      supported in the material name command line parameter of the `example_df_cuda` example.
    - The `example_df_cuda` has been updated to illustrate the use of the new function
      `mi::neuraylib::ILink_unit::add_material`.
    - The `example_execution_native` has been extended to illustrate the use of user-defined
      texture access functions.
    - The `example_mdl_browser` can now be built on Mac OS.

**Fixed Bugs**

- General
    - The handling of archives containing a single module has been fixed in the
      `mi::neuraylib::IMdl_discovery_api`.
    - The handling of relative search paths has been fixed in the
      `mi::neuraylib::IMdl_discovery_api`.

- MDL Compiler and Backends
    - Various fixes have been applied to the code generated for BSDF's:
        - The computation of the `evaluate()` function for glossy refraction has been fixed
          (`df::simple_glossy_bsdf, df::microfacet*`).
        - The `sample()` functions for layering and mixing now properly compute the full PDF
          including the non-selected components.
        - The implementation of `df::color_clamped_mix()` has been fixed
          (the PDF was incorrect and BSDFs potentially got skipped).
        - All mixers now properly clamp weights to 0..1.
        - Total internal reflection is now discarded for glossy BSDF
          (`df::simple_glossy_bsdf`, `df::microfacet*`) with mode `df::scatter_transmit`,
          as defined in the MDL specification.
    - Incorrect code generation for `math::normalize()` with the atomic types `float` and
      `double` has been fixed.
    - The generation of function names for array index functions for modules in packages
      has been fixed.
    - In rare cases, compilation of a df* function could result in undeclared parameter names
      (missing _param_X error), which has been fixed.
    - The compilation of MDL presets of re-exported materials has been fixed.
    - In rare cases, the original name of a preset was not computed, which has been fixed.


MDL SDK 2018.1 (307800.1800): 09 Aug 2018
-----------------------------------------------

- Initial open source release
- ABI compatible with the MDL SDK 2018.1 (307800.1800) binary release (see [https://developer.nvidia.com/mdl-sdk](https://developer.nvidia.com/mdl-sdk))
- The following features are only available in the binary release and excluded in the
  source code release:
    - MDL distiller
    - Texture baking (see *examples/mdl_sdk/execution_cuda* for example code for texture baking)
    - GLSL compiler back end
- Added: MDL Core API, a lower-level compiler API in the MDL SDK (see *src/prod/lib/mdl_core* and *doc/mdl_coreapi*)
- Added: examples for the MDL Core API (see *examples/mdl_core* and *doc/mdl_coreapi*)
