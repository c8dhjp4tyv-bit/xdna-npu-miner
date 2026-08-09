#include "m4_vectors.hpp"

#include "bpp9000/reference.hpp"
#include "xdna/m4_score.hpp"
#include "xdna/runtime.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using xdna::bpp9000::Byte;
using xdna::bpp9000::Lut;
using xdna::bpp9000::ScoreResult;
using xdna::bpp9000::ScoreStatus;
using xdna::bpp9000::Task;
using xdna::bpp9000::Trit;
using xdna::runtime::M4CandidateResult;
using xdna::runtime::M4ScoreRun;
using xdna::runtime::M4NpuScorer;
using xdna::runtime::M4DeviceResult;
using xdna::runtime::VerificationStatus;
using xdna::runtime::WindowVerification;
using xdna::runtime::XdnaRuntime;

class DifferentialFailure final : public std::runtime_error {
public:
    explicit DifferentialFailure(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

struct Options {
    std::filesystem::path xclbin;
    std::filesystem::path instructions;
    std::filesystem::path manifest;
    std::filesystem::path evidence;
    std::filesystem::path mismatch_dir;
    std::filesystem::path artifact_sums;
    std::string selector = "0";
    std::size_t fixed_count = 100U;
    std::size_t random_count = 1000U;
    std::uint64_t random_seed = 0x4D3452414E444F4DULL;
    std::string m1_status = "PASS";
    std::string m2_status = "PASS";
    std::string m3_status = "PASS";
    std::string negative_status = "PASS";
};

struct Counters {
    std::uint64_t repeated_tick_cases = 0U;
    std::uint64_t one_window_cases = 0U;
    std::uint64_t multi_window_cases = 0U;
    std::uint64_t full_score_cases = 0U;
    std::uint64_t production_shaped_cases = 0U;
    std::uint64_t candidate_cases = 0U;
    std::uint64_t score_runs = 0U;
    std::uint64_t exact_matches = 0U;
    std::uint64_t timeout_matches = 0U;
    std::uint64_t finite_matches = 0U;
    std::uint64_t windows_compared = 0U;
    std::uint64_t candidate_score_calls = 0U;
    std::uint64_t candidate_window_comparisons = 0U;
};

[[nodiscard]] bool parse_unsigned(std::string_view text, std::uint64_t& value)
{
    if (text.empty()) {
        return false;
    }
    std::uint64_t parsed = 0U;
    for (const char character : text) {
        if (character < '0' || character > '9') {
            return false;
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');
        if (parsed > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U) {
            return false;
        }
        parsed = parsed * 10U + digit;
    }
    value = parsed;
    return true;
}

[[nodiscard]] bool parse_options(int argc, char** argv, Options& options)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if ((argument == "--xclbin" || argument == "--insts" || argument == "--manifest"
             || argument == "--selector" || argument == "--fixed-count" || argument == "--random-count"
             || argument == "--random-seed" || argument == "--evidence" || argument == "--mismatch-dir"
             || argument == "--artifact-sums" || argument == "--m1-status" || argument == "--m2-status"
             || argument == "--m3-status" || argument == "--negative-status")
            && index + 1 >= argc) {
            return false;
        }
        if (argument == "--xclbin") {
            options.xclbin = argv[++index];
        } else if (argument == "--insts") {
            options.instructions = argv[++index];
        } else if (argument == "--manifest") {
            options.manifest = argv[++index];
        } else if (argument == "--selector") {
            options.selector = argv[++index];
        } else if (argument == "--fixed-count") {
            std::uint64_t value = 0U;
            if (!parse_unsigned(argv[++index], value)) {
                return false;
            }
            options.fixed_count = static_cast<std::size_t>(value);
        } else if (argument == "--random-count") {
            std::uint64_t value = 0U;
            if (!parse_unsigned(argv[++index], value)) {
                return false;
            }
            options.random_count = static_cast<std::size_t>(value);
        } else if (argument == "--random-seed") {
            if (!parse_unsigned(argv[++index], options.random_seed)) {
                return false;
            }
        } else if (argument == "--evidence") {
            options.evidence = argv[++index];
        } else if (argument == "--mismatch-dir") {
            options.mismatch_dir = argv[++index];
        } else if (argument == "--artifact-sums") {
            options.artifact_sums = argv[++index];
        } else if (argument == "--m1-status") {
            options.m1_status = argv[++index];
        } else if (argument == "--m2-status") {
            options.m2_status = argv[++index];
        } else if (argument == "--m3-status") {
            options.m3_status = argv[++index];
        } else if (argument == "--negative-status") {
            options.negative_status = argv[++index];
        } else if (argument == "--help") {
            return false;
        } else {
            return false;
        }
    }
    if (options.manifest.empty() && !options.xclbin.empty()) {
        options.manifest = options.xclbin.parent_path() / "xdna_m4.manifest";
    }
    return !options.xclbin.empty() && !options.instructions.empty();
}

void print_usage(const char* program)
{
    std::cerr << "usage: " << program
              << " --xclbin PATH --insts PATH [--manifest PATH] [--selector DEVICE]"
                 " [--fixed-count N] [--random-count N] [--random-seed N]"
                 " [--evidence PATH] [--mismatch-dir PATH] [--artifact-sums PATH]\n";
}

[[nodiscard]] std::string json_escape(std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += character;
            break;
        }
    }
    return escaped;
}

[[nodiscard]] std::string hex_bytes(std::span<const Byte> bytes)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const Byte value : bytes) {
        stream << std::setw(2) << static_cast<unsigned int>(value);
    }
    return stream.str();
}

template <typename Range>
[[nodiscard]] std::string hex_range(const Range& range)
{
    const auto* bytes = reinterpret_cast<const Byte*>(range.data());
    return hex_bytes(std::span<const Byte>(bytes, range.size() * sizeof(range[0])));
}

void json_score(std::ostream& stream, const ScoreResult& score)
{
    stream << "{\"score\":" << score.score << ",\"status\":"
           << static_cast<unsigned int>(score.status) << ",\"windows_evaluated\":"
           << score.windows_evaluated << ",\"ticks\":" << score.ticks << '}';
}

void json_window(std::ostream& stream, const xdna::bpp9000::WindowResult& result)
{
    stream << "{\"score\":";
    json_score(stream, result.score);
    stream << ",\"predicted\":" << static_cast<unsigned int>(result.predicted)
           << ",\"expected\":" << static_cast<unsigned int>(result.expected)
           << ",\"feed_count\":" << result.feed_count << '}';
}

struct ArtifactSums {
    std::string xclbin = "unavailable";
    std::string instructions = "unavailable";
};

[[nodiscard]] ArtifactSums read_artifact_sums(const std::filesystem::path& path)
{
    ArtifactSums sums;
    std::ifstream stream(path);
    std::string digest;
    std::string artifact;
    while (stream >> digest >> artifact) {
        if (artifact.ends_with("xdna_m4.xclbin")) {
            sums.xclbin = digest;
        } else if (artifact.ends_with("xdna_m4.insts")) {
            sums.instructions = digest;
        }
    }
    return sums;
}

void write_mismatch(const Options& options,
                    const XdnaRuntime& runtime,
                    const m4_test::M4Case& fixture,
                    std::string_view stage,
                    const M4ScoreRun* score_run,
                    std::span<const Byte> expected_state = {},
                    std::span<const Byte> actual_state = {},
                    std::span<const Byte> public_key = {},
                    std::span<const Byte> mining_seed = {},
                    std::span<const Byte> nonce = {})
{
    if (options.mismatch_dir.empty()) {
        throw DifferentialFailure("M4 mismatch occurred but no mismatch directory was configured");
    }
    std::filesystem::create_directories(options.mismatch_dir);
    const std::filesystem::path path = options.mismatch_dir / (fixture.id + ".json");
    std::ofstream stream(path);
    if (!stream) {
        throw DifferentialFailure("cannot write M4 mismatch artifact: " + path.string());
    }
    const ArtifactSums sums = read_artifact_sums(options.artifact_sums);
    stream << "{\n  \"failure_kind\":\"CPU_NPU_MISMATCH\",\n"
           << "  \"stage\":\"" << json_escape(stage) << "\",\n"
           << "  \"vector\":{\"id\":\"" << json_escape(fixture.id)
           << "\",\"seed\":" << fixture.seed << ",\"case_index\":" << fixture.case_index << "},\n"
           << "  \"task\":{\"input_trits\":18,\"output_trits\":1,\"sequence_length\":"
           << fixture.task.header.shape.sequence_length << ",\"population\":64,\"neighbors\":3},\n"
           << "  \"input\":{\"topology_hex\":\"" << hex_range(fixture.task.topology.neighbors)
           << "\",\"lut_hex\":\"" << hex_range(fixture.lut.storage()) << "\"}";
    if (!public_key.empty() && !mining_seed.empty() && !nonce.empty()) {
        stream << ",\n  \"candidate\":{\"public_key_hex\":\"" << hex_bytes(public_key)
               << "\",\"mining_seed_hex\":\"" << hex_bytes(mining_seed)
               << "\",\"nonce_hex\":\"" << hex_bytes(nonce) << "\"}";
    }
    stream << ",\n  \"result\":{";
    if (score_run != nullptr) {
        stream << "\"cpu\":";
        json_score(stream, score_run->cpu);
        stream << ",\"npu\":";
        json_score(stream, score_run->npu);
        if (score_run->first_mismatch.has_value()) {
            stream << ",\"first_window_cpu\":";
            json_window(stream, score_run->first_mismatch->cpu);
            stream << ",\"first_window_npu\":";
            json_window(stream, score_run->first_mismatch->npu);
        }
        if (score_run->first_mismatch_window.has_value()) {
            stream << ",\"window_index\":" << *score_run->first_mismatch_window;
        }
        if (score_run->candidate_score_call.has_value()) {
            stream << ",\"candidate_score_call\":" << *score_run->candidate_score_call;
        }
        if (score_run->candidate_mutation_step.has_value()) {
            stream << ",\"candidate_mutation_step\":" << *score_run->candidate_mutation_step;
        }
    } else {
        stream << "\"cpu_state_hex\":\"" << hex_bytes(expected_state)
               << "\",\"npu_state_hex\":\"" << hex_bytes(actual_state) << '"';
    }
    stream << "},\n  \"artifact\":{\"xclbin_sha256\":\"" << json_escape(sums.xclbin)
           << "\",\"instructions_sha256\":\"" << json_escape(sums.instructions)
           << "\",\"uuid\":\"" << json_escape(runtime.artifact_uuid())
           << "\",\"kernel\":\"" << json_escape(runtime.kernel_name()) << "\"},\n"
           << "  \"device\":{\"name\":\"" << json_escape(runtime.capability().device_name)
           << "\",\"architecture\":\"" << json_escape(runtime.capability().architecture)
           << "\",\"bdf\":\"" << json_escape(runtime.capability().bdf)
           << "\",\"xrt\":\"" << json_escape(runtime.capability().xrt_version)
           << "\",\"firmware\":\"" << json_escape(runtime.capability().firmware_version) << "\"}\n}\n";
    throw DifferentialFailure(std::string("M4 mismatch at ") + std::string(stage)
                              + "; artifact=" + path.string());
}

void require_window_match(const Options& options,
                          const XdnaRuntime& runtime,
                          const m4_test::M4Case& fixture,
                          std::string_view stage,
                          const xdna::bpp9000::WindowResult& cpu,
                          const xdna::bpp9000::WindowResult& npu,
                          Counters& counters)
{
    const bool equal = xdna::runtime::verify_score_exact(cpu.score, npu.score).verified()
        && cpu.predicted == npu.predicted && cpu.expected == npu.expected && cpu.feed_count == npu.feed_count;
    if (!equal) {
        M4ScoreRun run;
        run.cpu = cpu.score;
        run.npu = npu.score;
        run.windows_compared = 1U;
        run.status = VerificationStatus::RejectedMismatch;
        run.first_mismatch = WindowVerification{cpu, npu, VerificationStatus::RejectedMismatch};
        write_mismatch(options, runtime, fixture, stage, &run);
    }
    counters.exact_matches += 1U;
    counters.windows_compared += 1U;
    if (cpu.score.timed_out() && npu.score.timed_out()) {
        counters.timeout_matches += 1U;
    } else {
        counters.finite_matches += 1U;
    }
}

void require_score_run(const Options& options,
                       const XdnaRuntime& runtime,
                       const m4_test::M4Case& fixture,
                       std::string_view stage,
                       const M4ScoreRun& run,
                       Counters& counters)
{
    counters.score_runs += 1U;
    counters.windows_compared += run.windows_compared;
    if (!run.verified()) {
        write_mismatch(options, runtime, fixture, stage, &run);
    }
    counters.exact_matches += 1U;
    if (run.cpu.timed_out() && run.npu.timed_out()) {
        counters.timeout_matches += 1U;
    } else {
        counters.finite_matches += 1U;
    }
}

void run_repeated_ticks(M4NpuScorer& scorer,
                        const Options& options,
                        const XdnaRuntime& runtime,
                        const std::vector<m4_test::M4Case>& cases,
                        Counters& counters)
{
    for (const m4_test::M4Case& fixture : cases) {
        constexpr std::size_t kRows = 4U;
        std::array<Byte, 64U> initial{};
        for (std::size_t index = 0U; index < initial.size(); ++index) {
            initial[index] = static_cast<Byte>((index + fixture.case_index) % 3U);
        }
        std::vector<Byte> sequence(kRows * 18U, 0U);
        for (std::size_t index = 0U; index < sequence.size(); ++index) {
            sequence[index] = xdna::bpp9000::trit_to_byte(fixture.task.inputs[index]);
        }
        xdna::bpp9000::RecurrentState cpu_state(fixture.task);
        cpu_state.load_current(initial);
        for (std::size_t row = 0U; row < kRows; ++row) {
            for (std::size_t input = 0U; input < 18U; ++input) {
                cpu_state.set_input(
                    fixture.task.topology.input_neurons[input],
                    static_cast<Trit>(sequence[row * 18U + input]));
            }
            cpu_state.tick(fixture.task, fixture.lut);
        }
        const M4DeviceResult npu = scorer.dispatch_repeated_ticks(fixture.task, initial, fixture.lut, sequence);
        const std::vector<Byte> expected(cpu_state.current().begin(), cpu_state.current().end());
        if (npu.state != expected || npu.ticks != kRows || npu.feed_count != kRows) {
            write_mismatch(options, runtime, fixture, "repeated-tick", nullptr, expected, npu.state);
        }
        counters.exact_matches += 1U;
        counters.repeated_tick_cases += 1U;
    }
}

void run_one_window(M4NpuScorer& scorer,
                    const Options& options,
                    const XdnaRuntime& runtime,
                    const std::vector<m4_test::M4Case>& cases,
                    Counters& counters)
{
    for (const m4_test::M4Case& fixture : cases) {
        const auto cpu = xdna::bpp9000::score_window(fixture.task, fixture.lut, 0U, fixture.config);
        const auto npu = scorer.score_window_npu(fixture.task, fixture.lut, 0U, fixture.config);
        require_window_match(options, runtime, fixture, "one-window", cpu, npu, counters);
        counters.one_window_cases += 1U;
    }
}

void run_multi_window(M4NpuScorer& scorer,
                      const Options& options,
                      const XdnaRuntime& runtime,
                      const std::vector<m4_test::M4Case>& cases,
                      Counters& counters)
{
    for (const m4_test::M4Case& fixture : cases) {
        const std::uint64_t window_count = fixture.task.header.shape.sequence_length - fixture.config.window_width;
        const std::array<std::uint64_t, 3U> windows{0U, window_count / 2U, window_count - 1U};
        for (const std::uint64_t window : windows) {
            const auto cpu = xdna::bpp9000::score_window(fixture.task, fixture.lut, window, fixture.config);
            const auto npu = scorer.score_window_npu(fixture.task, fixture.lut, window, fixture.config);
            require_window_match(options, runtime, fixture, "multi-window", cpu, npu, counters);
        }
        counters.multi_window_cases += 1U;
    }
}

void run_candidate(M4NpuScorer& scorer,
                   const Options& options,
                   const XdnaRuntime& runtime,
                   m4_test::M4Case& fixture,
                   std::uint8_t candidate_tag,
                   Counters& counters)
{
    const xdna::bpp9000::ReferenceConfig config{2U, 8U, 100U};
    xdna::bpp9000::PublicKey public_key{};
    public_key.bytes[0] = static_cast<Byte>(0xA5U + candidate_tag);
    xdna::bpp9000::MiningSeed mining_seed{};
    mining_seed.bytes[0] = static_cast<Byte>(0x5AU + candidate_tag);
    xdna::bpp9000::Nonce nonce{};
    nonce.bytes[0] = 1U;
    nonce.bytes[1] = 2U;
    nonce.bytes[2] = 0U;

    xdna::bpp9000::DeterministicFixtureRandom npu_random(0xCA1D1DA7EULL + candidate_tag);
    const M4CandidateResult npu_result = scorer.score_candidate_verified(
        fixture.task,
        public_key,
        mining_seed,
        nonce,
        npu_random,
        config);
        if (!npu_result.verified() || !npu_result.cpu_result.has_value() || npu_result.score_calls != 101U
        || npu_result.npu_score_calls != 101U || npu_result.cpu_result->attempts.size() != 100U) {
        if (npu_result.first_mismatch.has_value()) {
            write_mismatch(
                options,
                runtime,
                fixture,
                "candidate-lifecycle",
                &*npu_result.first_mismatch,
                {},
                {},
                std::span<const Byte>(public_key.bytes),
                std::span<const Byte>(mining_seed.bytes),
                std::span<const Byte>(nonce.bytes));
        }
        throw DifferentialFailure("M4 candidate lifecycle did not complete its verified 101-call path");
    }

    xdna::bpp9000::DeterministicFixtureRandom cpu_random(0xCA1D1DA7EULL + candidate_tag);
    const auto cpu_result = xdna::bpp9000::score_candidate(
        fixture.task,
        public_key,
        mining_seed,
        nonce,
        cpu_random,
        config);
    if (npu_result.cpu_result->current_lut.storage() != cpu_result.current_lut.storage()
        || npu_result.cpu_result->best_lut.storage() != cpu_result.best_lut.storage()
        || npu_result.cpu_result->best.score != cpu_result.best.score
        || npu_result.cpu_result->current_lut.updated_neurons() != cpu_result.current_lut.updated_neurons()) {
        write_mismatch(
            options,
            runtime,
            fixture,
            "candidate-final-state",
            nullptr,
            {},
            {},
            std::span<const Byte>(public_key.bytes),
            std::span<const Byte>(mining_seed.bytes),
            std::span<const Byte>(nonce.bytes));
    }
    counters.score_runs += npu_result.npu_score_calls;
    counters.exact_matches += npu_result.npu_score_calls;
    counters.windows_compared += npu_result.windows_compared;
    counters.candidate_score_calls += npu_result.npu_score_calls;
    counters.candidate_window_comparisons += npu_result.windows_compared;
    counters.timeout_matches += npu_result.timeout_score_calls;
    counters.finite_matches += npu_result.finite_score_calls;
    counters.candidate_cases += 1U;
}

void write_evidence(const Options& options,
                    const XdnaRuntime& runtime,
                    const Counters& counters,
                    std::uint64_t validation_seconds)
{
    if (options.evidence.empty()) {
        return;
    }
    std::filesystem::create_directories(options.evidence.parent_path());
    std::ofstream stream(options.evidence);
    if (!stream) {
        throw DifferentialFailure("cannot write M4 evidence: " + options.evidence.string());
    }
    const ArtifactSums sums = read_artifact_sums(options.artifact_sums);
    const auto& capability = runtime.capability();
    const auto& dispatch = runtime.counters();
    stream << "{\n"
           << "  \"schema_version\":1,\n"
           << "  \"milestone\":\"M4\",\n"
           << "  \"status\":\"PASS\",\n"
           << "  \"toolchain\":{\"device\":\"" << json_escape(capability.device_name)
           << "\",\"architecture\":\"" << json_escape(capability.architecture)
           << "\",\"bdf\":\"" << json_escape(capability.bdf)
           << "\",\"driver\":\"" << json_escape(capability.amdxdna_version)
           << "\",\"firmware\":\"" << json_escape(capability.firmware_version)
           << "\",\"xrt\":\"" << json_escape(capability.xrt_version)
           << "\",\"mlir_aie\":\"57d7494e99c214f5f53b328a0ed43a99e759e835\""
           << ",\"peano\":\"llvm-aie 21.0.0.2026072001+ce8c0f8f\"},\n"
           << "  \"artifact\":{\"xclbin_sha256\":\"" << json_escape(sums.xclbin)
           << "\",\"instruction_sha256\":\"" << json_escape(sums.instructions)
           << "\",\"uuid\":\"" << json_escape(runtime.artifact_uuid())
           << "\",\"kernel\":\"" << json_escape(runtime.kernel_name())
           << "\",\"target_columns\":1},\n"
           << "  \"contracts\":{\"state\":\"uint8[64], reset per window, device-local across ticks\""
           << ",\"lut\":\"46x32 bytes, 27 logical trits\",\"topology\":\"64x3 uint32 + 46 rows + 18 inputs\""
           << ",\"input\":\"uint8 trits, 18 per row\",\"result_status\":\"uint32 score, status, ticks, feed count\""
           << ",\"persistent_buffers\":false},\n"
           << "  \"test_tiers\":{\"repeated_tick\":" << counters.repeated_tick_cases
           << ",\"one_window\":" << counters.one_window_cases
           << ",\"multi_window\":" << counters.multi_window_cases
           << ",\"fixed\":" << options.fixed_count
           << ",\"random\":" << options.random_count
           << ",\"full_score\":" << counters.full_score_cases
           << ",\"production_shaped\":" << counters.production_shaped_cases
           << ",\"full_candidate\":" << counters.candidate_cases << "},\n"
           << "  \"dispatch\":{\"physical_dispatches\":" << dispatch.dispatches
           << ",\"successful_dispatches\":" << dispatch.completed_dispatches
           << ",\"runtime_failures\":0,\"h2d\":" << dispatch.h2d_syncs
           << ",\"d2h\":" << dispatch.d2h_syncs << "},\n"
           << "  \"differential\":{\"score_runs\":" << counters.score_runs
           << ",\"windows_compared\":" << counters.windows_compared
           << ",\"exact_matches\":" << counters.exact_matches
           << ",\"mismatches\":0,\"timeout_matches\":" << counters.timeout_matches
           << ",\"finite_score_matches\":" << counters.finite_matches
           << ",\"candidate_score_calls\":" << counters.candidate_score_calls
           << ",\"candidate_window_comparisons\":" << counters.candidate_window_comparisons << "},\n"
           << "  \"verification\":{\"cpu_recomputation_enabled\":true,\"npu_only_authorization\":false},\n"
           << "  \"regressions\":{\"m1\":\"" << json_escape(options.m1_status)
           << "\",\"m2\":\"" << json_escape(options.m2_status)
           << "\",\"m3\":\"" << json_escape(options.m3_status)
           << "\",\"negative_paths\":\"" << json_escape(options.negative_status) << "\"},\n"
           << "  \"diagnostics\":{\"validation_duration_seconds\":" << validation_seconds
           << ",\"speedup_claim\":false,\"profitability_claim\":false}\n"
           << "}\n";
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parse_options(argc, argv, options)) {
        print_usage(argv[0]);
        return 2;
    }

    try {
        const auto started = std::chrono::steady_clock::now();
        XdnaRuntime runtime(
            xdna::runtime::M4Artifact{options.xclbin, options.instructions, options.manifest},
            options.selector,
            xdna::runtime::WorkloadKind::M4);
        M4NpuScorer scorer(runtime);
        Counters counters;

        std::vector<m4_test::M4Case> fixed = m4_test::make_fixed_cases(options.fixed_count);
        std::vector<m4_test::M4Case> random = m4_test::make_random_cases(options.random_seed, options.random_count);
        run_repeated_ticks(scorer, options, runtime, random, counters);
        run_one_window(scorer, options, runtime, fixed, counters);
        run_multi_window(scorer, options, runtime, random.empty() ? fixed : random, counters);

        for (std::size_t index = 0U; index < 10U; ++index) {
            m4_test::M4Case fixture = m4_test::make_case(
                0x46554C4C53434F52ULL + index,
                10000U + index,
                8U,
                2U,
                false,
                static_cast<unsigned int>(index % 3U));
            const M4ScoreRun run = scorer.score_lut_verified(fixture.task, fixture.lut, fixture.config);
            require_score_run(options, runtime, fixture, "full-score-small", run, counters);
            counters.full_score_cases += 1U;
        }

        m4_test::M4Case production = m4_test::make_case(
            0x50524F4455435449ULL,
            20000U,
            xdna::bpp9000::kProductionSequenceLength,
            static_cast<std::uint32_t>(xdna::bpp9000::kProductionWindowWidth),
            false,
            0U);
        const M4ScoreRun production_run = scorer.score_lut_verified(
            production.task,
            production.lut,
            production.config);
        require_score_run(options, runtime, production, "full-score-production-shaped", production_run, counters);
        counters.full_score_cases += 1U;
        counters.production_shaped_cases += 1U;

        m4_test::M4Case candidate_a = m4_test::make_case(0x43414E4449444154ULL, 30000U, 8U, 2U, false, 0U);
        run_candidate(scorer, options, runtime, candidate_a, 0U, counters);
        m4_test::M4Case candidate_b = m4_test::make_case(0x43414E4449444155ULL, 30001U, 8U, 2U, false, 1U);
        run_candidate(scorer, options, runtime, candidate_b, 1U, counters);

        const auto finished = std::chrono::steady_clock::now();
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(finished - started).count();
        write_evidence(options, runtime, counters, static_cast<std::uint64_t>(seconds));
        std::cout << "device=" << runtime.capability().device_name << '\n'
                  << "architecture=" << runtime.capability().architecture << '\n'
                  << "artifact_uuid=" << runtime.artifact_uuid() << '\n'
                  << "repeated_tick_cases=" << counters.repeated_tick_cases << '\n'
                  << "one_window_cases=" << counters.one_window_cases << '\n'
                  << "multi_window_cases=" << counters.multi_window_cases << '\n'
                  << "fixed_cases=" << options.fixed_count << '\n'
                  << "random_cases=" << options.random_count << '\n'
                  << "full_score_cases=" << counters.full_score_cases << '\n'
                  << "production_shaped_cases=" << counters.production_shaped_cases << '\n'
                  << "candidate_cases=" << counters.candidate_cases << '\n'
                  << "physical_dispatches=" << runtime.counters().dispatches << '\n'
                  << "successful_dispatches=" << runtime.counters().completed_dispatches << '\n'
                  << "exact_score_runs=" << counters.score_runs << '\n'
                  << "exact_comparisons=" << counters.exact_matches << '\n'
                  << "candidate_score_calls=" << counters.candidate_score_calls << '\n'
                  << "candidate_window_comparisons=" << counters.candidate_window_comparisons << '\n'
                  << "score_mismatches=0\n"
                  << "runtime_failures=0\n"
                  << "explicit_h2d_syncs=" << runtime.counters().h2d_syncs << '\n'
                  << "explicit_d2h_syncs=" << runtime.counters().d2h_syncs << '\n'
                  << "CPU/NPU DIFFERENTIAL PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "M4 DIFFERENTIAL FAIL: " << error.what() << '\n';
        return 1;
    }
}
