#pragma once

#include <optional>
#include <string>
#include <vector>

namespace mtfind {

inline constexpr std::size_t maxMaskLength = 100000;

struct MaskSegment {
    std::size_t pos = 0;      // position of the segment start in the mask
    std::string literal;      // literal characters (no '?')
};

struct Mask {
    std::size_t length = 0;
    std::vector<MaskSegment> segments;  

    bool all_wildcards() const noexcept { return segments.empty(); }
};

struct Match {
    std::uint64_t line = 0; 
    std::uint64_t col = 0; 
    std::string text;        
};

std::optional<std::string> validate_mask(std::string_view mask);

Mask parse_mask(std::string_view mask);

std::size_t search_chunk(std::string_view data, const Mask& mask, std::vector<Match>& out);

}  // namespace mtfind