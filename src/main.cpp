#include <algorithm>
#include <filesystem>
#include <iostream>
#include <ranges>

#include "pipeline.hpp"

namespace {

std::string usage_text() {
    return "Usage: mtfind <file> <mask>\n"
           "Search for non-overlapping occurrences of a mask ('?' = any char) in a text file.\n"
           "\n"
           "  <file>  path to the text file (7-bit ASCII, up to 1 TB)\n"
           "  <mask>  search mask, up to 100000 characters\n";
}

std::optional<std::pair<std::string, std::string>> parse_args(int argc, char** argv,
                                                              std::string* error) {
    if (argc != 3) {
        *error = argc < 3 ? "missing <file> or <mask>" : "too many arguments";
        return std::nullopt;
    }
    return std::make_pair(std::string(argv[1]), std::string(argv[2]));
}

int print_error(std::string_view message) {
    std::cerr << "Error: " << message << '\n';
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    std::string error;
    auto parsed = parse_args(argc, argv, &error);
    if (!parsed) {
        std::cerr << "mtfind: " << error << "\n" << usage_text();
        return 2;
    }

    const std::string file_path = std::move(parsed->first);
    const std::string mask_text = std::move(parsed->second);
    if (auto mask_error = mtfind::validate_mask(mask_text)) {
        return print_error(*mask_error);
    }

    std::error_code ec;
    const std::filesystem::path path(file_path);
    if (!std::filesystem::is_regular_file(path, ec)) {
        const std::string reason = ec ? ec.message() : "not a regular file";
        return print_error("cannot open file '" + file_path + "': " + reason);
    }

    const mtfind::Mask mask = mtfind::parse_mask(mask_text);
    const std::size_t hw = std::thread::hardware_concurrency();
    const std::size_t workers = hw > 0 ? hw : 1;

    const std::size_t chunk_capacity = workers + 2;
    const std::size_t result_capacity = 2 * workers + 1;
    mtfind::BlockingQueue<std::optional<mtfind::Chunk>> chunks(chunk_capacity);
    mtfind::BlockingQueue<mtfind::ChunkResult> results(result_capacity);

    std::thread producer(mtfind::producer_main, file_path, std::ref(chunks), workers);

    std::vector<std::thread> pool;
    pool.reserve(workers);
    std::ranges::for_each(std::views::iota(std::size_t{0}, workers), [&](std::size_t) {
        pool.emplace_back(mtfind::worker_main, std::cref(mask), std::ref(chunks),
                          std::ref(results));
    });

    std::vector<mtfind::Match> buffer;
    buffer.reserve(1024);
    const std::size_t count = mtfind::collector_main(results, workers, buffer);

    producer.join();
    std::ranges::for_each(pool, [](std::thread& worker) { worker.join(); });

    std::cout.sync_with_stdio(false);
    std::cout << count << '\n';
    std::ranges::for_each(buffer, [](const mtfind::Match& match) {
        std::cout << match.line << ' ' << match.col << ' ' << match.text << '\n';
    });
    return 0;
}