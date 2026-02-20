#ifndef CLIPBOARD_F946DCE3_FD92_463B_A506_B3ABF2DE26BD
#define CLIPBOARD_F946DCE3_FD92_463B_A506_B3ABF2DE26BD

#include <string>
#include <string_view>

namespace alia {

// Clipboard operations
namespace clipboard {
    // Text
    [[nodiscard]] bool has_text();
    [[nodiscard]] std::string get_text();
    bool set_text(std::string_view text);
    
    // Check if clipboard has image data
    [[nodiscard]] bool has_image();
    
    // Clear clipboard
    void clear();
}

} // namespace alia

#endif /* CLIPBOARD_F946DCE3_FD92_463B_A506_B3ABF2DE26BD */
