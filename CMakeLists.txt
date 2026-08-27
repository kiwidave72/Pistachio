cmake_minimum_required(VERSION 3.15)
project(Pistachio VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



# -------------------------
# Output directories (multi-config safe)
# -------------------------
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" CFGU)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFGU} ${CMAKE_BINARY_DIR}/bin/${cfg})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFGU} ${CMAKE_BINARY_DIR}/lib/${cfg})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFGU} ${CMAKE_BINARY_DIR}/lib/${cfg})
endforeach()

if(NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()

# -------------------------
# Windows: export symbols from SHARED libs automatically (fast-path for plugins)
# Later you can replace this with explicit __declspec(dllexport/dllimport) macros.
# -------------------------
if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# -------------------------
# OpenCASCADE Configuration
#
# Fully optional. Source investigation found exactly two .cpp files that
# actually #include real OCC headers: OcctRenderer.cpp (whose only
# constructor call, in main.cpp, is already commented out -- entirely
# unused at runtime) and StepFileLoader.cpp (one live call site in
# main.cpp, for STEP file import). Nothing else in the codebase transitively
# needs it. This flag drops both from the build, the OCC include/link
# directories, and the whole TKernel...TKCDF link list below -- this is
# what let RelWithDebInfo (and the whole TKernel.dll/tbb12.dll runtime-DLL
# saga) be sidestepped rather than chased further. Re-enable with
# -DUSE_OPENCASCADE=ON once that's worth revisiting; nothing here is
# deleted, just conditionally compiled out.
# -------------------------
option(USE_OPENCASCADE "Enable OpenCASCADE integration (STEP import via StepFileLoader; OcctRenderer, currently unused/dead code)" OFF)

if(USE_OPENCASCADE)
    if(DEFINED ENV{OpenCASCADE_DIR})
        set(OpenCASCADE_DIR $ENV{OpenCASCADE_DIR})
    else()
        set(OpenCASCADE_DIR "C:/opencascade-7.9.3/opencascade-7.9.3-vc14-64" CACHE PATH "OpenCASCADE installation directory")
    endif()

    if(EXISTS "${OpenCASCADE_DIR}/inc")
        set(OpenCASCADE_INCLUDE_DIR "${OpenCASCADE_DIR}/inc")
    elseif(EXISTS "${OpenCASCADE_DIR}/include/opencascade")
        set(OpenCASCADE_INCLUDE_DIR "${OpenCASCADE_DIR}/include/opencascade")
    else()
        message(FATAL_ERROR "Could not find OpenCASCADE include dir under: ${OpenCASCADE_DIR}")
    endif()

    if(EXISTS "${OpenCASCADE_DIR}/win64/vc14/lib")
        set(OpenCASCADE_LIBRARY_DIR "${OpenCASCADE_DIR}/win64/vc14/lib")
        set(OpenCASCADE_BINARY_DIR "${OpenCASCADE_DIR}/win64/vc14/bin")
    elseif(EXISTS "${OpenCASCADE_DIR}/lib")
        set(OpenCASCADE_LIBRARY_DIR "${OpenCASCADE_DIR}/lib")
        set(OpenCASCADE_BINARY_DIR "${OpenCASCADE_DIR}/bin")
    else()
        message(FATAL_ERROR "Could not find OpenCASCADE lib dir under: ${OpenCASCADE_DIR}")
    endif()

    message(STATUS "OpenCASCADE root: ${OpenCASCADE_DIR}")
    message(STATUS "OpenCASCADE includes: ${OpenCASCADE_INCLUDE_DIR}")
    message(STATUS "OpenCASCADE libraries: ${OpenCASCADE_LIBRARY_DIR}")
    message(STATUS "OpenCASCADE binaries: ${OpenCASCADE_BINARY_DIR}")
else()
    message(STATUS "OpenCASCADE: disabled (USE_OPENCASCADE=OFF) -- StepFileLoader (STEP import) will not be built; OcctRenderer was already unused")
endif()

# -------------------------
# Packages
# -------------------------
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)


## GLAD (OpenGL loader)
# We keep OpenGL 3.x symbols/defines available on Windows by using GLAD.
# Expected layout (generated GLAD files):
#   external/glad/include/glad/glad.h
#   external/glad/include/KHR/khrplatform.h
#   external/glad/src/glad.c
set(GLAD_DIR ${CMAKE_SOURCE_DIR}/external/glad)
if (EXISTS ${GLAD_DIR}/include/glad/glad.h AND EXISTS ${GLAD_DIR}/src/glad.c)
    add_library(glad STATIC ${GLAD_DIR}/src/glad.c)
    target_include_directories(glad PUBLIC ${GLAD_DIR}/include)
    target_compile_definitions(glad PUBLIC GLFW_INCLUDE_NONE)
else()
    message(FATAL_ERROR
        "GLAD not found or not generated.
"
        "Expected:
"
        "  external/glad/include/glad/glad.h
"
        "  external/glad/src/glad.c
"
        "Add GLAD as a submodule (or folder) under external/glad and generate GLAD files for OpenGL 3.3 core.
")
endif()

# -------------------------
# Walnut (Image helper)
# -------------------------
# ImGuiHost uses Walnut::Image (Decode + ctor/dtor). Build it as a tiny static
# library and link it into the host EXE (and plugin, if desired).
set(WALNUT_IMAGE_CPP "${CMAKE_CURRENT_SOURCE_DIR}/Walnut/Image.cpp")
set(WALNUT_IMAGE_H   "${CMAKE_CURRENT_SOURCE_DIR}/Walnut/Image.h")
if(NOT EXISTS "${WALNUT_IMAGE_CPP}")
    # Fallback if Walnut is vendored under src/
    set(WALNUT_IMAGE_CPP "${CMAKE_CURRENT_SOURCE_DIR}/src/Walnut/Image.cpp")
    set(WALNUT_IMAGE_H   "${CMAKE_CURRENT_SOURCE_DIR}/src/Walnut/Image.h")
endif()

add_library(walnut STATIC ${WALNUT_IMAGE_CPP} ${WALNUT_IMAGE_H})
target_include_directories(walnut PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/Walnut"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/stb"
)
target_link_libraries(walnut PUBLIC glad OpenGL::GL)

# -------------------------
# External deps: GLFW + ImGui + GLM + STB + nlohmann-json
# -------------------------
set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")
set(GLFW_DIR     "${EXTERNAL_DIR}/glfw")
set(IMGUI_DIR    "${EXTERNAL_DIR}/imgui-fork")
set(GLM_DIR      "${EXTERNAL_DIR}/glm")
set(STB_DIR      "${EXTERNAL_DIR}/stb")
set(NLOHMANN_JSON_DIR "${EXTERNAL_DIR}/nlohmann_json")
set(CLIPPER2_DIR "${EXTERNAL_DIR}/Clipper2")

set(IMGUI_CUSTOM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ImGui")

# ---- GLFW from submodule ----
add_subdirectory("${GLFW_DIR}")

# ---- Clipper2 (polygon clipping/offsetting) from submodule ----
# Repo: https://github.com/AngusJohnson/Clipper2.git
# The actual CMake project for the C++ library lives under CPP/, and
# produces a "Clipper2" target (plus "Clipper2Z" if CLIPPER2_USINGZ is ON).
# We don't need its tests/examples/benchmarks, so they're disabled below.
if(EXISTS "${CLIPPER2_DIR}/CPP/CMakeLists.txt")
    set(CLIPPER2_TESTS OFF CACHE BOOL "" FORCE)
    set(CLIPPER2_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CLIPPER2_UTILS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CLIPPER2_DIR}/CPP" "${CMAKE_BINARY_DIR}/Clipper2")
    target_compile_options(Clipper2 PRIVATE /EHsc)


else()
    message(FATAL_ERROR
        "Clipper2 not found under: ${CLIPPER2_DIR}
"
        "Add it as a submodule, e.g.:
"
        "  git submodule add https://github.com/AngusJohnson/Clipper2.git external/Clipper2
")
endif()
 
 
# ---- nlohmann-json (header-only) ----
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
    "${NLOHMANN_JSON_DIR}/single_include"
)

# ---- ImGui (SHARED) ----
# IMPORTANT: Host + plugin must share a single ImGui context (GImGui),
# so ImGui must NOT be statically linked into both.
add_library(imgui SHARED
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"

    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"

    # IMPORTANT:
    # Your repo also has ImGui/ImGuiBuild.cpp, which is a "unity build" TU that
    # *also* compiles ImGui + backends by including their .cpp files. If we compile
    # both the real .cpp files AND ImGuiBuild.cpp you get LNK2005 multiply-defined
    # symbols (exactly what you're seeing).
    #
    # Therefore: do NOT compile ImGuiBuild.cpp here.
    "${IMGUI_CUSTOM_DIR}/ImGuiTheme.cpp"
    "${IMGUI_CUSTOM_DIR}/Image.cpp"


    # stb_image implementation TU (required for stbi_* exports)
    "${IMGUI_CUSTOM_DIR}/StbImageImpl.cpp"
)

target_include_directories(imgui PUBLIC
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${IMGUI_CUSTOM_DIR}"
    "${GLM_DIR}"
    "${STB_DIR}"
)

target_include_directories(imgui PUBLIC ${GLAD_DIR}/include)
target_link_libraries(imgui PUBLIC OpenGL::GL glfw glad)

# IMPORTANT (Windows / hot-reload):
# Host EXE and UI plugin DLL must share a single ImGui context (GImGui).
# Therefore we build ImGui as a DLL and export/import its API properly.
#
# Do NOT pass raw "IMGUI_API=__declspec(...)" on the compiler command line.
# MSBuild can mangle those parens and you can end up with weird system-header
# compile errors. Instead, use ImGui's supported IMGUI_USER_CONFIG hook.
#
# We define IMGUI_USER_CONFIG to point at our own config header (in ./ImGui)
# and set IMGUI_BUILD_DLL on the imgui target to enable __declspec(dllexport).
if(WIN32)
    target_compile_definitions(imgui PUBLIC IMGUI_USER_CONFIG=\"imconfig_pistachio.h\"
        IMGUI_DLL
     IMGUI_IMPL_OPENGL_LOADER_GLAD)
    target_compile_definitions(imgui PRIVATE
        IMGUI_BUILD_DLL
    )
endif()

# -------------------------
# Include OpenCASCADE
# -------------------------
if(USE_OPENCASCADE)
    include_directories(${OpenCASCADE_INCLUDE_DIR})
endif()

# -------------------------
# pistachio_core (SHARED)
# -------------------------
file(GLOB_RECURSE SOLVER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/solver/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/solvers/*.cpp"
)

# CONFIGURE_DEPENDS makes CMake re-glob whenever you build, so new domain
# files get picked up without needing to re-run cmake configure manually.
# (Re-running configure is still the more "correct" CMake workflow if you
# ever see stale results, but this covers the common case.)
file(GLOB_RECURSE DOMAIN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/domain/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/domain/*.h"
)

# See the USE_OPENCASCADE option comment above -- these two files are the
# only ones in the whole codebase that actually #include real OCC headers.
set(OCC_SOURCES "")
if(USE_OPENCASCADE)
    list(APPEND OCC_SOURCES
        src/adapters/rendering/OcctRenderer.cpp
        src/adapters/loaders/StepFileLoader.cpp
    )
endif()

add_library(pistachio_core SHARED
    # Domain
    ${DOMAIN_SOURCES}

    # Core
    src/core/Application.cpp
    src/core/ConfigStore.cpp
    src/core/rendering/SketchRenderBuilder.cpp
    src/core/TaskRunner.cpp
    src/core/FileDialog.cpp

    # Adapters
    src/adapters/eventbus/EventBus.cpp
    src/adapters/console/TaskProgressReporterConsole.cpp
    
    src/adapters/rendering/CameraGizmoGLRender.cpp
    
    src/adapters/loaders/StlMeshLoader.cpp
    src/adapters/loaders/StlLoaderAdapter.cpp

    ${OCC_SOURCES}

    src/adapters/exporters/ObjExporter.cpp
    src/adapters/exporters/StlExporter.cpp

    # Persistence
    src/adapters/persistence/SketchDocumentMapper.cpp
    src/adapters/persistence/SketchDocumentJson.cpp
    src/adapters/persistence/JsonSketchDocumentAdapter.cpp

    # Slicers
    src/adapters/slicers/planarSlicer.cpp

    ${SOLVER_SOURCES}
)

target_include_directories(pistachio_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ports
    ${GLM_DIR}
    ${STB_DIR}
    "${NLOHMANN_JSON_DIR}/single_include"
)
if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
if(USE_OPENCASCADE)
    target_link_directories(pistachio_core PUBLIC ${OpenCASCADE_LIBRARY_DIR})
endif()

target_link_libraries(pistachio_core PUBLIC
    imgui
    nlohmann_json
    Threads::Threads
)

if(USE_OPENCASCADE)
    target_link_libraries(pistachio_core PUBLIC
        TKernel
        TKMath
        TKG2d
        TKG3d
        TKGeomBase
        TKGeomAlgo
        TKBRep
        TKTopAlgo
        TKPrim
        TKMesh
        TKService
        TKOpenGl
        TKV3d
        TKXCAF
        TKBin
        TKBinXCAF
        TKXSBase
        TKDESTEP
        TKLCAF
        TKCAF
        TKCDF
    )
    # PUBLIC so consumers (main.cpp) can #ifdef USE_OPENCASCADE around the
    # StepFileLoader construction and #include of OcctRenderer.h -- the
    # latter pulls in real OCC headers itself, so it can't just be included
    # unconditionally when the OCC include dir isn't even on the path.
    target_compile_definitions(pistachio_core PUBLIC USE_OPENCASCADE)
endif()

if(WIN32)
    target_link_libraries(pistachio_core PUBLIC opengl32)
endif()

# -------------------------
# pistachio_ui plugin (SHARED)
# -------------------------
add_library(pistachio_ui SHARED
    src/adapters/ui/ImGuiAdapter.cpp
    src/adapters/ui/UI.cpp
    src/adapters/ui/ThemeColors.cpp
    src/adapters/ui/plugins/PistachioUiModule.cpp
)

target_include_directories(pistachio_ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ports
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${IMGUI_CUSTOM_DIR}"
    ${GLM_DIR}
    ${STB_DIR}
)

target_link_libraries(pistachio_ui PRIVATE
    pistachio_core
    imgui
    OpenGL::GL
    nlohmann_json
    # glad and walnut come transitively through imgui/pistachio_core.
    # Linking them here again would produce a second static copy in the DLL
    # with separate global state (GLAD function tables, stb_image globals)
    # which causes heap corruption when the DLL is hot-reloaded.
)



# Put the plugin DLL next to the exe in bin/<cfg>/plugins
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" CFGU)
    set_target_properties(pistachio_ui PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins"
        LIBRARY_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins"
        ARCHIVE_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/lib/${cfg}"
    )
endforeach()

# -------------------------
# Main executable
# -------------------------
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/adapters/ui/ImGuiHost.cpp
    src/adapters/ui/ThemeColors.cpp
    src/adapters/ui/TaskProgressReporterImGui.cpp
    src/adapters/ui/plugins/UiPluginLoader.cpp
    src/adapters/ui/plugins/UiPluginRegistry.cpp
    src/adapters/plugins/ServiceModuleLoader.cpp
    src/adapters/plugins/ServiceModuleRegistry.cpp

    # --- OpenGL 3D viewport (offscreen FBO -> ImGui::Image) ---
    src/adapters/rendering/GLLoader.cpp
    src/adapters/rendering/FramebufferManager.cpp
    src/adapters/rendering/OpenGlRenderer.cpp
    src/adapters/rendering/GlCubeViewRenderer.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ports
    ${GLM_DIR}
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${IMGUI_CUSTOM_DIR}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    pistachio_core
    imgui
    OpenGL::GL
    glfw
    nlohmann_json
    glad
    walnut
)


if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
endif()

# -------------------------
# pistachio_config plugin
# -------------------------
add_library(pistachio_config SHARED
    src/adapters/ui/plugins/PistachioConfigPlugin.cpp
)

target_include_directories(pistachio_config PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ports
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${IMGUI_CUSTOM_DIR}"
    ${GLM_DIR}
    ${STB_DIR}
)

target_link_libraries(pistachio_config PRIVATE
    pistachio_core
    imgui
    nlohmann_json
)

foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" CFGU)
    set_target_properties(pistachio_config PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins"
        LIBRARY_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins"
        ARCHIVE_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/lib/${cfg}"
    )
endforeach()

# -------------------------
# pistachio_slicer_core plugin
# -------------------------
add_library(pistachio_slicer_core SHARED
    src/adapters/ui/plugins/SlicerCorePlugin/core/SlicerCorePlugin.cpp
    src/adapters/ui/plugins/SlicerCorePlugin/core/FolderScanner.cpp
    src/adapters/ui/plugins/SlicerCorePlugin/core/StlPreviewRenderer.cpp
    
    src/adapters/ui/plugins/SlicerCorePlugin/domain/Project.cpp

    src/adapters/ui/plugins/SlicerCorePlugin/core/BuildPLateRenderer.cpp
    src/adapters/ui/plugins/SlicerCorePlugin/domain/BoundaryBox.h
    src/adapters/ui/plugins/SlicerCorePlugin/domain/BoundaryBox.cpp
 
    src/adapters/ui/plugins/SlicerCorePlugin/core/ModelInstanceFactory.cpp
    src/adapters/ui/plugins/SlicerCorePlugin/core/SelectionManager.cpp
    src/adapters/ui/plugins/SlicerCorePlugin/core/NavigationManager.cpp
    src/adapters/ui/plugins/SlicerCorePlugin/core/EditableSceneGLRender.cpp
    src/adapters/ui/plugins/SlicerCorePlugin/core/MultiPlateSceneGLRender.cpp

)

target_include_directories(pistachio_slicer_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ports
    ${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/ui/plugins/SlicerCorePlugin
    ${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/ui/plugins/SlicerCorePlugin/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/ui/plugins/SlicerCorePlugin/domain
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${IMGUI_CUSTOM_DIR}"
    ${GLM_DIR}
    ${STB_DIR}
    "${CLIPPER2_DIR}/CPP/Clipper2Lib/include"
 )

target_link_libraries(pistachio_slicer_core PRIVATE
    pistachio_core
    imgui
    nlohmann_json
    Clipper2
    freetype          
)

foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" CFGU)
    set_target_properties(pistachio_slicer_core PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins"
        LIBRARY_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins"
        ARCHIVE_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/lib/${cfg}"
    )
endforeach()

# -------------------------
# pistachio_toolpath_visualization (ToolpathVisualizationPlugin � IUiModule)
# -------------------------
add_library(pistachio_toolpath_visualization SHARED
    src/adapters/ui/plugins/ToolpathVisualizationPlugin/ToolpathVisualizationPlugin.cpp
    src/adapters/ui/plugins/ToolpathVisualizationPlugin/TestGridGLRender.cpp
    src/adapters/ui/plugins/ToolpathVisualizationPlugin/ToolpathRibbonGLRender.cpp 
    src/adapters/ui/plugins/ToolpathVisualizationPlugin/ToolpathRibbonGLMesh.cpp
    src/adapters/ui/plugins/ToolpathVisualizationPlugin/DebugComparisonGLRender.cpp

)

target_include_directories(pistachio_toolpath_visualization PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ports
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${IMGUI_CUSTOM_DIR}"
    ${GLM_DIR}
    ${STB_DIR}
)

target_link_libraries(pistachio_toolpath_visualization PRIVATE
    pistachio_core
)

# Output directory is "plugins", not "services" � this is an IUiModule,
# loaded in Phase 2 by UiPluginRegistry, not ServiceModuleRegistry.
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" CFGU)
    set_target_properties(pistachio_toolpath_visualization PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins"
        LIBRARY_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins"
        ARCHIVE_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/lib/${cfg}"
    )
endforeach()

# -------------------------
# pistachio_kinetica (ToolpathEnginePlugin � IServiceModule)
# -------------------------
add_library(pistachio_kinetica SHARED
    src/adapters/plugins/ToolpathEnginePlugin/ToolpathEnginePlugin.cpp
    src/adapters/plugins/ToolpathEnginePlugin/ImportPhase.cpp
    src/adapters/plugins/ToolpathEnginePlugin/ValidationPhase.cpp
    src/adapters/plugins/ToolpathEnginePlugin/AccelerationPhase.cpp
    src/adapters/plugins/ToolpathEnginePlugin/SlicingPhase.cpp
    src/adapters/plugins/ToolpathEnginePlugin/ExtractionPhase.cpp
    src/adapters/plugins/ToolpathEnginePlugin/TopologyPhase.cpp
    src/adapters/plugins/ToolpathEnginePlugin/LayerBitmapDebug.cpp
    src/adapters/plugins/ToolpathEnginePlugin/WallGenerationPhase.cpp
    src/adapters/plugins/ToolpathEnginePlugin/InfillRegionPhase.cpp
    src/adapters/plugins/ToolpathEnginePlugin/RectilinearInfillStrategy.cpp
    src/adapters/plugins/ToolpathEnginePlugin/TopologyRepairPhase.cpp


)

target_include_directories(pistachio_kinetica PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
     "${CLIPPER2_DIR}/CPP/Clipper2Lib/include"
)

target_link_libraries(pistachio_kinetica PRIVATE
    pistachio_core
    Clipper2
)

# Output directory is "services", not "plugins" � this is what
# ServiceModuleRegistry::getServicesDir() scans.
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" CFGU)
    set_target_properties(pistachio_kinetica PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}/services"
        LIBRARY_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}/services"
        ARCHIVE_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/lib/${cfg}"
    )
endforeach()

# -------------------------
# pistachio_ui_stub (minimal hot-reload test plugin)
# -------------------------
add_library(pistachio_ui_stub SHARED
    src/adapters/ui/plugins/StubUiModule.cpp
)

target_include_directories(pistachio_ui_stub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(pistachio_ui_stub PRIVATE
    pistachio_core
    imgui
)

foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" CFGU)
    set_target_properties(pistachio_ui_stub PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins"
        LIBRARY_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}/plugins"
        ARCHIVE_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/lib/${cfg}"
    )
endforeach()

# -------------------------
# pistachio_service_stub (minimal IServiceModule test plugin)
# -------------------------
add_library(pistachio_service_stub SHARED
    src/adapters/plugins/StubServiceModule.cpp
)

target_include_directories(pistachio_service_stub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(pistachio_service_stub PRIVATE
    pistachio_core
)

# NOTE: output directory is "services", not "plugins" � this is what
# ServiceModuleRegistry::getServicesDir() scans. Every other plugin
# target in this file outputs to bin/<cfg>/plugins; this one must not.
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" CFGU)
    set_target_properties(pistachio_service_stub PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}/services"
        LIBRARY_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/bin/${cfg}/services"
        ARCHIVE_OUTPUT_DIRECTORY_${CFGU} "${CMAKE_BINARY_DIR}/lib/${cfg}"
    )
endforeach()

# -------------------------
# Switch between real and stub UI plugin
# cmake -DUSE_STUB_UI=ON to test hot-reload with the stub
# -------------------------
option(USE_STUB_UI "Use stub UI plugin for hot-reload testing" OFF)
if(USE_STUB_UI)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_STUB_UI=1)
endif()