#pragma once

#include "queue.hpp"
#include "search.hpp"

namespace mtfind {

inline constexpr std::size_t ChunkSize = 1024 * 1024;  // producer reads ~1 MiB per chunk
inline constexpr std::size_t TailBytes = 64 * 1024;     // tail buffer for line-aligned cut

struct Chunk {
    std::size_t seq = 0;  
    std::string data;     
};

struct ChunkResult {
    static constexpr std::size_t Done = static_cast<std::size_t>(-1);

    std::size_t seq = Done;    
    std::vector<Match> matches;
    std::size_t lines = 0;     
};

void producer_main(std::string_view path, BlockingQueue<std::optional<Chunk>>& chunks,
                   std::size_t workers);

void worker_main(const Mask& mask, BlockingQueue<std::optional<Chunk>>& chunks,
                 BlockingQueue<ChunkResult>& results);

std::size_t collector_main(BlockingQueue<ChunkResult>& results, std::size_t workers,
                           std::vector<Match>& buffer);

}  // namespace mtfind