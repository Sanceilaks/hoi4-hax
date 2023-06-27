#include "memory.hpp"

#include <Windows.h>

inline std::uint8_t* pattern_scanner(uint8_t* begin, uint8_t* end, const std::string_view signature) noexcept {
    static auto pattern_to_byte = [](const std::string_view pattern) {
        auto bytes = std::vector<int>();
        auto* const start = const_cast<char*>(pattern.data());
        auto* const end = const_cast<char*>(pattern.data()) + std::strlen(pattern.data());

        for (auto* current = start; current < end; ++current) {
            if (*current == '?') {
                ++current;

                if (*current == '?')
                    ++current;

                bytes.push_back(-1);
            } else {
                bytes.push_back(std::strtoul(current, &current, 16));
            }
        }
        return bytes;
    };


    const auto size_of_image = (uintptr_t)end - (uintptr_t)begin;
    auto pattern_bytes = pattern_to_byte(signature);
    auto* const scan_bytes = reinterpret_cast<std::uint8_t*>(begin);

    const auto s = pattern_bytes.size();
    auto* const d = pattern_bytes.data();

    for (auto i = 0ul; i < size_of_image - s; ++i) {
        auto found = true;

        for (auto j = 0ul; j < s; ++j) {
            if (scan_bytes[i + j] != d[j] && d[j] != -1) {
                found = false;
                break;
            }
        }
        if (found) {
            return &scan_bytes[i];
        }
    }

    return nullptr;
}

memory::Address<> memory::MemoryScanner::scan_impl(uint8_t* begin, uint8_t* end) const {
    return Address<>(pattern_scanner(begin, end, this->pattern));
}

memory::MemoryModule memory::MemoryModule::get_module_by_name(std::string_view name) {
    auto* handle = GetModuleHandle(name.data());
    return handle;
}