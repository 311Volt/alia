#include "dialog.hpp"
#include "os_context.hpp"
#include <utility>

namespace alia {
    namespace {
        struct dialog_context {
            os_context_handle *handle;
            const os_backend_interface *backend;
        };
        dialog_context context_for(const window *parent) {
            if (parent) {
                if (!parent->handle())
                    throw std::invalid_argument("dialog parent is not valid");
                return {parent->context_handle(), parent->backend()};
            }
            auto &c = get_os_context();
            return {c.handle(), c.backend()};
        }
    } // namespace
    std::optional<std::string> show_open_file_dialog(const window *p, const char *t, const file_dialog_options &o) {
        auto c = context_for(p);
        return c.backend->open_file_dialog.get_or_throw()(c.handle, p, t, o);
    }
    std::vector<std::string> show_open_files_dialog(const window *p, const char *t, const file_dialog_options &o) {
        auto c = context_for(p);
        return c.backend->open_files_dialog.get_or_throw()(c.handle, p, t, o);
    }
    std::optional<std::string> show_save_file_dialog(const window *p, const char *t, const file_dialog_options &o) {
        auto c = context_for(p);
        return c.backend->save_file_dialog.get_or_throw()(c.handle, p, t, o);
    }
    std::optional<std::string> show_folder_dialog(const window *p, const char *t, const char *path) {
        auto c = context_for(p);
        return c.backend->folder_dialog.get_or_throw()(c.handle, p, t, path);
    }
    message_box_result show_message_box(const char *t, const char *m, message_box_buttons b, message_box_icon i, const window *p) {
        auto c = context_for(p);
        return c.backend->message_box(c.handle, t, m, b, i, p);
    }
    void show_info_box(const char *t, const char *m, const window *p) {
        (void)show_message_box(t, m, message_box_buttons::ok, message_box_icon::info, p);
    }
    void show_warning_box(const char *t, const char *m, const window *p) {
        (void)show_message_box(t, m, message_box_buttons::ok, message_box_icon::warning, p);
    }
    void show_error_box(const char *t, const char *m, const window *p) {
        (void)show_message_box(t, m, message_box_buttons::ok, message_box_icon::error, p);
    }
    bool show_confirm_box(const char *t, const char *m, const window *p) {
        return show_message_box(t, m, message_box_buttons::yes_no, message_box_icon::question, p) == message_box_result::yes;
    }
    text_log::text_log(const char *title, bool mono) : text_log(get_os_context(), title, mono) {
    }
    text_log::text_log(os_context &c, const char *title, bool mono) {
        if (!c)
            throw std::invalid_argument("text_log requires a valid OS context");
        backend_ = c.backend();
        handle_ = backend_->create_text_log(c.handle(), title, mono);
        if (!handle_)
            throw std::runtime_error("text_log: native window creation failed");
    }
    text_log::~text_log() {
        if (handle_)
            backend_->destroy_text_log(handle_);
    }
    text_log::text_log(text_log &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)), backend_(other.backend_) {
    }
    text_log &text_log::operator=(text_log &&other) noexcept {
        if (this != &other) {
            if (handle_)
                backend_->destroy_text_log(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
            backend_ = other.backend_;
        }
        return *this;
    }
    bool text_log::valid() const {
        return handle_ != nullptr;
    }
    void text_log::append(const char *text) {
        append(std::string_view(text ? text : ""));
    }
    void text_log::append(std::string_view text) {
        if (handle_)
            backend_->text_log_append(handle_, text);
    }
    void text_log::clear() {
        if (handle_)
            backend_->text_log_clear(handle_);
    }
    void text_log::show() {
        if (handle_)
            backend_->text_log_show(handle_, true);
    }
    void text_log::hide() {
        if (handle_)
            backend_->text_log_show(handle_, false);
    }
    bool text_log::is_visible() const {
        return handle_ && backend_->text_log_is_visible(handle_);
    }
    event_source &text_log::get_event_source() {
        if (!handle_)
            throw std::logic_error("text_log is not valid");
        return backend_->text_log_event_source(handle_);
    }
} // namespace alia
