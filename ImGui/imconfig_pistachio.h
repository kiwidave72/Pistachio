#pragma once

// Pistachio ImGui configuration.
//
// We build Dear ImGui as a DLL so the host EXE and hot-reload UI plugin DLL
// share a single ImGui context (GImGui). On Windows, this requires exporting
// ImGui API symbols from imgui.dll.
//
// This header is injected using ImGui's supported IMGUI_USER_CONFIG hook.
// The build defines:
//   - IMGUI_DLL for all targets that use ImGui
//   - IMGUI_BUILD_DLL for the imgui target only

#if defined(_WIN32) && defined(IMGUI_DLL)
  #if defined(IMGUI_BUILD_DLL)
    #ifndef IMGUI_API
      #define IMGUI_API __declspec(dllexport)
    #endif
  #else
    #ifndef IMGUI_API
      #define IMGUI_API __declspec(dllimport)
    #endif
  #endif
#endif

// You can add other ImGui configuration tweaks here if needed.
