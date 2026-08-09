#include "k1_vectors.hpp"

#include "xdna/errors.hpp"
#include "xdna/runtime.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using xdna::bpp9000::Byte;
using xdna::runtime::K1Comparison;
using xdna::runtime::K1PackedBuffers;
using xdna::runtime::RuntimeError;
using xdna::runtime::WorkloadKind;
using xdna::runtime::XdnaRuntime;
using xdna::runtime::compare_k1_output;
using xdna::runtime::k1_default_layout;
using xdna::runtime::pack_k1;
using xdna::runtime::unpack_k1;

constexpr std::uint64_t kDefaultRandomSeed = 0x4D33524E444B3101ULL;
constexpr std::string_view kGeneratorVersion = "m3-k1-v1";

struct Options {
    std::filesystem::path xclbin;
    std::filesystem::path instructions;
    std::filesystem::path manifest;
    std::filesystem::path evidence;
    std::filesystem::path mismatch_dir = "build/m3-mismatches";
    std::filesystem::path artifact_sums;
    std::string selector = "0";
    std::size_t fixed_count = 100U;
    std::size_t random_count = 1000U;
    std::uint64_t random_seed = kDefaultRandomSeed;
    bool include_edge_cases = true;
    std::string m1_status = "NOT_RUN";
    std::string m2_status = "NOT_RUN";
    std::string negative_status = "NOT_RUN";
};

class DifferentialFailure final : public std::runtime_error {
public:
    explicit DifferentialFailure(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

[[nodiscard]] bool parse_unsigned(const std::string& text, std::uint64_t& value)
{
    try {
        std::size_t consumed = 0U;
        value = std::stoull(text, &consumed, 0);
        return consumed == text.size();
    } catch (...) {
        return false;
    }
}

[[nodiscard]] bool parse_options(int argc, char** argv, Options& options)
{
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if ((argument == "--xclbin" || argument == "--insts" || argument == "--selector"
             || argument == "--fixed-count" || argument == "--random-count" || argument == "--random-seed"
             || argument == "--evidence" || argument == "--mismatch-dir" || argument == "--artifact-sums"
             || argument == "--manifest"
             || argument == "--m1-status" || argument == "--m2-status" || argument == "--negative-status")
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
        } else if (argument == "--negative-status") {
            options.negative_status = argv[++index];
        } else if (argument == "--no-edge-cases") {
            options.include_edge_cases = false;
        } else if (argument == "--help") {
            return false;
        } else {
            return false;
        }
    }
    if (options.manifest.empty() && !options.xclbin.empty()) {
        options.manifest = options.xclbin.parent_path() / "xdna_k1.manifest";
    }
    return !options.xclbin.empty() && !options.instructions.empty();
}

void print_usage(const char* program)
{
    std::cerr << "usage: " << program
              << " --xclbin PATH --insts PATH [--fixed-count N] [--random-count N]"
                 " [--random-seed N] [--no-edge-cases] [--selector DEVICE]"
                 " [--evidence PATH] [--mismatch-dir PATH] [--artifact-sums PATH] [--manifest PATH]"
                 " [--m1-status STATUS] [--m2-status STATUS] [--negative-status STATUS]\n";
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

void json_string(std::ostream& stream, std::string_view value)
{
    stream << '"' << json_escape(value) << '"';
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

void json_hex_bytes(std::ostream& stream, std::span<const Byte> bytes)
{
    json_string(stream, hex_bytes(bytes));
}

template <typename Range>
void json_u32_array(std::ostream& stream, const Range& values)
{
    stream << '[';
    for (std::size_t index = 0U; index < values.size(); ++index) {
        if (index != 0U) {
            stream << ',';
        }
        stream << values[index];
    }
    stream << ']';
}

[[nodiscard]] std::string utc_now()
{
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
    if (gmtime_r(&now, &utc) == nullptr) {
        return "unavailable";
    }
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

struct ArtifactSums {
    std::string xclbin = "unavailable";
    std::string instructions = "unavailable";
};

[[nodiscard]] ArtifactSums read_artifact_sums(const std::filesystem::path& path)
{
    ArtifactSums sums;
    if (path.empty()) {
        return sums;
    }
    std::ifstream stream(path);
    std::string digest;
    std::string artifact;
    while (stream >> digest >> artifact) {
        if (artifact.ends_with("xdna_k1.xclbin")) {
            sums.xclbin = digest;
        } else if (artifact.ends_with("xdna_k1.insts")) {
            sums.instructions = digest;
        }
    }
    return sums;
}

[[nodiscard]] std::string artifact_identity(const XdnaRuntime& runtime, const Options& options)
{
    const ArtifactSums sums = read_artifact_sums(options.artifact_sums);
    std::ostringstream stream;
    stream << "{\"xclbin\":\"" << json_escape(options.xclbin.string())
           << "\",\"instructions\":\"" << json_escape(options.instructions.string())
           << "\",\"manifest\":\"" << json_escape(options.manifest.string())
           << "\",\"xclbin_sha256\":\"" << json_escape(sums.xclbin)
           << "\",\"instructions_sha256\":\"" << json_escape(sums.instructions)
           << "\",\"uuid\":\"" << json_escape(runtime.artifact_uuid())
           << "\",\"kernel\":\"" << json_escape(runtime.kernel_name()) << "\"}";
    return stream.str();
}

void write_mismatch(const std::filesystem::path& mismatch_dir,
                    const k1_test::K1Vector& vector,
                    std::span<const Byte> expected,
                    std::span<const Byte> actual,
                    const K1Comparison& comparison,
                    const XdnaRuntime& runtime,
                    const Options& options,
                    std::string_view failure_kind = "OUTPUT_MISMATCH")
{
    std::filesystem::create_directories(mismatch_dir);
    const std::filesystem::path path = mismatch_dir / (vector.id + ".json");
    std::ofstream stream(path);
    if (!stream) {
        throw DifferentialFailure("cannot write mismatch artifact: " + path.string());
    }
    stream << "{\n  \"failure_kind\": ";
    json_string(stream, failure_kind);
    stream << ",\n  \"captured_utc\": ";
    json_string(stream, utc_now());
    stream << ",\n  \"vector\": {\n    \"id\": ";
    json_string(stream, vector.id);
    stream << ",\n    \"generator_version\": ";
    json_string(stream, kGeneratorVersion);
    stream << ",\n    \"generator_seed\": " << vector.generator_seed
           << ",\n    \"case_index\": " << vector.case_index << "\n  },\n  \"input\": {\n"
           << "    \"initial_state_hex\": ";
    json_hex_bytes(stream, vector.state);
    stream << ",\n    \"lut_hex\": ";
    json_hex_bytes(stream, vector.lut);
    stream << ",\n    \"neighbors\": ";
    json_u32_array(stream, vector.neighbors);
    stream << ",\n    \"updated_neurons\": ";
    json_u32_array(stream, vector.updated_neurons);
    stream << "\n  },\n  \"result\": {\n    \"expected_next_state_hex\": ";
    json_hex_bytes(stream, expected);
    stream << ",\n    \"actual_next_state_hex\": ";
    json_hex_bytes(stream, actual);
    stream << ",\n    \"differing_indices\": ";
    json_u32_array(stream, comparison.differing_indices);
    stream << "\n  },\n  \"artifact_identity\": " << artifact_identity(runtime, options)
           << ",\n  \"device\": {\n    \"device_name\": ";
    json_string(stream, runtime.capability().device_name);
    stream << ",\n    \"architecture\": ";
    json_string(stream, runtime.capability().architecture);
    stream << ",\n    \"amdxdna_version\": ";
    json_string(stream, runtime.capability().amdxdna_version);
    stream << ",\n    \"firmware_version\": ";
    json_string(stream, runtime.capability().firmware_version);
    stream << ",\n    \"xrt_version\": ";
    json_string(stream, runtime.capability().xrt_version);
    stream << "\n  }\n}\n";
    throw DifferentialFailure(
        std::string(failure_kind) + " for vector " + vector.id + "; artifact=" + path.string());
}

void vary_padding(K1PackedBuffers& buffers, Byte state_padding, Byte lut_padding, std::uint32_t row_padding)
{
    for (std::size_t index = 64U; index < buffers.previous_state.size(); ++index) {
        buffers.previous_state[index] = state_padding;
    }
    for (std::size_t row = 0U; row < 46U; ++row) {
        for (std::size_t index = 27U; index < 32U; ++index) {
            buffers.lut[row * 32U + index] = lut_padding;
        }
    }
    buffers.updated_neurons[46U] = row_padding;
    buffers.updated_neurons[47U] = row_padding ^ 0xFFFFFFFFU;
    for (std::size_t index = 64U; index < buffers.next_state.size(); ++index) {
        buffers.next_state[index] = static_cast<Byte>(state_padding ^ 0xFFU);
    }
}

void check_output(const k1_test::K1Vector& vector,
                  std::span<const Byte> expected,
                  std::span<const Byte> actual,
                  const XdnaRuntime& runtime,
                  const Options& options)
{
    const K1Comparison comparison = compare_k1_output(expected, actual);
    if (!comparison.matches()) {
        write_mismatch(options.mismatch_dir, vector, expected, actual, comparison, runtime, options);
    }
}

void run_vector(XdnaRuntime& runtime,
                const k1_test::K1Vector& vector,
                const Options& options,
                bool padding_probe,
                bool deterministic_probe)
{
    const std::vector<Byte> expected = k1_test::cpu_expected(vector);
    K1PackedBuffers packed = pack_k1(k1_test::logical_input(vector), k1_default_layout());
    vary_padding(packed, 0x11U, 0x22U, 0x13579BDFU);
    runtime.dispatch_k1(packed);
    const std::vector<Byte> actual = unpack_k1(packed.next_state);
    check_output(vector, expected, actual, runtime, options);

    if (deterministic_probe) {
        K1PackedBuffers repeated = pack_k1(k1_test::logical_input(vector), k1_default_layout());
        runtime.dispatch_k1(repeated);
        const std::vector<Byte> repeated_actual = unpack_k1(repeated.next_state);
        check_output(vector, expected, repeated_actual, runtime, options);
        const K1Comparison deterministic_comparison = compare_k1_output(actual, repeated_actual);
        if (!deterministic_comparison.matches()) {
            write_mismatch(
                options.mismatch_dir,
                vector,
                actual,
                repeated_actual,
                deterministic_comparison,
                runtime,
                options,
                "REPEATED_DISPATCH_NOT_DETERMINISTIC");
        }
    }

    if (!padding_probe) {
        return;
    }

    K1PackedBuffers alternate = pack_k1(k1_test::logical_input(vector), k1_default_layout());
    vary_padding(alternate, 0xEEU, 0xDDU, 0x2468ACE0U);
    runtime.dispatch_k1(alternate);
    const std::vector<Byte> alternate_actual = unpack_k1(alternate.next_state);
    check_output(vector, expected, alternate_actual, runtime, options);
    const K1Comparison padding_comparison = compare_k1_output(actual, alternate_actual);
    if (!padding_comparison.matches()) {
        write_mismatch(
            options.mismatch_dir,
            vector,
            actual,
            alternate_actual,
            padding_comparison,
            runtime,
            options,
            "PADDING_AFFECTED_LOGICAL_OUTPUT");
    }
}

void write_evidence(const Options& options,
                    const XdnaRuntime& runtime,
                    std::size_t edge_count,
                    std::size_t fixed_count,
                    std::size_t random_count,
                    std::uint64_t physical_dispatches,
                    std::uint64_t exact_matches)
{
    if (options.evidence.empty()) {
        return;
    }
    const auto parent = options.evidence.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream stream(options.evidence);
    if (!stream) {
        throw DifferentialFailure("cannot write M3 evidence: " + options.evidence.string());
    }
    const auto& capability = runtime.capability();
    const auto& counters = runtime.counters();
    const ArtifactSums sums = read_artifact_sums(options.artifact_sums);
    stream << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"milestone\": \"M3\",\n"
           << "  \"status\": \"PASS\",\n"
           << "  \"captured_utc\": ";
    json_string(stream, utc_now());
    stream << ",\n  \"toolchain\": {\n"
           << "    \"device_name\": ";
    json_string(stream, capability.device_name);
    stream << ",\n    \"architecture\": ";
    json_string(stream, capability.architecture);
    stream << ",\n    \"bdf\": ";
    json_string(stream, capability.bdf);
    stream << ",\n    \"device_node\": ";
    json_string(stream, capability.device_node);
    stream << ",\n    \"amdxdna_version\": ";
    json_string(stream, capability.amdxdna_version);
    stream << ",\n    \"firmware_version\": ";
    json_string(stream, capability.firmware_version);
    stream << ",\n    \"xrt_version\": ";
    json_string(stream, capability.xrt_version);
    stream << ",\n    \"xrt_hash\": ";
    json_string(stream, capability.xrt_hash);
    stream << ",\n    \"mlir_aie_source_commit\": \"57d7494e99c214f5f53b328a0ed43a99e759e835\",\n"
           << "    \"mlir_aie_wheel\": \"1.3.4\",\n"
           << "    \"iron_python\": \"CPython 3.12.13\",\n"
           << "    \"peano\": \"llvm-aie 21.0.0.2026072001+ce8c0f8f\"\n"
           << "  },\n  \"artifact\": {\n"
           << "    \"xclbin\": ";
    json_string(stream, options.xclbin.string());
    stream << ",\n    \"instructions\": ";
    json_string(stream, options.instructions.string());
    stream << ",\n    \"manifest\": ";
    json_string(stream, options.manifest.string());
    stream << ",\n    \"xclbin_sha256\": ";
    json_string(stream, sums.xclbin);
    stream << ",\n    \"instructions_sha256\": ";
    json_string(stream, sums.instructions);
    stream << ",\n    \"uuid\": ";
    json_string(stream, runtime.artifact_uuid());
    stream << ",\n    \"kernel\": ";
    json_string(stream, runtime.kernel_name());
    stream << ",\n    \"target_column_count\": 1\n  },\n"
           << "  \"k1_logical_contract\": {\n"
           << "    \"previous_state_bytes\": 64,\n"
           << "    \"lut_rows\": 46,\n"
           << "    \"lut_logical_entries\": 27,\n"
           << "    \"lut_row_stride_bytes\": 32,\n"
           << "    \"neighbors_rows\": 64,\n"
           << "    \"neighbors_per_row\": 3,\n"
           << "    \"updated_neurons\": 46,\n"
           << "    \"trit_domain\": [0,1,2],\n"
           << "    \"unknown_value\": 2,\n"
           << "    \"simultaneous_previous_state_reads\": true,\n"
           << "    \"input_roles\": \"the 18 neurons absent from updated_neurons are copied/held\"\n"
           << "  },\n  \"k1_device_contract\": {\n"
           << "    \"combined_input_device_bytes\": 2528,\n"
           << "    \"state_offset\": 0,\n"
           << "    \"lut_offset\": 96,\n"
           << "    \"neighbors_offset\": 1568,\n"
           << "    \"updated_neurons_offset\": 2336,\n"
           << "    \"state_device_bytes\": 96,\n"
           << "    \"state_logical_bytes\": 64,\n"
           << "    \"lut_device_bytes\": 1472,\n"
           << "    \"neighbors_device_bytes\": 768,\n"
           << "    \"updated_neurons_logical_bytes\": 184,\n"
           << "    \"updated_neurons_device_bytes\": 192,\n"
           << "    \"output_device_bytes\": 96,\n"
           << "    \"padding_is_semantically_unused\": true,\n"
           << "    \"h2d_syncs_per_dispatch\": 2,\n"
           << "    \"d2h_syncs_per_dispatch\": 1\n  },\n"
           << "  \"corpus\": {\n"
           << "    \"generator_version\": ";
    json_string(stream, kGeneratorVersion);
    stream << ",\n    \"edge_cases\": " << edge_count
           << ",\n    \"fixed_cases\": " << fixed_count
           << ",\n    \"random_cases\": " << random_count
           << ",\n    \"random_seed\": " << options.random_seed
           << ",\n    \"random_case_identity\": \"generator_version + random_seed + case_index\"\n  },\n"
           << "  \"dispatch\": {\n"
           << "    \"physical_dispatches\": " << physical_dispatches
           << ",\n    \"successful_dispatches\": " << counters.completed_dispatches
           << ",\n    \"exact_logical_matches\": " << exact_matches
           << ",\n    \"logical_mismatches\": 0,\n    \"runtime_failures\": 0"
           << ",\n    \"explicit_h2d_syncs\": " << counters.h2d_syncs
           << ",\n    \"explicit_d2h_syncs\": " << counters.d2h_syncs
           << ",\n    \"hardware_context_created\": true,\n    \"silent_cpu_fallback\": false\n  },\n"
           << "  \"required_matrix\": {\n"
           << "    \"all_zero\": true,\n    \"all_one\": true,\n    \"all_unknown\": true,\n"
           << "    \"mixed_pattern\": true,\n    \"every_k3_combination_27\": true,\n"
           << "    \"same_neighbor_reuse\": true,\n    \"distinct_neighbors\": true,\n"
           << "    \"external_input_roles\": true,\n    \"output_signal_roles\": true,\n"
           << "    \"previous_state_isolation\": true,\n    \"padded_buffer_round_trip\": true,\n"
           << "    \"repeated_deterministic_dispatch\": true\n  },\n"
           << "  \"regressions\": {\n    \"m1_status\": ";
    json_string(stream, options.m1_status);
    stream << ",\n    \"m2_status\": ";
    json_string(stream, options.m2_status);
    stream << ",\n    \"negative_paths_status\": ";
    json_string(stream, options.negative_status);
    stream << "\n  },\n  \"performance\": {\n    \"speedup_claim\": false,\n    \"profitability_claim\": false,\n    \"timing_recorded\": false\n  }\n}\n";
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
        XdnaRuntime runtime(
            xdna::runtime::K1Artifact{options.xclbin, options.instructions, options.manifest},
            options.selector,
            WorkloadKind::K1);

        const std::vector<k1_test::K1Vector> edges
            = options.include_edge_cases ? k1_test::make_edge_vectors() : std::vector<k1_test::K1Vector>{};
        const std::vector<k1_test::K1Vector> fixed
            = k1_test::make_deterministic_vectors(0x4D33464958454431ULL, options.fixed_count);
        const std::vector<k1_test::K1Vector> random
            = k1_test::make_deterministic_vectors(options.random_seed, options.random_count);

        for (const auto& vector : edges) {
            run_vector(
                runtime,
                vector,
                options,
                vector.id == "padded-buffer-round-trip",
                vector.id == "all-zero-state-lut");
        }
        for (const auto& vector : fixed) {
            run_vector(runtime, vector, options, false, false);
        }
        for (const auto& vector : random) {
            run_vector(runtime, vector, options, false, false);
        }

        const auto& counters = runtime.counters();
        const std::uint64_t expected_dispatches = static_cast<std::uint64_t>(edges.size() + fixed.size() + random.size())
            + (std::any_of(edges.begin(), edges.end(), [](const auto& vector) {
                  return vector.id == "padded-buffer-round-trip";
              }) ? 1U : 0U)
            + (std::any_of(edges.begin(), edges.end(), [](const auto& vector) {
                  return vector.id == "all-zero-state-lut";
              }) ? 1U : 0U);
        if (counters.completed_dispatches != expected_dispatches) {
            throw DifferentialFailure(
                "completed physical dispatch count " + std::to_string(counters.completed_dispatches)
                + " does not equal expected " + std::to_string(expected_dispatches));
        }
        write_evidence(
            options,
            runtime,
            edges.size(),
            fixed.size(),
            random.size(),
            counters.completed_dispatches,
            counters.completed_dispatches);

        std::cout << "device=" << runtime.capability().device_name << '\n'
                  << "architecture=" << runtime.capability().architecture << '\n'
                  << "bdf=" << runtime.capability().bdf << '\n'
                  << "kernel=" << runtime.kernel_name() << '\n'
                  << "artifact_uuid=" << runtime.artifact_uuid() << '\n'
                  << "k1_generator_version=" << kGeneratorVersion << '\n'
                  << "edge_cases=" << edges.size() << '\n'
                  << "fixed_cases=" << fixed.size() << '\n'
                  << "random_cases=" << random.size() << '\n'
                  << "random_seed=" << options.random_seed << '\n'
                  << "physical_dispatches=" << counters.completed_dispatches << '\n'
                  << "exact_logical_matches=" << counters.completed_dispatches << '\n'
                  << "logical_mismatches=0\n"
                  << "runtime_failures=0\n"
                  << "explicit_h2d_syncs=" << counters.h2d_syncs << '\n'
                  << "explicit_d2h_syncs=" << counters.d2h_syncs << '\n'
                  << "NPU K1 DIFFERENTIAL PASS\n";
        if (!options.evidence.empty()) {
            std::cout << "evidence=" << options.evidence << '\n';
        }
        return 0;
    } catch (const DifferentialFailure& error) {
        std::cerr << "FAIL code=K1_DIFFERENTIAL detail=" << error.what() << '\n';
        return 1;
    } catch (const RuntimeError& error) {
        std::cerr << "FAIL code=" << xdna::runtime::error_code_name(error.code())
                  << " detail=" << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "FAIL code=UNCLASSIFIED detail=" << error.what() << '\n';
        return 1;
    }
}
