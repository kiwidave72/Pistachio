cmake_minimum_required(VERSION 3.15)
project(Pistachio VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(PISTACHIO_BUILD_APP "Build the Pistachio UI app (ImGui + OCC)" ON)
option(PISTACHIO_BUILD_TESTS "Build unit/integration tests" ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})

# -------------------------
# OpenCASCADE Configuration (App only)
# -------------------------
if(PISTACHIO_BUILD_APP)
    if(DEFINED ENV{OpenCASCADE_DIR})
        set(OpenCASCADE_DIR $ENV{OpenCASCADE_DIR})
    else()
        set(OpenCASCADE_DIR "C:/opencascade-7.9.3/opencascade-7.9.3-vc14-64" CACHE PATH "OpenCASCADE installation directory")
    endif()

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

    set(OpenCASCADE_3RDPARTY_ROOT "C:/opencascade-7.9.3/3rdparty-vc14-64")

    message(STATUS "OpenCASCADE root: ${OpenCASCADE_DIR}")
    message(STATUS "OpenCASCADE includes: ${OpenCASCADE_INCLUDE_DIR}")
    message(STATUS "OpenCASCADE libraries: ${OpenCASCADE_LIBRARY_DIR}")
    message(STATUS "OpenCASCADE binaries: ${OpenCASCADE_BINARY_DIR}")
    message(STATUS "OpenCASCADE 3rd party: ${OpenCASCADE_3RDPARTY_ROOT}")

    find_package(OpenGL REQUIRED)
endif()

# -------------------------
# External deps (App only): GLFW + ImGui + GLM + STB + nlohmann-json
# -------------------------
set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")
set(GLFW_DIR     "${EXTERNAL_DIR}/glfw")
set(IMGUI_DIR    "${EXTERNAL_DIR}/imgui-fork")
set(GLM_DIR      "${EXTERNAL_DIR}/glm")
set(STB_DIR      "${EXTERNAL_DIR}/stb")
set(NLOHMANN_JSON_DIR "${EXTERNAL_DIR}/nlohmann_json")
set(IMGUI_CUSTOM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ImGui")

if(PISTACHIO_BUILD_APP)
    add_subdirectory("${GLFW_DIR}")

    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE
        "${NLOHMANN_JSON_DIR}/single_include"
    )

    add_library(imgui STATIC
        "${IMGUI_DIR}/imgui.cpp"
        "${IMGUI_DIR}/imgui_draw.cpp"
        "${IMGUI_DIR}/imgui_tables.cpp"
        "${IMGUI_DIR}/imgui_widgets.cpp"
        "${IMGUI_DIR}/imgui_demo.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
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

    include_directories(${OpenCASCADE_INCLUDE_DIR})
endif()

# -------------------------
# Core sources (used by both App + Tests)
# -------------------------
set(DOMAIN_SOURCES
    src/domain/Model.cpp
    src/domain/Geometry.cpp
    src/domain/SketchEntities.cpp
    src/domain/SketchConstraints.cpp
)

set(DOMAIN_HEADERS
    src/domain/Model.h
    src/domain/Geometry.h
    src/domain/SketchIds.h
    src/domain/SketchMath.h
    src/domain/SketchEntities.h
    src/domain/SketchConstraints.h
    src/domain/SketchModel.h
)

# --- BasicConstraintSolver location (supports either src/... or repo-root adapters/...) ---
set(SOLVER_SOURCES "")
set(SOLVER_HEADERS "")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/solvers/BasicConstraintSolver.cpp")
    list(APPEND SOLVER_SOURCES src/adapters/solvers/BasicConstraintSolver.cpp)
    list(APPEND SOLVER_HEADERS src/adapters/solvers/BasicConstraintSolver.h)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/adapters/solvers/BasicConstraintSolver.cpp")
    list(APPEND SOLVER_SOURCES adapters/solvers/BasicConstraintSolver.cpp)
    list(APPEND SOLVER_HEADERS adapters/solvers/BasicConstraintSolver.h)
endif()

# --- SketchDirtyState header location (supports either src/... or repo-root core/...) ---
set(DIRTY_HEADERS "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/core/SketchDirtyState.h")
    list(APPEND DIRTY_HEADERS src/core/SketchDirtyState.h)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/core/SketchDirtyState.h")
    list(APPEND DIRTY_HEADERS core/SketchDirtyState.h)
endif()

# Core library: NO ImGui/OCC deps
add_library(pistachio_core STATIC
    ${DOMAIN_SOURCES}
    ${DOMAIN_HEADERS}
    ${SOLVER_SOURCES}
    ${SOLVER_HEADERS}
    ${DIRTY_HEADERS}
)

target_include_directories(pistachio_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}    # allows includes if solver/dirty live at repo-root
)

# -------------------------
# App target (your existing build, mostly unchanged)
# -------------------------
if(PISTACHIO_BUILD_APP)

    set(CORE_SOURCES
        src/core/Application.cpp
        src/core/rendering/SketchRenderBuilder.cpp
    )

    set(ADAPTER_SOURCES
        src/adapters/ui/ImGuiAdapter.cpp
        src/adapters/ui/UI.cpp
        src/adapters/loaders/StepFileLoader.cpp
        src/adapters/exporters/ObjExporter.cpp
        src/adapters/exporters/StlExporter.cpp
        src/adapters/rendering/OcctRenderer.cpp
        src/adapters/persistence/SketchDocumentMapper.cpp
        src/adapters/persistence/SketchDocumentJson.cpp
        src/adapters/persistence/JsonSketchDocumentAdapter.cpp
    )

    set(HEADER_FILES
        src/core/rendering/RenderScene.h
        src/core/rendering/CameraState.h
        src/core/rendering/SketchRenderBuilder.h
        src/ports/ISketchDocumentPersistencePort.h
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

    add_executable(${PROJECT_NAME}
        src/main.cpp
        ${CORE_SOURCES}
        ${ADAPTER_SOURCES}
        ${HEADER_FILES}
    )

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${GLM_DIR}
    )

    target_link_directories(${PROJECT_NAME} PRIVATE
        ${OpenCASCADE_LIBRARY_DIR}
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        pistachio_core
        imgui
        OpenGL::GL
        glfw
        nlohmann_json

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

    if(WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
    endif()

    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

endif()

# -------------------------
# Tests (no UI/OCC deps)
# -------------------------
if(PISTACHIO_BUILD_TESTS)
    enable_testing()

    # tests path (supports either tests/... at repo-root or src/tests/...)
    set(TEST_SOURCES "")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp")
        list(APPEND TEST_SOURCES
            tests/main.cpp
            tests/test_constraints.cpp
            tests/test_dirty_and_integration.cpp
        )
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/main.cpp")
        list(APPEND TEST_SOURCES
            src/tests/main.cpp
            src/tests/test_constraints.cpp
            src/tests/test_dirty_and_integration.cpp
        )
    endif()

    add_executable(pistachio_tests ${TEST_SOURCES})

    target_include_directories(pistachio_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}      # so "tests/mini_test.h" resolves
        ${CMAKE_CURRENT_SOURCE_DIR}/src  # so "domain/..." resolves
    )

    target_link_libraries(pistachio_tests PRIVATE pistachio_core)

    add_test(NAME pistachio_tests COMMAND pistachio_tests)
endif()
