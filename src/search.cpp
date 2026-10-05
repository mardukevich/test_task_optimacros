#include "search.hpp"

#include <algorithm>
#include <ranges>

namespace {

// First byte outside the 7-bit ASCII range.
inline constexpr std::string_view NonAsciiThreshold = "\x80";

}  // namespace

namespace mtfind {

std::optional<std::string> validate_mask(std::string_view mask) {
    if (mask.empty()) {
        return "mask must not be empty";
    }
    if (mask.size() > maxMaskLength) {
        return "mask length exceeds 100000 characters";
    }
    if (mask.find('\n') != std::string_view::npos) {
        return "mask must not contain newline characters";
    }
    if (std::ranges::any_of(mask, [](unsigned char c) {
            return c >= static_cast<unsigned char>(NonAsciiThreshold.front());
        })) {
        return "mask must contain only 7-bit ASCII characters";
    }
    return std::nullopt;
}

Mask parse_mask(std::string_view mask) {
    Mask result;
    result.length = mask.size();
    std::size_t pos = 0;
    for (const auto part : mask | std::views::split('?')) {
        if (!part.empty()) {
            std::string literal;
            std::ranges::copy(part, std::back_inserter(literal));
            result.segments.push_back(MaskSegment{pos, std::move(literal)});
        }
        pos += std::ranges::distance(part) + 1;
    }
    return result;
}

std::size_t search_chunk(std::string_view data, const Mask& mask, std::vector<Match>& out) {
    out.clear();
    const std::size_t mask_len = mask.length;
    if (mask_len > data.size()) {
        return std::ranges::count(data, '\n');
    }

    // Cursor over the chunk: (pos, lines, col) with col 1-based, 1 at a line start.
    std::size_t lines = 0;
    std::size_t col = 1;
    std::size_t pos = 0;
    const auto advance = [&](std::size_t target) {
        for (const char c : data.substr(pos, target - pos)) {
            if (c == '\n') {
                ++lines;
                col = 1;
            } else {
                ++col;
            }
        }
        pos = target;
    };

    // Verifies the whole mask at candidate position cand.
    // '?' never matches '\n', so a window containing a newline fails immediately.
    const auto verify = [&](std::size_t cand) {
        if (cand + mask_len > data.size()) {
            return false;
        }
        const std::string_view window = data.substr(cand, mask_len);
        if (window.find('\n') != std::string_view::npos) {
            return false;
        }
        return std::ranges::none_of(mask.segments, [&](const MaskSegment& seg) {
            return window.substr(seg.pos, seg.literal.size()) != seg.literal;
        });
    };

    if (mask.all_wildcards()) {
        // Every position matches as long as the window has no newline.
        std::size_t scan = 0;
        while (scan + mask_len <= data.size()) {
            const std::size_t nl = data.find('\n', scan);
            if (nl < scan + mask_len) {
                scan = nl + 1;
                continue;
            }
            advance(scan);
            out.push_back(Match{lines + 1, col, std::string(data.substr(scan, mask_len))});
            scan += mask_len;
        }
        advance(data.size());
        return lines;
    }

    // Anchor: the longest literal segment. Only positions where the anchor occurs
    // can be matches, so candidates are verified sparingly.
    const auto anchor_it = std::ranges::max_element(
        mask.segments, std::ranges::less{},
        [](const MaskSegment& seg) { return seg.literal.size(); });
    const MaskSegment& anchor = *anchor_it;

    std::size_t match_min = 0;  // first allowed match start (non-overlap)
    std::size_t scan = anchor.pos;
    const std::size_t data_size = data.size();
    while (scan + anchor.literal.size() <= data_size) {
        const std::size_t a = data.find(anchor.literal.front(), scan);
        if (a == std::string_view::npos) {
            break;
        }
        if (data.substr(a, anchor.literal.size()) != anchor.literal) {
            scan = a + 1;
            continue;
        }
        const std::size_t cand = a - anchor.pos;
        if (cand < match_min) {
            scan = a + 1;
            continue;
        }
        if (verify(cand)) {
            advance(cand);
            out.push_back(Match{lines + 1, col, std::string(data.substr(cand, mask_len))});
            match_min = cand + mask_len;
            scan = match_min + anchor.pos;
            continue;
        }
        scan = a + 1;
    }
    advance(data_size);
    return lines;
}

}  // namespace mtfind