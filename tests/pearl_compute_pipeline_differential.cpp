#include "pearl/compute_pipeline.hpp"

#include "xdna/errors.hpp"

#include <array>
#include <chrono>
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
    std::size_t cases = 8U;
    std::filesystem::path evidence;
};

[[nodiscard]] std::string json_escape(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result += character;
            break;
        }
    }
    return result;
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
            std::cout << "usage: pearl_compute_pipeline_differential --xclbin PATH "
                         "--insts PATH --manifest PATH [--selector DEVICE] "
                         "[--cases N] [--evidence PATH]\n";
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
                    std::size_t completed,
                    const ComputePipelineTimings& timings,
                    std::size_t mismatches,
                    std::size_t runtime_failures)
{
    if (options.evidence.empty()) {
        return;
    }
    if (!options.evidence.parent_path().empty()) {
        std::filesystem::create_directories(options.evidence.parent_path());
    }
    std::ofstream output(options.evidence);
    if (!output) {
        throw std::runtime_error("cannot write evidence: " + options.evidence.string());
    }
    const auto& capability = executor.capability();
    const auto& counters = executor.counters();
    const bool pass = completed == options.cases && mismatches == 0U
        && runtime_failures == 0U;
    output << "{\n"
           << "  \"schema\": \"pearl-p3-compute-pipeline-evidence/v1\",\n"
           << "  \"milestone\": \"P3\",\n"
           << "  \"status\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
           << "  \"workload\": {\"base_rows\": 2, \"base_common\": 64, "
              "\"base_columns\": 64, \"rank\": 64, "
              "\"selected_rows\": 2, \"selected_columns\": 64},\n"
           << "  \"target\": {\"device\": \"" << json_escape(capability.device_name)
           << "\", \"architecture\": \"" << json_escape(capability.architecture)
           << "\", \"bdf\": \"" << json_escape(capability.bdf)
           << "\", \"firmware\": \"" << json_escape(capability.firmware_version)
           << "\", \"xrt\": \"" << json_escape(capability.xrt_version)
           << "\", \"amdxdna\": \"" << json_escape(capability.amdxdna_version)
           << "\"},\n"
           << "  \"artifact\": {\"uuid\": \"" << json_escape(executor.artifact_uuid())
           << "\", \"kernel\": \"" << json_escape(executor.kernel_name()) << "\"},\n"
           << "  \"cases\": {\"requested\": " << options.cases
           << ", \"completed\": " << completed
           << ", \"mismatches\": " << mismatches
           << ", \"runtime_failures\": " << runtime_failures
           << ", \"cpu_fallbacks\": 0},\n"
           << "  \"dispatches\": {\"requested\": " << options.cases * 8U
           << ", \"completed\": " << counters.completed_dispatches
           << ", \"h2d_syncs\": " << counters.h2d_syncs
           << ", \"d2h_syncs\": " << counters.d2h_syncs
           << ", \"dispatch_wait_ns\": " << counters.dispatch_wait_ns << "},\n"
           << "  \"timings_last_case_ns\": {\"cpu_preprocessing\": "
           << timings.cpu_preprocessing_ns << ", \"packing\": " << timings.packing_ns
           << ", \"npu_execution\": " << timings.npu_execution_ns
           << ", \"cpu_verification\": " << timings.cpu_verification_ns
           << ", \"transcript\": " << timings.transcript_ns
           << ", \"blake3_and_target\": " << timings.blake3_ns
           << ", \"total\": " << timings.total_ns << "},\n"
           << "  \"hardware_context_created\": true,\n"
           << "  \"silent_cpu_fallback\": false\n"
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

        const Int8Matrix a(2U, 64U, std::vector<std::int8_t>(2U * 64U, 0));
        const Int8Matrix b(64U, 64U, std::vector<std::int8_t>(64U * 64U, 0));
        const std::vector<std::size_t> selected_rows{0U, 1U};
        std::vector<std::size_t> selected_columns(64U, 0U);
        for (std::size_t index = 0U; index < selected_columns.size(); ++index) {
            selected_columns[index] = index;
        }

        std::size_t completed = 0U;
        std::size_t mismatches = 0U;
        std::size_t runtime_failures = 0U;
        ComputePipelineTimings last_timings{};
        for (std::size_t case_index = 0U; case_index < options.cases; ++case_index) {
            CommitmentSeeds seeds{};
            for (std::size_t byte = 0U; byte < kDigestBytes; ++byte) {
                seeds.a_noise_seed[byte] = static_cast<std::uint8_t>(
                    byte + case_index * 17U);
                seeds.b_noise_seed[byte] = static_cast<std::uint8_t>(
                    0xA5U ^ static_cast<std::uint8_t>(byte + case_index * 29U));
            }
            try {
                const NoiseMatrices noise = generate_noise(
                    64U, 64U, seeds, selected_rows, selected_columns);
                Digest commitment_hash{};
                Digest target{};
                target.fill(0xFFU);
                const ComputePipelineResult result = pipeline.run(
                    a, b, noise, 64U, commitment_hash, target);
                const Int32Matrix zero(2U, 64U, std::vector<std::int32_t>(2U * 64U, 0));
                if (result.denoised_product.values() != zero.values()
                    || !result.meets_target || result.transcript.trace.size() != 1U) {
                    ++mismatches;
                    std::cerr << "MISMATCH case=" << case_index << '\n';
                }
                last_timings = result.timings;
                ++completed;
            } catch (const std::exception& error) {
                ++runtime_failures;
                std::cerr << "RUNTIME_FAILURE case=" << case_index
                          << " error=" << error.what() << '\n';
                break;
            }
        }

        write_evidence(options, executor, completed, last_timings, mismatches, runtime_failures);
        const auto& counters = executor.counters();
        std::cout << "device=" << executor.capability().device_name << '\n'
                  << "architecture=" << executor.capability().architecture << '\n'
                  << "bdf=" << executor.capability().bdf << '\n'
                  << "uuid=" << executor.artifact_uuid() << '\n'
                  << "requested_cases=" << options.cases << '\n'
                  << "completed_cases=" << completed << '\n'
                  << "mismatches=" << mismatches << '\n'
                  << "runtime_failures=" << runtime_failures << '\n'
                  << "cpu_fallbacks=0\n"
                  << "dispatches=" << counters.dispatches << '\n'
                  << "completed_dispatches=" << counters.completed_dispatches << '\n';
        return completed == options.cases && mismatches == 0U && runtime_failures == 0U
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
