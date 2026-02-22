#include "dialog.hpp"
#include "window.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdexcept>
#include <algorithm>

namespace alia {

namespace {

std::wstring utf8_to_utf16(const char* utf8)
{
    if (!utf8 || *utf8 == '\0') return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (size <= 0) return L"";
    std::wstring result(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &result[0], size);
    return result;
}

std::wstring utf8_to_utf16(std::string_view utf8)
{
    if (utf8.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (size <= 0) return L"";
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), &result[0], size);
    return result;
}

std::string utf16_to_utf8(const wchar_t* utf16)
{
    if (!utf16 || *utf16 == L'\0') return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string result(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16, -1, &result[0], size, nullptr, nullptr);
    return result;
}

HWND get_hwnd(const window* parent)
{
    if (parent) {
        return static_cast<HWND>(parent->native_handle());
    }
    return nullptr;
}

} // namespace

message_box_result show_message_box(
    const char* title,
    const char* message,
    message_box_buttons buttons,
    message_box_icon icon,
    const window* parent
) {
    UINT type = 0;

    switch (buttons) {
        case message_box_buttons::ok:                 type |= MB_OK; break;
        case message_box_buttons::ok_cancel:          type |= MB_OKCANCEL; break;
        case message_box_buttons::yes_no:             type |= MB_YESNO; break;
        case message_box_buttons::yes_no_cancel:      type |= MB_YESNOCANCEL; break;
        case message_box_buttons::retry_cancel:       type |= MB_RETRYCANCEL; break;
        case message_box_buttons::abort_retry_ignore: type |= MB_ABORTRETRYIGNORE; break;
    }

    switch (icon) {
        case message_box_icon::none:     break;
        case message_box_icon::info:     type |= MB_ICONINFORMATION; break;
        case message_box_icon::warning:  type |= MB_ICONWARNING; break;
        case message_box_icon::error:    type |= MB_ICONERROR; break;
        case message_box_icon::question: type |= MB_ICONQUESTION; break;
    }

    int result = MessageBoxW(
        get_hwnd(parent),
        utf8_to_utf16(message).c_str(),
        utf8_to_utf16(title).c_str(),
        type
    );

    switch (result) {
        case IDOK:     return message_box_result::ok;
        case IDCANCEL: return message_box_result::cancel;
        case IDYES:    return message_box_result::yes;
        case IDNO:     return message_box_result::no;
        case IDRETRY:  return message_box_result::retry;
        case IDABORT:  return message_box_result::abort;
        case IDIGNORE: return message_box_result::ignore;
        default:       return message_box_result::cancel;
    }
}

void show_info_box(const char* title, const char* message, const window* parent)
{
    show_message_box(title, message, message_box_buttons::ok, message_box_icon::info, parent);
}

void show_warning_box(const char* title, const char* message, const window* parent)
{
    show_message_box(title, message, message_box_buttons::ok, message_box_icon::warning, parent);
}

void show_error_box(const char* title, const char* message, const window* parent)
{
    show_message_box(title, message, message_box_buttons::ok, message_box_icon::error, parent);
}

bool show_confirm_box(const char* title, const char* message, const window* parent)
{
    return show_message_box(title, message, message_box_buttons::yes_no, message_box_icon::question, parent) == message_box_result::yes;
}

// File Dialog Helper
struct com_initializer {
    com_initializer() { CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); }
    ~com_initializer() { CoUninitialize(); }
};

std::optional<std::string> show_open_file_dialog(
    const window* parent,
    const char* title,
    const file_dialog_options& options
) {
    com_initializer com;
    IFileOpenDialog* pFileOpen;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

    if (FAILED(hr)) return std::nullopt;

    pFileOpen->SetTitle(utf8_to_utf16(title).c_str());

    if (!options.filters.empty()) {
        std::vector<COMDLG_FILTERSPEC> specs;
        std::vector<std::wstring> descriptions;
        std::vector<std::wstring> patterns;
        for (const auto& f : options.filters) {
            descriptions.push_back(utf8_to_utf16(f.description));
            patterns.push_back(utf8_to_utf16(f.patterns));
            specs.push_back({descriptions.back().c_str(), patterns.back().c_str()});
        }
        pFileOpen->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
    }

    if (!options.initial_path.empty()) {
        IShellItem* pItem;
        hr = SHCreateItemFromParsingName(utf8_to_utf16(options.initial_path).c_str(), nullptr, IID_PPV_ARGS(&pItem));
        if (SUCCEEDED(hr)) {
            pFileOpen->SetFolder(pItem);
            pItem->Release();
        }
    }

    if (!options.default_extension.empty()) {
        pFileOpen->SetDefaultExtension(utf8_to_utf16(options.default_extension).c_str());
    }

    FILEOPENDIALOGOPTIONS fos;
    pFileOpen->GetOptions(&fos);
    if (options.show_hidden) fos |= FOS_FORCESHOWHIDDEN;
    pFileOpen->SetOptions(fos);

    hr = pFileOpen->Show(get_hwnd(parent));
    std::optional<std::string> result;

    if (SUCCEEDED(hr)) {
        IShellItem* pItem;
        hr = pFileOpen->GetResult(&pItem);
        if (SUCCEEDED(hr)) {
            PWSTR pszFilePath;
            hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
            if (SUCCEEDED(hr)) {
                result = utf16_to_utf8(pszFilePath);
                CoTaskMemFree(pszFilePath);
            }
            pItem->Release();
        }
    }

    pFileOpen->Release();
    return result;
}

std::vector<std::string> show_open_files_dialog(
    const window* parent,
    const char* title,
    const file_dialog_options& options
) {
    com_initializer com;
    IFileOpenDialog* pFileOpen;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

    if (FAILED(hr)) return {};

    pFileOpen->SetTitle(utf8_to_utf16(title).c_str());

    if (!options.filters.empty()) {
        std::vector<COMDLG_FILTERSPEC> specs;
        std::vector<std::wstring> descriptions;
        std::vector<std::wstring> patterns;
        for (const auto& f : options.filters) {
            descriptions.push_back(utf8_to_utf16(f.description));
            patterns.push_back(utf8_to_utf16(f.patterns));
            specs.push_back({descriptions.back().c_str(), patterns.back().c_str()});
        }
        pFileOpen->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
    }

    FILEOPENDIALOGOPTIONS fos;
    pFileOpen->GetOptions(&fos);
    fos |= FOS_ALLOWMULTISELECT;
    if (options.show_hidden) fos |= FOS_FORCESHOWHIDDEN;
    pFileOpen->SetOptions(fos);

    hr = pFileOpen->Show(get_hwnd(parent));
    std::vector<std::string> result;

    if (SUCCEEDED(hr)) {
        IShellItemArray* pItems;
        hr = pFileOpen->GetResults(&pItems);
        if (SUCCEEDED(hr)) {
            DWORD count;
            pItems->GetCount(&count);
            for (DWORD i = 0; i < count; ++i) {
                IShellItem* pItem;
                hr = pItems->GetItemAt(i, &pItem);
                if (SUCCEEDED(hr)) {
                    PWSTR pszFilePath;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                    if (SUCCEEDED(hr)) {
                        result.push_back(utf16_to_utf8(pszFilePath));
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pItems->Release();
        }
    }

    pFileOpen->Release();
    return result;
}

std::optional<std::string> show_save_file_dialog(
    const window* parent,
    const char* title,
    const file_dialog_options& options
) {
    com_initializer com;
    IFileSaveDialog* pFileSave;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_IFileSaveDialog, reinterpret_cast<void**>(&pFileSave));

    if (FAILED(hr)) return std::nullopt;

    pFileSave->SetTitle(utf8_to_utf16(title).c_str());

    if (!options.filters.empty()) {
        std::vector<COMDLG_FILTERSPEC> specs;
        std::vector<std::wstring> descriptions;
        std::vector<std::wstring> patterns;
        for (const auto& f : options.filters) {
            descriptions.push_back(utf8_to_utf16(f.description));
            patterns.push_back(utf8_to_utf16(f.patterns));
            specs.push_back({descriptions.back().c_str(), patterns.back().c_str()});
        }
        pFileSave->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
    }

    if (!options.default_extension.empty()) {
        pFileSave->SetDefaultExtension(utf8_to_utf16(options.default_extension).c_str());
    }

    hr = pFileSave->Show(get_hwnd(parent));
    std::optional<std::string> result;

    if (SUCCEEDED(hr)) {
        IShellItem* pItem;
        hr = pFileSave->GetResult(&pItem);
        if (SUCCEEDED(hr)) {
            PWSTR pszFilePath;
            hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
            if (SUCCEEDED(hr)) {
                result = utf16_to_utf8(pszFilePath);
                CoTaskMemFree(pszFilePath);
            }
            pItem->Release();
        }
    }

    pFileSave->Release();
    return result;
}

std::optional<std::string> show_folder_dialog(
    const window* parent,
    const char* title,
    const char* initial_path
) {
    com_initializer com;
    IFileOpenDialog* pFileOpen;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

    if (FAILED(hr)) return std::nullopt;

    pFileOpen->SetTitle(utf8_to_utf16(title).c_str());

    FILEOPENDIALOGOPTIONS fos;
    pFileOpen->GetOptions(&fos);
    fos |= FOS_PICKFOLDERS;
    pFileOpen->SetOptions(fos);

    if (initial_path && *initial_path) {
        IShellItem* pItem;
        hr = SHCreateItemFromParsingName(utf8_to_utf16(initial_path).c_str(), nullptr, IID_PPV_ARGS(&pItem));
        if (SUCCEEDED(hr)) {
            pFileOpen->SetFolder(pItem);
            pItem->Release();
        }
    }

    hr = pFileOpen->Show(get_hwnd(parent));
    std::optional<std::string> result;

    if (SUCCEEDED(hr)) {
        IShellItem* pItem;
        hr = pFileOpen->GetResult(&pItem);
        if (SUCCEEDED(hr)) {
            PWSTR pszFilePath;
            hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
            if (SUCCEEDED(hr)) {
                result = utf16_to_utf8(pszFilePath);
                CoTaskMemFree(pszFilePath);
            }
            pItem->Release();
        }
    }

    pFileOpen->Release();
    return result;
}

// text_log implementation
struct text_log::impl {
    HWND hwnd = nullptr;
    HWND edit_hwnd = nullptr;
    event_source events;
    bool monospace = true;
    std::wstring title;
    HFONT font = nullptr;

    impl(const char* title_str, bool mono) : monospace(mono), title(utf8_to_utf16(title_str)) {
        register_class();
        hwnd = CreateWindowExW(
            0, L"AliaTextLog", title.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
            nullptr, nullptr, GetModuleHandle(nullptr), this
        );

        edit_hwnd = CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
            0, 0, 0, 0,
            hwnd, nullptr, GetModuleHandle(nullptr), nullptr
        );

        if (monospace) {
            font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier New");
            SendMessage(edit_hwnd, WM_SETFONT, (WPARAM)font, TRUE);
        } else {
            NONCLIENTMETRICSW ncm = { sizeof(NONCLIENTMETRICSW) };
            if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0)) {
                font = CreateFontIndirectW(&ncm.lfMessageFont);
                SendMessage(edit_hwnd, WM_SETFONT, (WPARAM)font, TRUE);
            }
        }
    }

    ~impl() {
        if (hwnd) DestroyWindow(hwnd);
        if (font) DeleteObject(font);
    }

    static void register_class() {
        static bool registered = false;
        if (registered) return;
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = wnd_proc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = L"AliaTextLog";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassExW(&wc);
        registered = true;
    }

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        impl* self = reinterpret_cast<impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = reinterpret_cast<impl*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }

        if (self) {
            switch (msg) {
                case WM_SIZE: {
                    int w = LOWORD(lparam);
                    int h = HIWORD(lparam);
                    MoveWindow(self->edit_hwnd, 0, 0, w, h, TRUE);
                    return 0;
                }
                case WM_CLOSE: {
                    // self->events.post(close_event{}); // Need to define what event to post
                    ShowWindow(hwnd, SW_HIDE);
                    return 0;
                }
            }
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    void append(const char* text) {
        std::wstring wtext = utf8_to_utf16(text);
        // Replace \n with \r\n for Windows EDIT control
        std::wstring processed;
        processed.reserve(wtext.size() * 1.1);
        for (wchar_t c : wtext) {
            if (c == L'\n') processed += L"\r\n";
            else processed += c;
        }

        int len = GetWindowTextLengthW(edit_hwnd);
        SendMessageW(edit_hwnd, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        SendMessageW(edit_hwnd, EM_REPLACESEL, FALSE, (LPARAM)processed.c_str());
        SendMessageW(edit_hwnd, WM_VSCROLL, SB_BOTTOM, 0);
    }

    void clear() {
        SetWindowTextW(edit_hwnd, L"");
    }
};

text_log::text_log(const char* title, bool monospace)
    : impl_(std::make_unique<impl>(title, monospace)) {}

text_log::text_log(text_log&& other) noexcept = default;
text_log& text_log::operator=(text_log&& other) noexcept = default;
text_log::~text_log() = default;

bool text_log::valid() const { return impl_ != nullptr; }

void text_log::append(const char* text) { if (impl_) impl_->append(text); }
void text_log::append(std::string_view text) { 
    if (impl_) {
        std::string s(text);
        impl_->append(s.c_str());
    }
}

void text_log::clear() { if (impl_) impl_->clear(); }
void text_log::show() { if (impl_) ShowWindow(impl_->hwnd, SW_SHOW); }
void text_log::hide() { if (impl_) ShowWindow(impl_->hwnd, SW_HIDE); }
bool text_log::is_visible() const { return impl_ && IsWindowVisible(impl_->hwnd); }

event_source& text_log::get_event_source() {
    if (!impl_) throw std::runtime_error("text_log is not valid");
    return impl_->events;
}

} // namespace alia
