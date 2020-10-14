#ifndef GEAROENIX_CORE_STRING_HPP
#define GEAROENIX_CORE_STRING_HPP

#include "event/gx-cr-ev-button.hpp"
#include "gx-cr-language.hpp"
#include <optional>
#include <string>

#ifdef GX_IN_IOS
#import <Foundation/Foundation.h>
#endif

namespace gearoenix::core {
class String {
public:
    [[nodiscard]] static bool is_character(event::button::KeyboardKeyId) noexcept;
    [[nodiscard]] static std::optional<wchar_t> to_character(event::button::KeyboardKeyId, bool shift_pressed = false, Language language = Language::English) noexcept;
    [[nodiscard]] static std::string to_string(const std::wstring& s) noexcept;

#ifdef GX_IN_IOS
    [[nodiscard]] static NSString* to_objc_string(const std::string& s) noexcept;
    [[nodiscard]] static NSString* to_objc_string(const char* s) noexcept;
    [[nodiscard]] static std::string join_path(const NSString* dir, const std::string& s) noexcept;
    [[nodiscard]] static std::string join_path(const NSString* dir, const char* s) noexcept;
    [[nodiscard]] static std::string join_path(const NSString* dir, const NSString* s) noexcept;
#endif
};
}
#endif
