#include "FileDialog.h"

#include <shobjidl.h>   // IFileOpenDialog, IShellItem
#include <combaseapi.h> // CoCreateInstance, CoTaskMemFree

namespace core {

    namespace {

        void logHResultFailure(const wchar_t* stage, HRESULT hr)
        {
            wchar_t buf[256];
            swprintf_s(buf, L"[FileDialog] %s failed: 0x%08X\n", stage, static_cast<unsigned int>(hr));
            OutputDebugStringW(buf);
        }

    } // namespace

    std::optional<std::wstring> showOpenFileDialog(
        const std::wstring& title,
        const std::vector<std::pair<std::wstring, std::wstring>>& filters,
        HWND ownerHwnd)
    {
        IFileOpenDialog* pDialog = nullptr;
        HRESULT hr = CoCreateInstance(
            CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&pDialog));
        if (FAILED(hr)) {
            logHResultFailure(L"CoCreateInstance", hr);
            if (hr == CO_E_NOTINITIALIZED) {
                OutputDebugStringW(
                    L"[FileDialog] COM was never initialized on this thread. "
                    L"Call CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED) once "
                    L"on the UI thread at startup (see main.cpp) before showing "
                    L"any file dialog.\n");
            }
            return std::nullopt;
        }

        pDialog->SetTitle(title.c_str());

        std::vector<COMDLG_FILTERSPEC> specs;
        specs.reserve(filters.size());
        for (auto& [name, pattern] : filters) {
            specs.push_back({ name.c_str(), pattern.c_str() });
        }
        if (!specs.empty()) {
            pDialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
            pDialog->SetFileTypeIndex(1);
        }

        hr = pDialog->Show(ownerHwnd);
        if (FAILED(hr)) {
            // HRESULT_FROM_WIN32(ERROR_CANCELLED) means the user just closed the dialog � not a real error.
            if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
                logHResultFailure(L"Show", hr);
            }
            pDialog->Release();
            return std::nullopt;
        }

        IShellItem* pItem = nullptr;
        hr = pDialog->GetResult(&pItem);
        if (FAILED(hr)) {
            logHResultFailure(L"GetResult", hr);
            pDialog->Release();
            return std::nullopt;
        }

        PWSTR filePath = nullptr;
        hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
        std::optional<std::wstring> result;
        if (SUCCEEDED(hr)) {
            result = std::wstring(filePath);
            CoTaskMemFree(filePath);
        }
        else {
            logHResultFailure(L"GetDisplayName", hr);
        }

        pItem->Release();
        pDialog->Release();
        return result;
    }

} // namespace core