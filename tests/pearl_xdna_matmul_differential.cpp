#include "pearl/xdna_matmul.hpp"

#include "xdna/errors.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace xdna::pearl;

struct Options {
    XdnaMatmulArtifact artifact;
    std::string selector = "0";
    std::size_t cases = 100U;
    std::filesystem::path evidence;
};

[[nodiscard]] std::uint64_t next_word(std::uint64_t& state)
{
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t value = state;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

[[nodiscard]] Int8Matrix make_matrix(std::size_t rows,
                                     std::size_t cols,
                                     std::uint64_t& state,
                                     std::size_t case_index,
                                     bool alternating)
{
    std::vector<std::int8_t> values;
    values.reserve(rows * cols);
    for (std::size_t index = 0U; index < rows * cols; ++index) {
        std::int32_t value = 0;
        if (case_index == 0U) {
            value = 0;
        } else if (case_index == 1U) {
            value = 1;
        } else if (case_index == 2U) {
            value = (index % 2U) == 0U ? -63 : 63;
        } else if (case_index == 3U) {
            value = -64;
        } else if (case_index == 4U) {
            value = 63;
        } else if (case_index == 5U) {
            value = (index % 17U) == 0U ? 63 : 0;
        } else if (alternating) {
            value = (index % 2U) == 0U ? -64 : 63;
        } else {
            value = static_cast<std::int32_t>(next_word(state) % 128U) - 64;
        }
        values.push_back(static_cast<std::int8_t>(value));
    }
    return Int8Matrix(rows, cols, std::move(values));
}

void parse_options(int argc, char** argv, Options& options)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        const auto require_value = [&](const char* name) -> std::string {
            if (index + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[++index];
        };
        if (argument == "--xclbin") {
            options.artifact.xclbin = require_value("--xclbin");
        } else if (argument == "--insts") {
            options.artifact.instructions = require_value("--insts");
        } else if (argument == "--manifest") {
            options.artifact.manifest = require_value("--manifest");
        } else if (argument == "--selector") {
            options.selector = require_value("--selector");
        } else if (argument == "--cases") {
            options.cases = static_cast<std::size_t>(std::stoull(require_value("--cases")));
            if (options.cases == 0U) {
                throw std::runtime_error("--cases must be positive");
            }
        } else if (argument == "--evidence") {
            options.evidence = require_value("--evidence");
        } else if (argument == "--help") {
            std::cout << "usage: pearl_xdna_matmul_differential --xclbin PATH --insts PATH "
                         "--manifest PATH [--selector DEVICE] [--cases N] [--evidence PATH]\n";
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + std::string(argument));
        }
    }
    if (options.artifact.xclbin.empty() || options.artifact.instructions.empty()
        || options.artifact.manifest.empty()) {
        throw std::runtime_error("--xclbin, --insts, and --manifest are required");
    }
}

void write_evidence(const Options& options,
                    const XdnaMatmulExecutor& executor,
                    std::size_t mismatches,
                    std::size_t runtime_failures,
                    std::size_t cpu_fallbacks)
{
    if (options.evidence.empty()) {
        return;
    }
    if (!options.evidence.parent_path().empty()) {
        std::filesystem::create_directories(options.evidence.parent_path());
    }
    const auto& capability = executor.capability();
    const auto& counters = executor.counters();
    std::ofstream output(options.evidence);
    if (!output) {
        throw std::runtime_error("cannot write evidence: " + options.evidence.string());
    }
    const bool pass = mismatches == 0U && runtime_failures == 0U && cpu_fallbacks == 0U
        && counters.completed_dispatches == options.cases;
    output << "{\n"
           << "  \"schema\": \"pearl-p2-xdna-gemm-evidence/v1\",\n"
           << "  \"milestone\": \"P2\",\n"
           << "  \"status\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
           << "  \"workload\": {\"rows\": 4, \"common\": 64, \"columns\": 8, "
              "\"left_dtype\": \"int8\", \"right_dtype\": \"int8\", "
              "\"output_dtype\": \"int32\", \"layout\": \"row-major\"},\n"
           << "  \"target\": {\"device\": \"" << capability.device_name
           << "\", \"architecture\": \"" << capability.architecture
           << "\", \"bdf\": \"" << capability.bdf
           << "\", \"device_node\": \"" << capability.device_node
           << "\", \"firmware\": \"" << capability.firmware_version
           << "\", \"xrt\": \"" << capability.xrt_version
           << "\", \"xrt_hash\": \"" << capability.xrt_hash
           << "\", \"amdxdna\": \"" << capability.amdxdna_version << "\"},\n"
           << "  \"artifact\": {\"xclbin\": \"" << options.artifact.xclbin.string()
           << "\", \"instructions\": \"" << options.artifact.instructions.string()
           << "\", \"uuid\": \"" << executor.artifact_uuid()
           << "\", \"kernel\": \"" << executor.kernel_name() << "\"},\n"
           << "  \"cases\": {\"requested\": " << options.cases
           << ", \"dispatches\": " << counters.dispatches
           << ", \"completed_dispatches\": " << counters.completed_dispatches
           << ", \"mismatches\": " << mismatches
           << ", \"runtime_failures\": " << runtime_failures
           << ", \"cpu_fallbacks\": " << cpu_fallbacks << "},\n"
           << "  \"transfers\": {\"h2d_syncs\": " << counters.h2d_syncs
           << ", \"h2d_bytes\": " << counters.h2d_bytes
           << ", \"d2h_syncs\": " << counters.d2h_syncs
           << ", \"d2h_bytes\": " << counters.d2h_bytes
           << ", \"dispatch_wait_ns\": " << counters.dispatch_wait_ns << "},\n"
           << "  \"hardware_context_created\": true,\n"
           << "  \"silent_cpu_fallback\": false,\n"
           << "  \"runtime_pin_note\": \"Observed amdxdna is recorded exactly; compare with runtime-pins.json historical M2 pin.\"\n"
           << "}\n";
}

} // namespace

int main(int argc, char** argv)
{
    try {
        Options options;
        parse_options(argc, argv, options);
        XdnaMatmulExecutor executor(options.artifact, options.selector);
        std::uint64_t state = 0x504541524C503200ULL;
        std::size_t mismatches = 0U;
        std::size_t runtime_failures = 0U;
        const std::size_t cpu_fallbacks = 0U;
        for (std::size_t case_index = 0U; case_index < options.cases; ++case_index) {
            const Int8Matrix left = make_matrix(
                kP2Rows, kP2Common, state, case_index, (case_index % 2U) == 0U);
            const Int8Matrix right = make_matrix(
                kP2Common, kP2Columns, state, case_index, (case_index % 2U) != 0U);
            const Int32Matrix expected = gemm_checked(left, right);
            try {
                const Int32Matrix actual = executor.dispatch(left, right);
                if (actual.values() != expected.values()) {
                    ++mismatches;
                    std::cerr << "MISMATCH case=" << case_index << '\n';
                }
            } catch (const std::exception& error) {
                ++runtime_failures;
                std::cerr << "RUNTIME_FAILURE case=" << case_index
                          << " error=" << error.what() << '\n';
                break;
            }
        }
        write_evidence(options, executor, mismatches, runtime_failures, cpu_fallbacks);
        const auto& counters = executor.counters();
        std::cout << "device=" << executor.capability().device_name << '\n'
                  << "architecture=" << executor.capability().architecture << '\n'
                  << "bdf=" << executor.capability().bdf << '\n'
                  << "kernel=" << executor.kernel_name() << '\n'
                  << "uuid=" << executor.artifact_uuid() << '\n'
                  << "requested_cases=" << options.cases << '\n'
                  << "dispatches=" << counters.dispatches << '\n'
                  << "completed_dispatches=" << counters.completed_dispatches << '\n'
                  << "mismatches=" << mismatches << '\n'
                  << "runtime_failures=" << runtime_failures << '\n'
                  << "cpu_fallbacks=" << cpu_fallbacks << '\n'
                  << "h2d_syncs=" << counters.h2d_syncs << '\n'
                  << "d2h_syncs=" << counters.d2h_syncs << '\n';
        return mismatches == 0U && runtime_failures == 0U
                && counters.completed_dispatches == options.cases
            ? 0
            : 1;
    } catch (const xdna::runtime::RuntimeError& error) {
        std::cerr << xdna::runtime::error_code_name(error.code())
                  << ": " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "INVALID_ARGUMENT: " << error.what() << '\n';
        return 2;
    }
}
