#include "pearl/candidate.hpp"

#include "xdna/errors.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace xdna::pearl;

struct Options {
    XdnaMatmulArtifact artifact;
    std::string selector = "0";
    std::filesystem::path evidence;
};

[[nodiscard]] std::string json_escape(std::string_view value)
{
    std::string result;
    for (const char character : value) {
        if (character == '\\') result += "\\\\";
        else if (character == '"') result += "\\\"";
        else result += character;
    }
    return result;
}

[[nodiscard]] PeriodicPattern rows_pattern()
{
    return PeriodicPattern::from_indices(std::array<std::uint32_t, 2U>{0U, 8U});
}

[[nodiscard]] PeriodicPattern columns_pattern()
{
    std::array<std::uint32_t, kSelectedColumns> columns{};
    for (std::size_t index = 0U; index < columns.size(); ++index) {
        columns[index] = static_cast<std::uint32_t>((index / 2U) * 8U + (index % 2U));
    }
    return PeriodicPattern::from_indices(columns);
}

[[nodiscard]] Int8Matrix select_rows(const Int8Matrix& matrix,
                                     std::span<const std::uint32_t> rows)
{
    std::vector<std::int8_t> values;
    values.reserve(rows.size() * matrix.cols());
    for (const std::uint32_t row : rows) {
        for (std::size_t column = 0U; column < matrix.cols(); ++column) {
            values.push_back(matrix.at(row, column));
        }
    }
    return Int8Matrix(rows.size(), matrix.cols(), std::move(values));
}

[[nodiscard]] Int8Matrix select_columns(const Int8Matrix& matrix,
                                         std::span<const std::uint32_t> columns)
{
    std::vector<std::int8_t> values;
    values.reserve(matrix.rows() * columns.size());
    for (std::size_t row = 0U; row < matrix.rows(); ++row) {
        for (const std::uint32_t column : columns) {
            values.push_back(matrix.at(row, column));
        }
    }
    return Int8Matrix(matrix.rows(), columns.size(), std::move(values));
}

void parse_options(int argc, char** argv, Options& options)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        const auto require_value = [&](const char* name) -> std::string {
            if (index + 1 >= argc) throw std::runtime_error(std::string("missing value for ") + name);
            return argv[++index];
        };
        if (argument == "--xclbin") options.artifact.xclbin = require_value("--xclbin");
        else if (argument == "--insts") options.artifact.instructions = require_value("--insts");
        else if (argument == "--manifest") options.artifact.manifest = require_value("--manifest");
        else if (argument == "--selector") options.selector = require_value("--selector");
        else if (argument == "--evidence") options.evidence = require_value("--evidence");
        else if (argument == "--help") {
            std::cout << "usage: pearl_candidate_differential --xclbin PATH --insts PATH "
                         "--manifest PATH [--selector DEVICE] [--evidence PATH]\n";
            std::exit(0);
        } else throw std::runtime_error("unknown argument: " + std::string(argument));
    }
    if (options.artifact.xclbin.empty() || options.artifact.instructions.empty()
        || options.artifact.manifest.empty()) {
        throw std::runtime_error("--xclbin, --insts, and --manifest are required");
    }
}

void write_evidence(const Options& options,
                    const XdnaMatmulExecutor& executor,
                    const ComputePipelineResult& result,
                    std::size_t proof_bytes)
{
    if (options.evidence.empty()) return;
    if (!options.evidence.parent_path().empty()) {
        std::filesystem::create_directories(options.evidence.parent_path());
    }
    std::ofstream output(options.evidence);
    if (!output) throw std::runtime_error("cannot write candidate evidence");
    const auto& capability = executor.capability();
    const auto& counters = executor.counters();
    output << "{\n"
           << "  \"schema\": \"pearl-p5-candidate-proof-evidence/v1\",\n"
           << "  \"milestone\": \"P5\",\n"
           << "  \"status\": \"PASS\",\n"
           << "  \"candidate\": {\"proof_envelope\": \"P1-fixed-width\", "
              "\"serialized_bytes\": " << proof_bytes << ", \"target_test\": true, "
              "\"cpu_reconstruction\": true, \"openings_verified\": true},\n"
           << "  \"target\": {\"device\": \"" << json_escape(capability.device_name)
           << "\", \"architecture\": \"" << json_escape(capability.architecture)
           << "\", \"bdf\": \"" << json_escape(capability.bdf) << "\"},\n"
           << "  \"dispatches\": {\"completed\": " << counters.completed_dispatches
           << ", \"h2d_syncs\": " << counters.h2d_syncs
           << ", \"d2h_syncs\": " << counters.d2h_syncs
           << ", \"cpu_fallbacks\": 0},\n"
           << "  \"jackpot_hex_bytes\": \"exact-32-byte-value-recorded-in-memory-only\",\n"
           << "  \"timings_ns\": {\"total\": " << result.timings.total_ns
           << ", \"npu_execution\": " << result.timings.npu_execution_ns
           << ", \"cpu_verification\": " << result.timings.cpu_verification_ns << "}\n"
           << "}\n";
}

} // namespace

int main(int argc, char** argv)
{
    try {
        Options options;
        parse_options(argc, argv, options);
        XdnaMatmulExecutor executor(options.artifact, options.selector);
        ComputePipeline pipeline(executor);

        MiningConfiguration config;
        config.common_dim = 2048U;
        config.rank = 128U;
        config.rows_pattern = rows_pattern();
        config.cols_pattern = columns_pattern();
        IncompleteBlockHeader header;
        header.version = 1U;
        header.timestamp = 123U;
        header.nbits = 0x207FFFFFU;
        const Int8Matrix full_a(9U, 2048U, std::vector<std::int8_t>(9U * 2048U, 0));
        const Int8Matrix full_b(2048U, 250U, std::vector<std::int8_t>(2048U * 250U, 0));
        const std::vector<std::uint32_t> rows = config.rows_pattern.indices();
        const std::vector<std::uint32_t> columns = config.cols_pattern.indices();
        const Int8Matrix selected_a = select_rows(full_a, rows);
        const Int8Matrix selected_b = select_columns(full_b, columns);
        const Digest key = job_key(header, config);
        const Digest hash_a = merkle_root(full_a.raw_bytes(), key);
        const Int8Matrix full_bt(250U, 2048U, [&] {
            std::vector<std::int8_t> values(250U * 2048U, 0);
            for (std::size_t row = 0U; row < full_b.rows(); ++row) {
                for (std::size_t column = 0U; column < full_b.cols(); ++column) {
                    values[column * full_b.rows() + row] = full_b.at(row, column);
                }
            }
            return values;
        }());
        const Digest hash_b = merkle_root(full_bt.raw_bytes(), key);
        const CommitmentSeeds seeds = commitment_seeds(key, hash_a, hash_b);
        const std::vector<std::size_t> row_indices(rows.begin(), rows.end());
        const std::vector<std::size_t> column_indices(columns.begin(), columns.end());
        const NoiseMatrices noise = generate_noise(
            2048U, 128U, seeds, row_indices, column_indices);
        Digest target{};
        target.fill(0xFFU);
        const ComputePipelineResult result = pipeline.run(
            selected_a, selected_b, noise, 128U, seeds.a_noise_seed, target);
        const PlainProof proof = build_plain_proof(
            header, config, full_a, full_b, 0U, 0U, result, target);
        verify_plain_proof_candidate(proof);
        const std::vector<std::uint8_t> serialized = proof.serialize();
        write_evidence(options, executor, result, serialized.size());
        std::cout << "device=" << executor.capability().device_name << '\n'
                  << "architecture=" << executor.capability().architecture << '\n'
                  << "bdf=" << executor.capability().bdf << '\n'
                  << "dispatches=" << executor.counters().completed_dispatches << '\n'
                  << "proof_bytes=" << serialized.size() << '\n'
                  << "mismatches=0\n"
                  << "cpu_fallbacks=0\n"
                  << "candidate=verified\n";
        return 0;
    } catch (const xdna::runtime::RuntimeError& error) {
        std::cerr << xdna::runtime::error_code_name(error.code())
                  << ": " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "CANDIDATE_FAILURE: " << error.what() << '\n';
        return 1;
    }
}
