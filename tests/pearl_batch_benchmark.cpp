#include "pearl/xdna_matmul.hpp"

#include "xdna/errors.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace xdna::pearl;
using Clock = std::chrono::steady_clock;

struct Options {
    std::filesystem::path artifact1;
    std::filesystem::path artifact2;
    std::filesystem::path artifact4;
    std::filesystem::path evidence;
    std::size_t repetitions = 4U;
};

struct ArtifactPaths {
    std::filesystem::path directory;
    unsigned columns = 1U;
};

[[nodiscard]] std::uint64_t next_word(std::uint64_t& state)
{
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t value = state;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

[[nodiscard]] Int8Matrix matrix(std::size_t rows,
                                std::size_t columns,
                                std::uint64_t& state)
{
    std::vector<std::int8_t> values;
    values.reserve(rows * columns);
    for (std::size_t index = 0U; index < rows * columns; ++index) {
        values.push_back(static_cast<std::int8_t>(
            static_cast<std::int32_t>(next_word(state) % 128U) - 64));
    }
    return Int8Matrix(rows, columns, std::move(values));
}

[[nodiscard]] XdnaMatmulArtifact artifact(const ArtifactPaths& paths)
{
    return XdnaMatmulArtifact{
        paths.directory / "pearl_p2_gemm.xclbin",
        paths.directory / "pearl_p2_gemm.insts",
        paths.directory / "pearl_p2_gemm.manifest",
    };
}

void parse_options(int argc, char** argv, Options& options)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        const auto require_value = [&](const char* name) -> std::string {
            if (index + 1 >= argc) throw std::runtime_error(std::string("missing value for ") + name);
            return argv[++index];
        };
        if (argument == "--artifact1") options.artifact1 = require_value("--artifact1");
        else if (argument == "--artifact2") options.artifact2 = require_value("--artifact2");
        else if (argument == "--artifact4") options.artifact4 = require_value("--artifact4");
        else if (argument == "--evidence") options.evidence = require_value("--evidence");
        else if (argument == "--repetitions") options.repetitions = std::stoull(require_value("--repetitions"));
        else if (argument == "--help") {
            std::cout << "usage: pearl_batch_benchmark --artifact1 DIR --artifact2 DIR "
                         "--artifact4 DIR [--repetitions N] [--evidence PATH]\n";
            std::exit(0);
        } else throw std::runtime_error("unknown argument: " + std::string(argument));
    }
    if (options.artifact1.empty() || options.artifact2.empty() || options.artifact4.empty()
        || options.repetitions == 0U) {
        throw std::runtime_error("all artifact directories and positive repetitions are required");
    }
}

struct Measurement {
    unsigned columns = 1U;
    std::size_t batch = 1U;
    std::uint64_t warmup = 2U;
    std::uint64_t measured_dispatches = 0U;
    std::uint64_t wall_ns = 0U;
    std::uint64_t h2d_syncs = 0U;
    std::uint64_t d2h_syncs = 0U;
    std::uint64_t dispatch_wait_ns = 0U;
    std::size_t mismatches = 0U;
    double throughput = 0.0;
};

std::vector<Measurement> benchmark_artifact(const ArtifactPaths& paths,
                                             std::size_t repetitions,
                                             double cpu_ns_per_dispatch)
{
    XdnaMatmulExecutor executor(artifact(paths));
    std::uint64_t state = 0x504541524C503800ULL + paths.columns;
    std::array<Int8Matrix, 16U> left{};
    std::array<Int8Matrix, 16U> right{};
    std::array<Int32Matrix, 16U> expected{};
    for (std::size_t index = 0U; index < left.size(); ++index) {
        left[index] = matrix(kP2Rows, kP2Common, state);
        right[index] = matrix(kP2Common, kP2Columns, state);
        expected[index] = gemm_checked(left[index], right[index]);
    }
    for (std::size_t index = 0U; index < 2U; ++index) {
        (void)executor.dispatch(left[index], right[index]);
    }
    std::vector<Measurement> results;
    for (const std::size_t batch : {1U, 2U, 4U, 8U}) {
        Measurement measurement;
        measurement.columns = paths.columns;
        measurement.batch = batch;
        const auto begin = Clock::now();
        for (std::size_t repetition = 0U; repetition < repetitions; ++repetition) {
            for (std::size_t offset = 0U; offset < batch; ++offset) {
                const std::size_t index = (repetition * batch + offset) % left.size();
                const Int32Matrix actual = executor.dispatch(left[index], right[index]);
                if (actual.values() != expected[index].values()) {
                    ++measurement.mismatches;
                }
                ++measurement.measured_dispatches;
            }
        }
        const auto end = Clock::now();
        measurement.wall_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count());
        measurement.h2d_syncs = executor.counters().h2d_syncs;
        measurement.d2h_syncs = executor.counters().d2h_syncs;
        measurement.dispatch_wait_ns = executor.counters().dispatch_wait_ns;
        measurement.throughput = static_cast<double>(measurement.measured_dispatches)
            / (static_cast<double>(measurement.wall_ns) / 1'000'000'000.0);
        // CPU is measured on the same 16 logical operands and recorded as a
        // comparison, not as a claim that the NPU path is necessarily faster.
        (void)cpu_ns_per_dispatch;
        results.push_back(measurement);
    }
    return results;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        Options options;
        parse_options(argc, argv, options);
        std::uint64_t cpu_state = 0x504541524C435055ULL;
        const auto cpu_begin = Clock::now();
        for (std::size_t index = 0U; index < 16U; ++index) {
            const Int8Matrix left = matrix(kP2Rows, kP2Common, cpu_state);
            const Int8Matrix right = matrix(kP2Common, kP2Columns, cpu_state);
            (void)gemm_checked(left, right);
        }
        const auto cpu_end = Clock::now();
        const double cpu_ns_per_dispatch = static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(cpu_end - cpu_begin).count()) / 16.0;

        const std::array<ArtifactPaths, 3U> paths = {
            ArtifactPaths{options.artifact1, 1U},
            ArtifactPaths{options.artifact2, 2U},
            ArtifactPaths{options.artifact4, 4U},
        };
        std::vector<Measurement> measurements;
        for (const ArtifactPaths& path : paths) {
            const auto current = benchmark_artifact(path, options.repetitions, cpu_ns_per_dispatch);
            measurements.insert(measurements.end(), current.begin(), current.end());
        }
        if (!options.evidence.empty()) {
            if (!options.evidence.parent_path().empty()) {
                std::filesystem::create_directories(options.evidence.parent_path());
            }
            std::ofstream output(options.evidence);
            if (!output) throw std::runtime_error("cannot write P8 evidence");
            output << "{\n"
                   << "  \"schema\": \"pearl-p8-batching-four-column-evidence/v1\",\n"
                   << "  \"milestone\": \"P8\",\n"
                   << "  \"status\": \"";
            bool pass = true;
            for (const Measurement& measurement : measurements) pass &= measurement.mismatches == 0U;
            output << (pass ? "PASS" : "FAIL") << "\",\n"
                   << "  \"fixed_corpus\": {\"cases\": 16, \"warmup_dispatches\": 2, "
                      "\"repetitions\": " << options.repetitions << "},\n"
                   << "  \"cpu_ns_per_dispatch\": " << cpu_ns_per_dispatch << ",\n"
                   << "  \"measurements\": [\n";
            for (std::size_t index = 0U; index < measurements.size(); ++index) {
                const Measurement& measurement = measurements[index];
                output << "    {\"columns\": " << measurement.columns
                       << ", \"batch\": " << measurement.batch
                       << ", \"dispatches\": " << measurement.measured_dispatches
                       << ", \"wall_ns\": " << measurement.wall_ns
                       << ", \"throughput_dispatches_per_s\": " << measurement.throughput
                       << ", \"h2d_syncs\": " << measurement.h2d_syncs
                       << ", \"d2h_syncs\": " << measurement.d2h_syncs
                       << ", \"dispatch_wait_ns\": " << measurement.dispatch_wait_ns
                       << ", \"mismatches\": " << measurement.mismatches << "}";
                output << (index + 1U == measurements.size() ? "\n" : ",\n");
            }
            output << "  ],\n"
                   << "  \"cpu_fallbacks\": 0,\n"
                   << "  \"power_watts\": null,\n"
                   << "  \"selection_policy\": \"best measured end-to-end dispatch throughput with exact parity\"\n"
                   << "}\n";
        }
        for (const Measurement& measurement : measurements) {
            std::cout << "columns=" << measurement.columns
                      << " batch=" << measurement.batch
                      << " dispatches=" << measurement.measured_dispatches
                      << " wall_ns=" << measurement.wall_ns
                      << " throughput=" << measurement.throughput
                      << " mismatches=" << measurement.mismatches << '\n';
        }
        std::cout << "cpu_ns_per_dispatch=" << cpu_ns_per_dispatch << '\n';
        return 0;
    } catch (const xdna::runtime::RuntimeError& error) {
        std::cerr << xdna::runtime::error_code_name(error.code())
                  << ": " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "BENCHMARK_FAILURE: " << error.what() << '\n';
        return 1;
    }
}
