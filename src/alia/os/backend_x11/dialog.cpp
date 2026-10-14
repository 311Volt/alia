#include "x11_ops.hpp"
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_X11
#include <X11/keysym.h>
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

extern char **environ;
namespace alia {
namespace {
std::optional<std::string> run_zenity(x11_context &c, std::vector<std::string> arguments) {
    // Direct argv preserves paths, titles and filters without shell expansion.
    arguments.insert(arguments.begin(), c.zenity_path);
    std::vector<char *> argv; for (auto &arg : arguments) argv.push_back(arg.data()); argv.push_back(nullptr);
    int pipefd[2]; if (::pipe(pipefd) != 0) throw std::runtime_error("X11 dialog: pipe failed");
    struct pipe_owner { int read, write; ~pipe_owner() { if (read >= 0) close(read); if (write >= 0) close(write); } } pipe{pipefd[0], pipefd[1]};
    fcntl(pipe.read, F_SETFD, FD_CLOEXEC); fcntl(pipe.write, F_SETFD, FD_CLOEXEC);
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) throw std::runtime_error("X11 dialog: spawn setup failed");
    struct action_owner { posix_spawn_file_actions_t *a; ~action_owner() { posix_spawn_file_actions_destroy(a); } } action_cleanup{&actions};
    if (posix_spawn_file_actions_adddup2(&actions, pipe.write, STDOUT_FILENO) ||
        posix_spawn_file_actions_addclose(&actions, pipe.read) || posix_spawn_file_actions_addclose(&actions, pipe.write))
        throw std::runtime_error("X11 dialog: spawn pipe setup failed");
    pid_t child; const int error = posix_spawn(&child, c.zenity_path.c_str(), &actions, nullptr, argv.data(), environ);
    close(pipe.write); pipe.write = -1;
    if (error) throw std::runtime_error("X11 dialog: cannot launch zenity");
    struct child_owner {
        pid_t pid;
        ~child_owner() { if (pid > 0) { kill(pid, SIGTERM); while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {} } }
    } child_cleanup{child};
    fcntl(pipe.read, F_SETFL, fcntl(pipe.read, F_GETFL) | O_NONBLOCK);
    std::string output; int status = 0; bool exited = false, eof = false, oversized = false;
    // Continue servicing clipboard requests and window events while the dialog
    // process is open; no backend worker thread owns this connection.
    while (!exited || !eof) {
        char buffer[4096]; ssize_t count;
        while ((count = read(pipe.read, buffer, sizeof(buffer))) > 0) {
            if (output.size() + static_cast<std::size_t>(count) <= 16 * 1024 * 1024) output.append(buffer, static_cast<std::size_t>(count)); else oversized = true;
        }
        if (count == 0) eof = true;
        if (!exited) {
            const auto result = waitpid(child, &status, WNOHANG);
            if (result == child) { exited = true; child_cleanup.pid = -1; }
            else if (result < 0 && errno != EINTR) { exited = true; child_cleanup.pid = -1; status = 1 << 8; }
        }
        if (!exited || !eof) {
            x11_poll(&c);
            pollfd fds[] = {{pipe.read, POLLIN, 0}, {ConnectionNumber(c.display), POLLIN, 0}};
            ::poll(fds, 2, 20);
        }
    }
    if (oversized) throw std::length_error("X11 dialog output exceeds 16 MiB");
    if (!WIFEXITED(status) || WEXITSTATUS(status) > 1) throw std::runtime_error("X11 dialog service failed");
    if (WEXITSTATUS(status) == 1) return std::nullopt;
    // Zenity appends a single newline, which is separate from the filename.
    if (!output.empty() && output.back() == '\n') output.pop_back();
    return output;
}
std::vector<std::string> file_arguments(const window *parent, const char *title, const file_dialog_options &options) {
    std::vector<std::string> args{"--file-selection", std::string("--title=") + (title ? title : "")};
    if (parent) args.push_back("--attach=" + std::to_string(reinterpret_cast<std::uintptr_t>(parent->native_handle())));
    if (!options.initial_path.empty()) args.push_back("--filename=" + options.initial_path);
    if (options.show_hidden) throw unsupported_operation_exception("The Zenity file-picker service cannot force hidden files to be shown; use its hidden-files toggle");
    for (const auto &filter : options.filters) {
        auto patterns = filter.patterns; std::replace(patterns.begin(), patterns.end(), ';', ' ');
        args.push_back("--file-filter=" + filter.description + " | " + patterns);
    }
    return args;
}
XFontSet create_font(x11_context &c, bool mono) {
    char **missing = nullptr; int count = 0; char *fallback = nullptr;
    auto font = XCreateFontSet(c.display, mono ? "-misc-fixed-*-*-*-*-14-*-*-*-*-*-*-*" : "-*-helvetica-*-*-*-*-14-*-*-*-*-*-*-*", &missing, &count, &fallback);
    if (missing) XFreeStringList(missing);
    if (!font) { missing = nullptr; font = XCreateFontSet(c.display, "fixed", &missing, &count, &fallback); if (missing) XFreeStringList(missing); }
    return font;
}
std::vector<std::string_view> lines(std::string_view text) {
    std::vector<std::string_view> result;
    do { const auto end = text.find('\n'); result.push_back(text.substr(0, end)); if (end == text.npos) break; text.remove_prefix(end + 1); } while (true);
    return result;
}
void draw_line(x11_context &c, ::Window w, GC gc, XFontSet font, int x, int y, std::string_view text) {
    if (font) Xutf8DrawString(c.display, w, font, gc, x, y, text.data(), static_cast<int>(text.size()));
    else XDrawString(c.display, w, gc, x, y, text.data(), static_cast<int>(text.size()));
}
}
std::optional<std::string> x11_open_file_dialog(os_context_handle *h, const window *parent, const char *title, const file_dialog_options &options) {
    return run_zenity(*as_x11_context(h), file_arguments(parent, title, options));
}
std::vector<std::string> x11_open_files_dialog(os_context_handle *h, const window *parent, const char *title, const file_dialog_options &options) {
    auto args = file_arguments(parent, title, options); args.push_back("--multiple"); args.push_back("--separator=\x1f");
    auto text = run_zenity(*as_x11_context(h), std::move(args)); std::vector<std::string> result;
    if (!text) return result;
    std::string_view remaining(*text);
    while (!remaining.empty()) { const auto end = remaining.find('\x1f'); result.emplace_back(remaining.substr(0, end)); if (end == remaining.npos) break; remaining.remove_prefix(end + 1); }
    return result;
}
std::optional<std::string> x11_save_file_dialog(os_context_handle *h, const window *parent, const char *title, const file_dialog_options &options) {
    auto args = file_arguments(parent, title, options); args.push_back("--save"); args.push_back("--confirm-overwrite");
    auto result = run_zenity(*as_x11_context(h), std::move(args));
    if (result && !options.default_extension.empty()) {
        const auto slash = result->find_last_of('/'), dot = result->find_last_of('.');
        if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
            *result += (options.default_extension.front() == '.' ? "" : ".") + options.default_extension;
            std::error_code error;
            if (std::filesystem::exists(*result, error)) {
                const std::string message = "The file " + *result + " already exists. Replace it?";
                if (x11_message_box(h, "Replace file", message.c_str(), message_box_buttons::yes_no, message_box_icon::question, parent) != message_box_result::yes) return std::nullopt;
            }
        }
    }
    return result;
}
std::optional<std::string> x11_folder_dialog(os_context_handle *h, const window *parent, const char *title, const char *path) {
    file_dialog_options options; if (path) options.initial_path = path;
    auto args = file_arguments(parent, title, options); args.push_back("--directory");
    return run_zenity(*as_x11_context(h), std::move(args));
}
message_box_result x11_message_box(os_context_handle *h, const char *title, const char *message, message_box_buttons buttons, message_box_icon icon, const window *parent) {
    auto &c = *as_x11_context(h);
    using choice = std::pair<const char *, message_box_result>;
    std::vector<choice> choices;
    switch (buttons) {
    case message_box_buttons::ok: choices = {{"OK", message_box_result::ok}}; break;
    case message_box_buttons::ok_cancel: choices = {{"OK", message_box_result::ok}, {"Cancel", message_box_result::cancel}}; break;
    case message_box_buttons::yes_no: choices = {{"Yes", message_box_result::yes}, {"No", message_box_result::no}}; break;
    case message_box_buttons::yes_no_cancel: choices = {{"Yes", message_box_result::yes}, {"No", message_box_result::no}, {"Cancel", message_box_result::cancel}}; break;
    case message_box_buttons::retry_cancel: choices = {{"Retry", message_box_result::retry}, {"Cancel", message_box_result::cancel}}; break;
    case message_box_buttons::abort_retry_ignore: choices = {{"Abort", message_box_result::abort}, {"Retry", message_box_result::retry}, {"Ignore", message_box_result::ignore}}; break;
    }
    if (choices.empty()) throw std::invalid_argument("invalid message box button configuration");
    const auto text_lines = lines(message ? message : "");
    const int width = 600, height = std::clamp(static_cast<int>(text_lines.size()) * 20 + 90, 140, 800);
    const auto w = XCreateSimpleWindow(c.display, c.root, 100, 100, width, height, 1, BlackPixel(c.display, c.screen), WhitePixel(c.display, c.screen));
    if (!w) throw std::runtime_error("X11 message box creation failed");
    struct resources {
        x11_context &c; ::Window w; GC gc; XFontSet font;
        ~resources() { if (font) XFreeFontSet(c.display, font); if (gc) XFreeGC(c.display, gc); if (w) XDestroyWindow(c.display, w); XFlush(c.display); }
    } owned{c, w, XCreateGC(c.display, w, 0, nullptr), create_font(c, false)};
    XStoreName(c.display, w, title ? title : "");
    if (parent) XSetTransientForHint(c.display, w, static_cast<::Window>(reinterpret_cast<std::uintptr_t>(parent->native_handle())));
    XSetWMProtocols(c.display, w, &c.wm_delete, 1);
    const Atom type = c.atom("_NET_WM_WINDOW_TYPE_DIALOG"); XChangeProperty(c.display, w, c.atom("_NET_WM_WINDOW_TYPE"), XA_ATOM, 32, PropModeReplace, reinterpret_cast<const unsigned char *>(&type), 1);
    const Atom modal = c.atom("_NET_WM_STATE_MODAL"); XChangeProperty(c.display, w, c.atom("_NET_WM_STATE"), XA_ATOM, 32, PropModeReplace, reinterpret_cast<const unsigned char *>(&modal), 1);
    XSizeHints hints{}; hints.flags = PMinSize | PMaxSize; hints.min_width = hints.max_width = width; hints.min_height = hints.max_height = height; XSetWMNormalHints(c.display, w, &hints);
    XSelectInput(c.display, w, ExposureMask | ButtonReleaseMask | KeyPressMask | StructureNotifyMask);
    XMapRaised(c.display, w); XFlush(c.display);
    const int button_width = 110, start_x = (width - static_cast<int>(choices.size()) * button_width) / 2;
    const auto close_result = buttons == message_box_buttons::ok ? message_box_result::ok : buttons == message_box_buttons::yes_no ? message_box_result::no : buttons == message_box_buttons::abort_retry_ignore ? message_box_result::abort : message_box_result::cancel;
    std::size_t selected = 0;
    const auto redraw = [&] { XClearArea(c.display, w, 0, 0, 0, 0, True); XFlush(c.display); };
    for (;;) {
        XEvent event; XNextEvent(c.display, &event);
        if (event.xany.window != w) { x11_dispatch(c, event); continue; }
        if (event.type == DestroyNotify) { owned.w = None; return close_result; }
        if (event.type == Expose) {
            XSetForeground(c.display, owned.gc, BlackPixel(c.display, c.screen));
            const char *symbol = icon == message_box_icon::error ? "Error" : icon == message_box_icon::warning ? "Warning" : icon == message_box_icon::question ? "Question" : icon == message_box_icon::info ? "Information" : "";
            draw_line(c, w, owned.gc, owned.font, 16, 22, symbol);
            int y = 45; for (auto line : text_lines) { if (y > height - 60) break; draw_line(c, w, owned.gc, owned.font, 16, y, line); y += 20; }
            for (std::size_t i = 0; i < choices.size(); ++i) {
                const int x = start_x + static_cast<int>(i) * button_width;
                XDrawRectangle(c.display, w, owned.gc, x, height - 45, button_width - 10, 28);
                if (i == selected) XDrawRectangle(c.display, w, owned.gc, x + 2, height - 43, button_width - 14, 24);
                draw_line(c, w, owned.gc, owned.font, x + 16, height - 25, choices[i].first);
            }
        } else if (event.type == ButtonRelease && event.xbutton.button == Button1 && event.xbutton.y >= height - 45 && event.xbutton.y <= height - 17) {
            const int x = event.xbutton.x - start_x;
            if (x >= 0 && x < static_cast<int>(choices.size()) * button_width && x % button_width < button_width - 10)
                return choices[static_cast<std::size_t>(x / button_width)].second;
        } else if (event.type == KeyPress) {
            const auto key = XLookupKeysym(&event.xkey, 0);
            if (key == XK_Return || key == XK_KP_Enter || key == XK_space) return choices[selected].second;
            if (key == XK_Escape) return close_result;
            if (key == XK_Tab || key == XK_Right) { selected = (selected + 1) % choices.size(); redraw(); }
            if (key == XK_ISO_Left_Tab || key == XK_Left) { selected = (selected + choices.size() - 1) % choices.size(); redraw(); }
        } else if (event.type == ClientMessage && event.xclient.message_type == c.wm_protocols && static_cast<Atom>(event.xclient.data.l[0]) == c.wm_delete) return close_result;
    }
}
x11_text_log::~x11_text_log() {
    if (font) XFreeFontSet(context->display, font);
    if (gc) XFreeGC(context->display, gc);
    if (xid) { context->logs.erase(xid); XDestroyWindow(context->display, xid); } XFlush(context->display);
}
os_text_log_handle *x11_create_text_log(os_context_handle *h, const char *title, bool mono) {
    auto &c = *as_x11_context(h); auto result = std::make_unique<x11_text_log>(); auto &log = *result; log.context = &c;
    log.xid = XCreateSimpleWindow(c.display, c.root, 100, 100, log.width, log.height, 1, BlackPixel(c.display, c.screen), WhitePixel(c.display, c.screen));
    if (!log.xid) return nullptr;
    log.gc = XCreateGC(c.display, log.xid, 0, nullptr); log.font = create_font(c, mono);
    XStoreName(c.display, log.xid, title ? title : ""); XSetWMProtocols(c.display, log.xid, &c.wm_delete, 1);
    XSelectInput(c.display, log.xid, ExposureMask | StructureNotifyMask); c.logs.emplace(log.xid, &log);
    return result.release();
}
void x11_draw_text_log(x11_text_log &log) {
    auto &c = *log.context; XClearWindow(c.display, log.xid); XSetForeground(c.display, log.gc, BlackPixel(c.display, c.screen));
    const auto text_lines = lines(log.text); const std::size_t visible = static_cast<std::size_t>(std::max(1, (log.height - 10) / 18));
    const auto start = text_lines.size() > visible ? text_lines.size() - visible : 0;
    int y = 20; for (std::size_t i = start; i < text_lines.size(); ++i) { draw_line(c, log.xid, log.gc, log.font, 8, y, text_lines[i]); y += 18; }
    XFlush(c.display);
}
}
#endif
