#pragma once

#include <optional>
#include <string>
#include <vector>
#include <utility>
#include <windows.h>

namespace core {

    // Shows a native "open file" dialog. Returns nullopt if the user cancels or an error occurs.
    // filters: list of (display name, pattern) pairs, e.g. {L"Workspace Files (*.json)", L"*.json"}
    // ownerHwnd: the main window handle, used so the dialog is modal to (and appears above) the app.
    std::optional<std::wstring> showOpenFileDialog(
        const std::wstring& title,
        const std::vector<std::pair<std::wstring, std::wstring>>& filters,
        HWND ownerHwnd = nullptr);

} // namespace core