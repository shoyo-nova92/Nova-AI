#pragma once
#include <string>
#include <string_view>
#include <stdexcept>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace Utf8 {
class Error : public std::runtime_error {
public:
    Error(const std::string& field, std::size_t offset)
        : std::runtime_error("Invalid UTF-8 in " + field + " at byte " + std::to_string(offset)) {}
};
inline std::size_t invalidOffset(std::string_view value) {
    for (std::size_t i = 0; i < value.size();) {
        const auto c = static_cast<unsigned char>(value[i]);
        const unsigned n = c < 0x80 ? 1 : c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
        if (!n || i + n > value.size()) return i;
        for (unsigned j = 1; j < n; ++j)
            if ((static_cast<unsigned char>(value[i+j]) & 0xc0) != 0x80) return i+j;
        if (n >= 3) {
            const auto b = static_cast<unsigned char>(value[i+1]);
            if ((c == 0xe0 && b < 0xa0) || (c == 0xed && b >= 0xa0) ||
                (c == 0xf0 && b < 0x90) || (c == 0xf4 && b >= 0x90)) return i;
        }
        i += n;
    }
    return std::string_view::npos;
}
inline void require(std::string_view value, const std::string& field) {
    const auto offset = invalidOffset(value);
    if (offset != std::string_view::npos) throw Error(field, offset);
}
inline std::string truncate(std::string_view value, std::size_t bytes, const std::string& field = "text") {
    require(value, field);
    if (value.size() <= bytes) return std::string(value);
    while (bytes && (static_cast<unsigned char>(value[bytes]) & 0xc0) == 0x80) --bytes;
    return std::string(value.substr(0, bytes));
}
inline void validate(const nlohmann::json& value, const std::string& field) {
    if (value.is_string()) require(value.get_ref<const std::string&>(), field);
    else if (value.is_array()) {
        for (std::size_t i=0; i<value.size(); ++i) validate(value[i], field + "[" + std::to_string(i) + "]");
    } else if (value.is_object()) {
        for (auto it=value.begin(); it!=value.end(); ++it) {
            require(it.key(), field + ".key");
            validate(it.value(), field + "." + it.key());
        }
    }
}
inline std::string dump(const nlohmann::json& value, const std::string& field, int indent=-1) {
    validate(value, field);
    return value.dump(indent);
}
inline std::string path(const std::filesystem::path& value) {
    const auto text = value.u8string();
    return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}
inline std::filesystem::path toPath(const std::string& value) {
    require(value,"filesystem.path");
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(value.data()),value.size()));
}
}
