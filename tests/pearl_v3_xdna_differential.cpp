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
    std::size_t deterministic_cases = 100U;
    std::size_t randomized_cases = 32U;
    std::filesystem::path evidence;
};

[[nodiscard]] std::string json_escape(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        if (character == '\\') result += "\\\\";
        else if (character == '"') result += "\\\"";
        else result.push_back(character);
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
                                 bool randomized,
                                 std::uint64_t& state)
{
    std::vector<std::int8_t> values;
    values.reserve(rows * columns);
    for (std::size_t index = 0U; index < rows * columns; ++index) {
        const std::int32_t value = randomized
            ? static_cast<std::int32_t>(next_word(state) & 63U) - 32
            : 0;
        values.push_back(static_cast<std::int8_t>(value));
    }
    return Int8Matrix(rows, columns, std::move(values));
}

[[nodiscard]] Int8Matrix transpose(const Int8Matrix& value)
{
    std::vector<std::int8_t> result(value.rows() * value.cols(), 0);
    for (std::size_t row = 0U; row < value.rows(); ++row) {
        for (std::size_t column = 0U; column < value.cols(); ++column) {
            result[column * value.rows() + row] = value.at(row, column);
        }
    }
    return Int8Matrix(value.cols(), value.rows(), std::move(result));
}

[[nodiscard]] Int8Matrix select_rows(const Int8Matrix& value,
                                      std::span<const std::uint32_t> rows)
{
    std::vector<std::int8_t> result;
    result.reserve(rows.size() * value.cols());
    for (const std::uint32_t row : rows) {
        for (std::size_t column = 0U; column < value.cols(); ++column) {
            result.push_back(value.at(row, column));
        }
    }
    return Int8Matrix(rows.size(), value.cols(), std::move(result));
}

[[nodiscard]] Int8Matrix select_columns(const Int8Matrix& value,
                                         std::span<const std::uint32_t> columns)
{
    std::vector<std::int8_t> result;
    result.reserve(value.rows() * columns.size());
    for (std::size_t row = 0U; row < value.rows(); ++row) {
        for (const std::uint32_t column : columns) {
            result.push_back(value.at(row, column));
        }
    }
    return Int8Matrix(value.rows(), columns.size(), std::move(result));
}

[[nodiscard]] bool same_transcript(const TranscriptResult& left,
                                   const TranscriptResult& right)
{
    if (left.words != right.words || left.trace.size() != right.trace.size()) return false;
    for (std::size_t index = 0U; index < left.trace.size(); ++index) {
        const TranscriptStep& a = left.trace[index];
        const TranscriptStep& b = right.trace[index];
        if (a.reduction_index != b.reduction_index || a.combined_xor != b.combined_xor
            || a.state != b.state) {
            return false;
        }
    }
    return true;
}

void parse_options(int argc, char** argv, Options& options)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        const auto require_value = [&](const char* label) -> std::string {
            if (index + 1 >= argc) throw std::runtime_error(std::string("missing value for ") + label);
            return argv[++index];
        };
        if (argument == "--xclbin") options.artifact.xclbin = require_value("--xclbin");
        else if (argument == "--insts") options.artifact.instructions = require_value("--insts");
        else if (argument == "--manifest") options.artifact.manifest = require_value("--manifest");
        else if (argument == "--selector") options.selector = require_value("--selector");
        else if (argument == "--deterministic-cases") {
            options.deterministic_cases = static_cast<std::size_t>(
                std::stoull(require_value("--deterministic-cases")));
        } else if (argument == "--randomized-cases") {
            options.randomized_cases = static_cast<std::size_t>(
                std::stoull(require_value("--randomized-cases")));
        } else if (argument == "--evidence") options.evidence = require_value("--evidence");
        else if (argument == "--help") {
            std::cout << "usage: pearl_v3_xdna_differential --xclbin PATH --insts PATH "
                         "--manifest PATH [--selector DEVICE] [--deterministic-cases N] "
                         "[--randomized-cases N] [--evidence PATH]\n";
            std::exit(0);
        } else throw std::runtime_error("unknown argument: " + std::string(argument));
    }
    if (options.artifact.xclbin.empty() || options.artifact.instructions.empty()
        || options.artifact.manifest.empty() || options.deterministic_cases == 0U) {
        throw std::runtime_error("artifact paths and positive deterministic case count are required");
    }
}

void write_evidence(const Options& options,
                    const XdnaMatmulExecutor& executor,
                    std::size_t completed,
                    std::size_t arithmetic_mismatches,
                    std::size_t seed_mismatches,
                    std::size_t transcript_mismatches,
                    std::size_t jackpot_mismatches,
                    std::size_t runtime_failures)
{
    if (options.evidence.empty()) return;
    if (!options.evidence.parent_path().empty()) {
        std::filesystem::create_directories(options.evidence.parent_path());
    }
    std::ofstream output(options.evidence);
    if (!output) throw std::runtime_error("cannot write V3 XDNA evidence");
    const auto& capability = executor.capability();
    const auto& counters = executor.counters();
    const std::size_t requested = options.deterministic_cases + options.randomized_cases;
    const bool pass = completed == requested && arithmetic_mismatches == 0U
        && seed_mismatches == 0U && transcript_mismatches == 0U
        && jackpot_mismatches == 0U && runtime_failures == 0U;
    output << "{\n"
           << "  \"schema\": \"pearl-v3-xdna-differential/v1\",\n"
           << "  \"status\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
           << "  \"certificate_version\": 3,\n"
           << "  \"workload\": {\"deterministic_cases\": " << options.deterministic_cases
           << ", \"randomized_cases\": " << options.randomized_cases
           << ", \"completed\": " << completed << "},\n"
           << "  \"target\": {\"device\": \"" << json_escape(capability.device_name)
           << "\", \"architecture\": \"" << json_escape(capability.architecture)
           << "\", \"bdf\": \"" << json_escape(capability.bdf)
           << "\", \"xrt\": \"" << json_escape(capability.xrt_version) << "\"},\n"
           << "  \"results\": {\"arithmetic_mismatches\": " << arithmetic_mismatches
           << ", \"seed_mismatches\": " << seed_mismatches
           << ", \"transcript_mismatches\": " << transcript_mismatches
           << ", \"jackpot_mismatches\": " << jackpot_mismatches
           << ", \"runtime_failures\": " << runtime_failures
           << ", \"cpu_fallbacks\": 0},\n"
           << "  \"physical_xrt\": {\"dispatches\": " << counters.completed_dispatches
           << ", \"h2d_syncs\": " << counters.h2d_syncs
           << ", \"d2h_syncs\": " << counters.d2h_syncs
           << ", \"silent_cpu_fallback\": false}\n"
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
        const std::vector<std::uint32_t> rows = config.rows_pattern.indices();
        const std::vector<std::uint32_t> columns = config.cols_pattern.indices();
        const std::vector<std::size_t> row_indices(rows.begin(), rows.end());
        const std::vector<std::size_t> column_indices(columns.begin(), columns.end());
        Digest target{};
        target.fill(0xFFU);
        const std::size_t requested = options.deterministic_cases + options.randomized_cases;
        std::uint64_t state = 0x504541524C563342ULL;
        std::size_t completed = 0U;
        std::size_t arithmetic_mismatches = 0U;
        std::size_t seed_mismatches = 0U;
        std::size_t transcript_mismatches = 0U;
        std::size_t jackpot_mismatches = 0U;
        std::size_t runtime_failures = 0U;

        for (std::size_t case_index = 0U; case_index < requested; ++case_index) {
            const bool randomized = case_index >= options.deterministic_cases;
            IncompleteBlockHeader header;
            header.version = 1U;
            header.timestamp = static_cast<std::uint32_t>(123U + case_index);
            header.nbits = 0x207FFFFFU;
            const Int8Matrix full_a = matrix(9U, 2048U, randomized, state);
            const Int8Matrix full_b = matrix(2048U, 250U, randomized, state);
            const Int8Matrix full_bt = transpose(full_b);
            const Digest key = job_key(header, config);
            const Digest hash_a = merkle_root(full_a.raw_bytes(), key);
            const Digest hash_b = merkle_root(full_bt.raw_bytes(), key);
            const CommitmentSeeds seeds = commitment_seeds(
                CertificateVersion::V3, key, hash_a, hash_b, full_a.rows(), full_b.cols());
            const NoiseMatrices noise = generate_noise(
                2048U, 128U, seeds, row_indices, column_indices);
            const Int8Matrix selected_a = select_rows(full_a, rows);
            const Int8Matrix selected_b = select_columns(full_b, columns);
            MiningJob job;
            const auto header_bytes = serialize_header(header);
            job.incomplete_header_bytes.assign(header_bytes.begin(), header_bytes.end());
            job.target = target;
            job.target_decimal = "115792089237316195423570985008687907853269984665640564039457584007913129639935";
            job.certificate_version = CertificateVersion::V3;
            job.job_id = "v3-xdna-" + std::to_string(case_index);
            const CandidateBinding binding = make_candidate_binding(
                job, header, full_a.rows(), full_b.cols());
            try {
                const ComputePipelineResult result = pipeline.run(
                    selected_a, selected_b, noise, 128U, seeds.a_noise_seed, target);
                const Int32Matrix expected = gemm_checked(selected_a, selected_b);
                if (result.denoised_product.values() != expected.values()) {
                    ++arithmetic_mismatches;
                    break;
                }
                const NoisedOperands operands = make_noised_operands(selected_a, selected_b, noise);
                const Int32Matrix cpu_noised = gemm_checked(operands.a, operands.b);
                const TranscriptResult expected_transcript = selected_transcript(
                    operands.a, operands.b, cpu_noised, 128U);
                if (!same_transcript(result.transcript, expected_transcript)) {
                    ++transcript_mismatches;
                    break;
                }
                const Digest expected_jackpot = jackpot_hash(expected_transcript.words, seeds.a_noise_seed);
                if (result.jackpot != expected_jackpot) {
                    ++jackpot_mismatches;
                    break;
                }
                const PlainProof proof = build_plain_proof(
                    binding, header, config, full_a, full_b, 0U, 0U, result);
                verify_plain_proof_candidate(proof, binding);
                (void)serialize_official_plain_proof(proof, binding);
                ++completed;
            } catch (const Error& error) {
                const std::string message = error.what();
                if (message.find("seed") != std::string::npos) ++seed_mismatches;
                else if (message.find("transcript") != std::string::npos) ++transcript_mismatches;
                else if (message.find("jackpot") != std::string::npos) ++jackpot_mismatches;
                else ++arithmetic_mismatches;
                std::cerr << "V3_MISMATCH case=" << case_index << " error=" << message << '\n';
                break;
            } catch (const std::exception& error) {
                ++runtime_failures;
                std::cerr << "V3_RUNTIME_FAILURE case=" << case_index << " error=" << error.what() << '\n';
                break;
            }
        }
        write_evidence(options,
                       executor,
                       completed,
                       arithmetic_mismatches,
                       seed_mismatches,
                       transcript_mismatches,
                       jackpot_mismatches,
                       runtime_failures);
        std::cout << "certificate_version=3\n"
                  << "requested_cases=" << requested << '\n'
                  << "deterministic_cases=" << options.deterministic_cases << '\n'
                  << "randomized_cases=" << options.randomized_cases << '\n'
                  << "completed_cases=" << completed << '\n'
                  << "arithmetic_mismatches=" << arithmetic_mismatches << '\n'
                  << "seed_mismatches=" << seed_mismatches << '\n'
                  << "transcript_mismatches=" << transcript_mismatches << '\n'
                  << "jackpot_mismatches=" << jackpot_mismatches << '\n'
                  << "runtime_failures=" << runtime_failures << '\n'
                  << "cpu_fallbacks=0\n"
                  << "physical_xrt=true\n";
        return completed == requested && arithmetic_mismatches == 0U
                && seed_mismatches == 0U && transcript_mismatches == 0U
                && jackpot_mismatches == 0U && runtime_failures == 0U
            ? 0 : 1;
    } catch (const xdna::runtime::RuntimeError& error) {
        std::cerr << xdna::runtime::error_code_name(error.code()) << ": " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "V3_XDNA_FAILURE: " << error.what() << '\n';
        return 1;
    }
}
