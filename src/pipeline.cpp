#include "pipeline.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <ranges>

namespace mtfind {

namespace {

void push_pills(BlockingQueue<std::optional<Chunk>>& chunks, std::size_t workers) {
    std::ranges::for_each(std::views::iota(std::size_t{0}, workers),
                          [&](std::size_t) { chunks.push(std::nullopt); });
}

}  // namespace

void producer_main(std::string_view path, BlockingQueue<std::optional<Chunk>>& chunks,
                   std::size_t workers) {
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file) {
        std::cerr << "mtfind: cannot open file '" << path << "'\n";
        push_pills(chunks, workers);
        return;
    }

    const auto fail = [&] {
        std::cerr << "mtfind: read failed\n";
    };

    std::array<char, TailBytes> tail;
    std::size_t seq = 0;
    for (;;) {
        Chunk chunk;
        chunk.seq = seq++;
        chunk.data.resize(ChunkSize);
        file.read(chunk.data.data(), static_cast<std::streamsize>(ChunkSize));
        const std::size_t n = static_cast<std::size_t>(file.gcount());
        if (n == 0) {
            if (file.eof()) {
                break;  // EOF
            }
            fail();
            break;
        }
        chunk.data.resize(n);

        // Extend the chunk to the end of the current line so that chunk
        // boundaries always fall on newlines.
        while (chunk.data.back() != '\n') {
            file.read(tail.data(), static_cast<std::streamsize>(tail.size()));
            const std::size_t m = static_cast<std::size_t>(file.gcount());
            if (m == 0) {
                if (!file.eof()) {
                    fail();
                }
                break;
            }
            chunk.data.append(tail.data(), m);
        }

        chunks.push(std::move(chunk));
    }

    push_pills(chunks, workers);
}

void worker_main(const Mask& mask, BlockingQueue<std::optional<Chunk>>& chunks,
                 BlockingQueue<ChunkResult>& results) {
    for (;;) {
        std::optional<Chunk> chunk = chunks.pop();
        if (!chunk) {
            results.push(ChunkResult{});  // DONE marker
            return;
        }
        ChunkResult result;
        result.seq = chunk->seq;
        result.lines = search_chunk(chunk->data, mask, result.matches);
        results.push(std::move(result));
    }
}

std::size_t collector_main(BlockingQueue<ChunkResult>& results, std::size_t workers,
                           std::vector<Match>& buffer) {
    std::size_t next_seq = 0;
    std::size_t done = 0;
    std::uint64_t prefix = 0;  // lines completed before the next chunk
    std::map<std::size_t, ChunkResult> pending;

    const auto drain = [&]() {
        auto it = pending.find(next_seq);
        while (it != pending.end()) {
            std::ranges::transform(it->second.matches, std::back_inserter(buffer),
                                   [&](Match& match) {
                                       match.line += prefix;
                                       return std::move(match);
                                   });
            prefix += it->second.lines;
            pending.erase(it);
            ++next_seq;
            it = pending.find(next_seq);
        }
    };

    for (;;) {
        ChunkResult result = results.pop();
        if (result.seq == ChunkResult::Done) {
            ++done;
            if (done == workers) {
                break;
            }
            continue;
        }
        pending.emplace(result.seq, std::move(result));
        drain();
    }
    drain();
    return buffer.size();
}

}  // namespace mtfind