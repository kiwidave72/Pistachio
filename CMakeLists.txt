cmake_minimum_required(VERSION 3.15)
project(Pistachio VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})

# -------------------------
# OpenCASCADE Configuration
# -------------------------
if(DEFINED ENV{OpenCASCADE_DIR})
    set(OpenCASCADE_DIR $ENV{OpenCASCADE_DIR})
else()
    set(OpenCASCADE_DIR "C:/opencascade-7.9.3/opencascade-7.9.3-vc14-64" CACHE PATH "OpenCASCADE installation directory")
endif()

# Find OpenCASCADE directories
if(EXISTS "${OpenCASCADE_DIR}/inc")
    set(OpenCASCADE_INCLUDE_DIR "${OpenCASCADE_DIR}/inc")
elseif(EXISTS "${OpenCASCADE_DIR}/include/opencascade")
    set(OpenCASCADE_INCLUDE_DIR "${OpenCASCADE_DIR}/include/opencascade")
endif()

if(EXISTS "${OpenCASCADE_DIR}/win64/vc14/lib")
    set(OpenCASCADE_LIBRARY_DIR "${OpenCASCADE_DIR}/win64/vc14/lib")
    set(OpenCASCADE_BINARY_DIR "${OpenCASCADE_DIR}/win64/vc14/bin")
elseif(EXISTS "${OpenCASCADE_DIR}/lib")
    set(OpenCASCADE_LIBRARY_DIR "${OpenCASCADE_DIR}/lib")
    set(OpenCASCADE_BINARY_DIR "${OpenCASCADE_DIR}/bin")
endif()

# 3rd party directory (OCC DLL search)
set(OpenCASCADE_3RDPARTY_ROOT "C:/opencascade-7.9.3/3rdparty-vc14-64")

message(STATUS "OpenCASCADE root: ${OpenCASCADE_DIR}")
message(STATUS "OpenCASCADE includes: ${OpenCASCADE_INCLUDE_DIR}")
message(STATUS "OpenCASCADE libraries: ${OpenCASCADE_LIBRARY_DIR}")
message(STATUS "OpenCASCADE binaries: ${OpenCASCADE_BINARY_DIR}")
message(STATUS "OpenCASCADE 3rd party: ${OpenCASCADE_3RDPARTY_ROOT}")

# -------------------------
# Packages
# -------------------------
find_package(OpenGL REQUIRED)

# -------------------------
# External deps: GLFW + ImGui + GLM + STB + nlohmann-json
# -------------------------
set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")
set(GLFW_DIR     "${EXTERNAL_DIR}/glfw")
set(IMGUI_DIR    "${EXTERNAL_DIR}/imgui-fork")
set(GLM_DIR      "${EXTERNAL_DIR}/glm")
set(STB_DIR      "${EXTERNAL_DIR}/stb")
set(NLOHMANN_JSON_DIR "${EXTERNAL_DIR}/nlohmann_json")

# Your custom ImGui extension folder
set(IMGUI_CUSTOM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ImGui")

# ---- GLFW from submodule ----
add_subdirectory("${GLFW_DIR}")

# ---- nlohmann-json (header-only) ----
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
    "${NLOHMANN_JSON_DIR}/single_include"
)

# ---- ImGui from submodule ----
add_library(imgui STATIC
    # Core ImGui
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"

    # Backends
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"

    # Custom ImGui additions
    "${IMGUI_CUSTOM_DIR}/ImGuiBuild.cpp"
    "${IMGUI_CUSTOM_DIR}/ImGuiTheme.cpp"
    "${IMGUI_CUSTOM_DIR}/Image.cpp"
)

target_include_directories(imgui PUBLIC
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${IMGUI_CUSTOM_DIR}"
    "${GLM_DIR}"
    "${STB_DIR}"
)

target_link_libraries(imgui PUBLIC OpenGL::GL glfw)

# -------------------------
# Include OpenCASCADE
# -------------------------
include_directories(${OpenCASCADE_INCLUDE_DIR})

# -------------------------
# Source files
# -------------------------
set(DOMAIN_SOURCES
    src/domain/Model.cpp
    src/domain/Geometry.cpp

    # ---- Sketch domain (NEW) ----
    src/domain/SketchEntities.cpp
    src/domain/SketchConstraints.cpp
)

set(CORE_SOURCES
    src/core/Application.cpp

    # ---- Rendering (NEW) ----
    src/core/rendering/SketchRenderBuilder.cpp
)

set(ADAPTER_SOURCES
    src/adapters/ui/ImGuiAdapter.cpp
    src/adapters/ui/UI.cpp
    src/adapters/loaders/StepFileLoader.cpp
    src/adapters/exporters/ObjExporter.cpp
    src/adapters/exporters/StlExporter.cpp
    src/adapters/rendering/OcctRenderer.cpp

    # ---- Persistence (NEW) ----
    src/adapters/persistence/SketchDocumentMapper.cpp
    src/adapters/persistence/SketchDocumentJson.cpp
    src/adapters/persistence/JsonSketchDocumentAdapter.cpp
)

set(HEADER_FILES
    src/domain/Model.h
    src/domain/Geometry.h

    # ---- Sketch domain (NEW) ----
    src/domain/SketchIds.h
    src/domain/SketchMath.h
    src/domain/SketchEntities.h
    src/domain/SketchConstraints.h
    src/domain/SketchModel.h

    # ---- Rendering DTOs (NEW) ----
    src/core/rendering/RenderScene.h
    src/core/rendering/CameraState.h
    src/core/rendering/SketchRenderBuilder.h

    # ---- Ports (NEW) ----
    src/ports/ISketchDocumentPersistencePort.h

    # ---- Persistence DTO (NEW) ----
    src/adapters/persistence/SketchDocumentDto.h
    src/adapters/persistence/SketchDocumentMapper.h
    src/adapters/persistence/SketchDocumentJson.h
    src/adapters/persistence/JsonSketchDocumentAdapter.h

    src/ports/IUIPort.h
    src/ports/IFileLoaderPort.h
    src/ports/IExporterPort.h
    src/ports/IRendererPort.h
    src/core/Application.h
    src/adapters/ui/ImGuiAdapter.h
    src/adapters/ui/UI.h
    src/adapters/loaders/StepFileLoader.h
    src/adapters/exporters/ObjExporter.h
    src/adapters/exporters/StlExporter.h
    src/adapters/rendering/OcctRenderer.h
)

# -------------------------
# Main executable
# -------------------------
add_executable(${PROJECT_NAME}
    src/main.cpp
    ${DOMAIN_SOURCES}
    ${CORE_SOURCES}
    ${ADAPTER_SOURCES}
    ${HEADER_FILES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GLM_DIR}
)

# Add OpenCASCADE link directories
target_link_directories(${PROJECT_NAME} PRIVATE
    ${OpenCASCADE_LIBRARY_DIR}
)

# Link all libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui
    OpenGL::GL
    glfw
    nlohmann_json

    # Core OpenCASCADE libraries
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

    # Data Exchange libraries
    TKXSBase
    TKDESTEP

    # Additional required libraries
    TKLCAF
    TKCAF
    TKCDF
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
