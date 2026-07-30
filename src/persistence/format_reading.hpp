#pragma once

#include <cctype>
#include <cstddef>
#include <istream>
#include <string>

// Small bounded-token helpers for human-readable persistence formats. Numeric
// extraction remains stream-based, while labels are constrained before an
// attacker-controlled token can grow an unbounded std::string.
namespace FormatReading {

const std::size_t kMaximumTokenBytes = 64u;

inline bool ReadToken(std::istream& input, std::string& value,
                      std::size_t maximumBytes = kMaximumTokenBytes) {
    value.clear();
    input >> std::ws;
    if (input.fail()) return false;

    while (true) {
        const int next = input.peek();
        if (next == std::char_traits<char>::eof()) break;
        if (std::isspace(static_cast<unsigned char>(next))) break;
        if (value.size() >= maximumBytes) {
            input.setstate(std::ios::failbit);
            return false;
        }
        value += static_cast<char>(input.get());
    }
    return !value.empty();
}

} // namespace FormatReading
