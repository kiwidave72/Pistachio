cmake_minimum_required(VERSION 3.15)
project(Pistachio VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})

# OpenCASCADE Configuration
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

# 3rd party directory
set(OpenCASCADE_3RDPARTY_ROOT "C:/opencascade-7.9.3/3rdparty-vc14-64")

message(STATUS "OpenCASCADE root: ${OpenCASCADE_DIR}")
message(STATUS "OpenCASCADE includes: ${OpenCASCADE_INCLUDE_DIR}")
message(STATUS "OpenCASCADE libraries: ${OpenCASCADE_LIBRARY_DIR}")
message(STATUS "OpenCASCADE binaries: ${OpenCASCADE_BINARY_DIR}")
message(STATUS "OpenCASCADE 3rd party: ${OpenCASCADE_3RDPARTY_ROOT}")

# Find packages
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

# Include OpenCASCADE
include_directories(${OpenCASCADE_INCLUDE_DIR})

# ImGui setup from git submodule
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
if(EXISTS ${IMGUI_DIR})
    add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC 
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC OpenGL::GL glfw)
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
    message(STATUS "ImGui found at: ${IMGUI_DIR}")
else()
    message(FATAL_ERROR "ImGui not found. Run: git submodule update --init --recursive")
endif()

# Source files
set(DOMAIN_SOURCES
    src/domain/Model.cpp
    src/domain/Geometry.cpp
)

set(CORE_SOURCES
    src/core/Application.cpp
)

set(ADAPTER_SOURCES
    src/adapters/ui/ImGuiAdapter.cpp
    src/adapters/loaders/StepFileLoader.cpp
    src/adapters/exporters/ObjExporter.cpp
    src/adapters/exporters/StlExporter.cpp
    src/adapters/rendering/OcctRenderer.cpp
)

set(HEADER_FILES
    src/domain/Model.h
    src/domain/Geometry.h
    src/ports/IUIPort.h
    src/ports/IFileLoaderPort.h
    src/ports/IExporterPort.h
    src/ports/IRendererPort.h
    src/core/Application.h
    src/adapters/ui/ImGuiAdapter.h
    src/adapters/loaders/StepFileLoader.h
    src/adapters/exporters/ObjExporter.h
    src/adapters/exporters/StlExporter.h
    src/adapters/rendering/OcctRenderer.h
)

# Main executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    ${DOMAIN_SOURCES}
    ${CORE_SOURCES}
    ${ADAPTER_SOURCES}
    ${HEADER_FILES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external
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
    TKBinL
    TKXmlL
)

# Windows-specific OpenGL linking
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
endif()

# Compiler options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3)
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/O2>)
endif()

# Set Visual Studio properties
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

if(WIN32)
    # Get all subdirectories in 3rdparty
    file(GLOB OCC_3RDPARTY_SUBDIRS "${OpenCASCADE_3RDPARTY_ROOT}/*")
    
    # Start with OpenCASCADE bin directory
    set(OCC_ALL_PATHS "${OpenCASCADE_BINARY_DIR}")
    
    message(STATUS "Searching for 3rd party DLL directories...")
    
    # Add bin directory from each subdirectory if it exists
    foreach(SUBDIR ${OCC_3RDPARTY_SUBDIRS})
        if(IS_DIRECTORY ${SUBDIR})
            # Check for bin directory
            set(BIN_PATH "${SUBDIR}/bin")
            if(EXISTS ${BIN_PATH})
                set(OCC_ALL_PATHS "${OCC_ALL_PATHS};${BIN_PATH}")
                message(STATUS "  Found: ${BIN_PATH}")
            endif()
            
            # Also check for bin/win64 subdirectory (like OpenVR)
            set(BIN_WIN64_PATH "${SUBDIR}/bin/win64")
            if(EXISTS ${BIN_WIN64_PATH})
                set(OCC_ALL_PATHS "${OCC_ALL_PATHS};${BIN_WIN64_PATH}")
                message(STATUS "  Found: ${BIN_WIN64_PATH}")
            endif()
        endif()
    endforeach()
    
    message(STATUS "Final runtime DLL paths configured for Visual Studio debugger")
    
    set_target_properties(${PROJECT_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}"
        VS_DEBUGGER_ENVIRONMENT "PATH=${OCC_ALL_PATHS};%PATH%"
    )
endif()

# Print configuration
get_target_property(LINKED_LIBS ${PROJECT_NAME} LINK_LIBRARIES)
message(STATUS "Linked libraries: ${LINKED_LIBS}")

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)