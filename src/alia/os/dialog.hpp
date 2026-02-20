#ifndef DIALOG_D0E88B7A_0F29_490F_B4E5_520555722B09
#define DIALOG_D0E88B7A_0F29_490F_B4E5_520555722B09

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <format>
#include "../events/event_source.hpp"

namespace alia {

class window;

// File dialog filter
struct file_filter {
    std::string description;     // e.g., "Image Files"
    std::string patterns;        // e.g., "*.png;*.jpg;*.bmp"
};

// File dialog options
struct file_dialog_options {
    std::vector<file_filter> filters;
    std::string initial_path;
    std::string default_extension;
    bool allow_multiple = false;  // For open dialogs
    bool show_hidden = false;
};

// Open file dialog
[[nodiscard]] std::optional<std::string> show_open_file_dialog(
    const window* parent = nullptr,
    const char* title = "Open File",
    const file_dialog_options& options = {}
);

// Open multiple files dialog
[[nodiscard]] std::vector<std::string> show_open_files_dialog(
    const window* parent = nullptr,
    const char* title = "Open Files",
    const file_dialog_options& options = {}
);

// Save file dialog
[[nodiscard]] std::optional<std::string> show_save_file_dialog(
    const window* parent = nullptr,
    const char* title = "Save File",
    const file_dialog_options& options = {}
);

// Folder selection dialog
[[nodiscard]] std::optional<std::string> show_folder_dialog(
    const window* parent = nullptr,
    const char* title = "Select Folder",
    const char* initial_path = nullptr
);

// Message box button configurations
enum class message_box_buttons {
    ok,
    ok_cancel,
    yes_no,
    yes_no_cancel,
    retry_cancel,
    abort_retry_ignore,
};

// Message box icons
enum class message_box_icon {
    none,
    info,
    warning,
    error,
    question,
};

// Message box result
enum class message_box_result {
    ok,
    cancel,
    yes,
    no,
    retry,
    abort,
    ignore,
};

// Show message box
[[nodiscard]] message_box_result show_message_box(
    const char* title,
    const char* message,
    message_box_buttons buttons = message_box_buttons::ok,
    message_box_icon icon = message_box_icon::none,
    const window* parent = nullptr
);

// Convenience functions
void show_info_box(const char* title, const char* message, 
                   const window* parent = nullptr);

void show_warning_box(const char* title, const char* message,
                      const window* parent = nullptr);

void show_error_box(const char* title, const char* message,
                    const window* parent = nullptr);

[[nodiscard]] bool show_confirm_box(const char* title, const char* message,
                                     const window* parent = nullptr);

// Text log window (for debug output)
class text_log {
public:
    text_log() = default;
    text_log(const char* title, bool monospace = true);
    
    text_log(text_log&&) noexcept;
    text_log& operator=(text_log&&) noexcept;
    ~text_log();
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Append text
    void append(const char* text);
    void append(std::string_view text);
    
    template<typename... Args>
    void printf(const char* fmt, Args&&... args) {
        append(std::format(fmt, std::forward<Args>(args)...));
    }
    
    // Clear contents
    void clear();
    
    // Show/hide
    void show();
    void hide();
    [[nodiscard]] bool is_visible() const;
    
    // Get event source for close events
    event_source& get_event_source();
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace alia

#endif /* DIALOG_D0E88B7A_0F29_490F_B4E5_520555722B09 */
