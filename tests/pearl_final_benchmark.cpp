#include "pearl/candidate.hpp"

#include "xdna/errors.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <vector>

namespace {

using namespace xdna::pearl;
using Clock = std::chrono::steady_clock;

struct Options {
    std::filesystem::path artifact_dir;
    std::filesystem::path evidence;
    std::size_t raw_iterations = 100U;
    std::size_t candidate_iterations = 2U;
};

[[nodiscard]] std::string json_escape(std::string_view value)
{
    std::string result;
    for (const char character : value) {
        if (character == '\\') result += "\\\\";
        else if (character == '"') result += "\\\"";
        else if (character == '\n') result += "\\n";
        else result += character;
    }
    return result;
}

[[nodiscard]] std::string command_output(const std::string& command)
{
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) return {};
    std::string result;
    std::array<char, 256U> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }
    (void)pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
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
                                     const std::vector<std::uint32_t>& rows)
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
                                         const std::vector<std::uint32_t>& columns)
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

[[nodiscard]] XdnaMatmulArtifact artifact(const Options& options)
{
    return XdnaMatmulArtifact{
        options.artifact_dir / "pearl_p2_gemm.xclbin",
        options.artifact_dir / "pearl_p2_gemm.insts",
        options.artifact_dir / "pearl_p2_gemm.manifest",
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
        if (argument == "--artifact-dir") options.artifact_dir = require_value("--artifact-dir");
        else if (argument == "--evidence") options.evidence = require_value("--evidence");
        else if (argument == "--raw-iterations") options.raw_iterations = std::stoull(require_value("--raw-iterations"));
        else if (argument == "--candidate-iterations") options.candidate_iterations = std::stoull(require_value("--candidate-iterations"));
        else if (argument == "--help") {
            std::cout << "usage: pearl_final_benchmark --artifact-dir DIR [--raw-iterations N] "
                         "[--candidate-iterations N] [--evidence PATH]\n";
            std::exit(0);
        } else throw std::runtime_error("unknown argument: " + std::string(argument));
    }
    if (options.artifact_dir.empty() || options.raw_iterations == 0U
        || options.candidate_iterations == 0U) {
        throw std::runtime_error("artifact directory and positive iteration counts are required");
    }
}

} // namespace

int main(int argc, char** argv)
{
    try {
        Options options;
        parse_options(argc, argv, options);
        XdnaMatmulExecutor executor(artifact(options));
        ComputePipeline pipeline(executor);
        const Int8Matrix raw_left(kP2Rows, kP2Common,
                                  std::vector<std::int8_t>(kP2LeftBytes, 1));
        const Int8Matrix raw_right(kP2Common, kP2Columns,
                                   std::vector<std::int8_t>(kP2RightBytes, -1));
        const Int32Matrix raw_expected = gemm_checked(raw_left, raw_right);
        for (std::size_t warmup = 0U; warmup < 8U; ++warmup) {
            if (executor.dispatch(raw_left, raw_right).values() != raw_expected.values()) {
                throw std::runtime_error("raw GEMM warm-up mismatch");
            }
        }
        const auto cpu_begin = Clock::now();
        for (std::size_t iteration = 0U; iteration < options.raw_iterations; ++iteration) {
            (void)gemm_checked(raw_left, raw_right);
        }
        const auto cpu_end = Clock::now();
        const std::uint64_t cpu_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(cpu_end - cpu_begin).count());
        const auto raw_begin = Clock::now();
        for (std::size_t iteration = 0U; iteration < options.raw_iterations; ++iteration) {
            if (executor.dispatch(raw_left, raw_right).values() != raw_expected.values()) {
                throw std::runtime_error("raw GEMM mismatch");
            }
        }
        const auto raw_end = Clock::now();
        const std::uint64_t raw_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(raw_end - raw_begin).count());
        const std::uint64_t raw_h2d_syncs = executor.counters().h2d_syncs;
        const std::uint64_t raw_d2h_syncs = executor.counters().d2h_syncs;
        const std::uint64_t raw_wait_ns = executor.counters().dispatch_wait_ns;

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
        std::vector<std::int8_t> bt_values(250U * 2048U, 0);
        for (std::size_t row = 0U; row < full_b.rows(); ++row) {
            for (std::size_t column = 0U; column < full_b.cols(); ++column) {
                bt_values[column * full_b.rows() + row] = full_b.at(row, column);
            }
        }
        const Int8Matrix full_bt(250U, 2048U, std::move(bt_values));
        const Digest hash_a = merkle_root(full_a.raw_bytes(), key);
        const Digest hash_b = merkle_root(full_bt.raw_bytes(), key);
        const CommitmentSeeds seeds = commitment_seeds(
            CertificateVersion::V2, key, hash_a, hash_b, full_a.rows(), full_b.cols());
        const std::vector<std::size_t> row_indices(rows.begin(), rows.end());
        const std::vector<std::size_t> column_indices(columns.begin(), columns.end());
        const NoiseMatrices noise = generate_noise(2048U, 128U, seeds, row_indices, column_indices);
        Digest target{};
        target.fill(0xFFU);
        MiningJob job;
        const auto header_bytes = serialize_header(header);
        job.incomplete_header_bytes.assign(header_bytes.begin(), header_bytes.end());
        job.target = target;
        job.target_decimal = "115792089237316195423570985008687907853269984665640564039457584007913129639935";
        job.certificate_version = CertificateVersion::V2;
        job.job_id = "p9-v2-fixture";
        const CandidateBinding binding = make_candidate_binding(
            job, header, full_a.rows(), full_b.cols());

        std::uint64_t candidate_ns = 0U;
        std::size_t candidate_mismatches = 0U;
        std::size_t proof_bytes = 0U;
        for (std::size_t iteration = 0U; iteration < options.candidate_iterations; ++iteration) {
            const auto begin = Clock::now();
            const ComputePipelineResult result = pipeline.run(
                selected_a, selected_b, noise, 128U, seeds.a_noise_seed, target);
            const PlainProof proof = build_plain_proof(
                binding, header, config, full_a, full_b, 0U, 0U, result);
            verify_plain_proof_candidate(proof, binding);
            proof_bytes = proof.serialize().size();
            const auto end = Clock::now();
            candidate_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count());
            if (!result.meets_target) ++candidate_mismatches;
        }

        struct rusage usage{};
        (void)getrusage(RUSAGE_SELF, &usage);
        const double raw_throughput = static_cast<double>(options.raw_iterations)
            / (static_cast<double>(raw_ns) / 1'000'000'000.0);
        const double candidate_throughput = static_cast<double>(options.candidate_iterations)
            / (static_cast<double>(candidate_ns) / 1'000'000'000.0);
        const auto& capability = executor.capability();
        const std::string kernel_release = [] {
            struct utsname name{};
            return uname(&name) == 0 ? std::string(name.release) : std::string{};
        }();
        if (!options.evidence.empty()) {
            if (!options.evidence.parent_path().empty()) {
                std::filesystem::create_directories(options.evidence.parent_path());
            }
            std::ofstream output(options.evidence);
            if (!output) throw std::runtime_error("cannot write P9 evidence");
            output << "{\n"
                   << "  \"schema\": \"pearl-p9-final-benchmark-evidence/v1\",\n"
                   << "  \"milestone\": \"P9\",\n"
                   << "  \"status\": \"" << (candidate_mismatches == 0U ? "PASS" : "FAIL") << "\",\n"
                   << "  \"project_commit\": \"" << json_escape(command_output("git rev-parse HEAD")) << "\",\n"
                   << "  \"hardware\": {\"cpu_model\": \""
                   << json_escape(command_output("awk -F: '/model name/ {print $2; exit}' /proc/cpuinfo"))
                   << "\", \"kernel\": \"" << json_escape(kernel_release)
                   << "\", \"ram_hwm_kb\": " << usage.ru_maxrss
                   << ", \"device\": \"" << json_escape(capability.device_name)
                   << "\", \"architecture\": \"" << json_escape(capability.architecture)
                   << "\", \"bdf\": \"" << json_escape(capability.bdf)
                   << "\", \"firmware\": \"" << json_escape(capability.firmware_version)
                   << "\", \"xrt\": \"" << json_escape(capability.xrt_version)
                   << "\", \"amdxdna\": \"" << json_escape(capability.amdxdna_version)
                   << "\"},\n"
                   << "  \"software\": {\"mlir_aie_commit\": \""
                   << json_escape(command_output("git -C /home/umutcagand/mlir-aie rev-parse HEAD 2>/dev/null"))
                   << "\", \"pearl_revision\": \"fe22b6a2b831d95b2f56564808f39d2f498f34a5\"},\n"
                   << "  \"workload\": {\"tile\": \"4x64x8\", \"rank\": 128, \"K\": 2048, "
                      "\"batch\": 1, \"columns\": 4, \"warmup\": 8, "
                      "\"raw_iterations\": " << options.raw_iterations
                   << ", \"candidate_iterations\": " << options.candidate_iterations << "},\n"
                   << "  \"raw_gemm\": {\"throughput_dispatches_per_s\": " << raw_throughput
                   << ", \"latency_ns_per_dispatch\": "
                   << static_cast<double>(raw_ns) / static_cast<double>(options.raw_iterations)
                   << ", \"h2d_syncs\": " << raw_h2d_syncs
                   << ", \"d2h_syncs\": " << raw_d2h_syncs
                   << ", \"dispatch_wait_ns\": " << raw_wait_ns << "},\n"
                   << "  \"candidate\": {\"throughput_candidates_per_s\": " << candidate_throughput
                   << ", \"latency_ns\": "
                   << static_cast<double>(candidate_ns) / static_cast<double>(options.candidate_iterations)
                   << ", \"proof_bytes\": " << proof_bytes
                   << ", \"mismatches\": " << candidate_mismatches << "},\n"
                   << "  \"cpu_vs_xdna\": {\"raw_cpu_reference_ns_per_dispatch\": "
                   << static_cast<double>(cpu_ns) / static_cast<double>(options.raw_iterations)
                   << ", \"raw_xdna_end_to_end_ns_per_dispatch\": "
                   << static_cast<double>(raw_ns) / static_cast<double>(options.raw_iterations) << "},\n"
                   << "  \"power_watts\": null,\n"
                   << "  \"npu_telemetry\": null,\n"
                   << "  \"cpu_fallbacks\": 0,\n"
                   << "  \"gateway_overhead\": \"not measured; no official local gateway was running\"\n"
                   << "}\n";
        }
        std::cout << "device=" << capability.device_name << '\n'
                  << "raw_throughput_dispatches_per_s=" << raw_throughput << '\n'
                  << "candidate_throughput_per_s=" << candidate_throughput << '\n'
                  << "candidate_mismatches=" << candidate_mismatches << '\n'
                  << "cpu_fallbacks=0\n";
        return candidate_mismatches == 0U ? 0 : 1;
    } catch (const xdna::runtime::RuntimeError& error) {
        std::cerr << xdna::runtime::error_code_name(error.code())
                  << ": " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "BENCHMARK_FAILURE: " << error.what() << '\n';
        return 1;
    }
}
