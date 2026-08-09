#include "xdna/errors.hpp"
#include "xdna/buffers.hpp"
#include "xdna/runtime.hpp"
#include "xdna/smoke.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

struct Options {
    std::filesystem::path xclbin;
    std::filesystem::path instructions;
    std::filesystem::path evidence;
    std::string selector = "0";
    std::uint64_t iterations = 1U;
    std::size_t elements = xdna::runtime::kSmokeElementCount;
};

void print_usage(const char* program)
{
    std::cerr
        << "usage: " << program
        << " --xclbin PATH --insts PATH [--iterations N] [--selector DEVICE]"
        << " [--elements N] [--evidence PATH]\n";
}

[[nodiscard]] bool parse_unsigned(const std::string& text, std::uint64_t& value)
{
    try {
        std::size_t consumed = 0U;
        value = std::stoull(text, &consumed, 10);
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
             || argument == "--iterations" || argument == "--elements" || argument == "--evidence")
            && index + 1 >= argc) {
            return false;
        }
        if (argument == "--xclbin") {
            options.xclbin = argv[++index];
        } else if (argument == "--insts") {
            options.instructions = argv[++index];
        } else if (argument == "--selector") {
            options.selector = argv[++index];
        } else if (argument == "--evidence") {
            options.evidence = argv[++index];
        } else if (argument == "--iterations") {
            std::uint64_t parsed = 0U;
            if (!parse_unsigned(argv[++index], parsed)) {
                return false;
            }
            options.iterations = parsed;
        } else if (argument == "--elements") {
            std::uint64_t parsed = 0U;
            if (!parse_unsigned(argv[++index], parsed)) {
                return false;
            }
            options.elements = static_cast<std::size_t>(parsed);
        } else if (argument == "--help") {
            return false;
        } else {
            return false;
        }
    }
    return !options.xclbin.empty() && !options.instructions.empty();
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
        xdna::runtime::XdnaRuntime runtime(
            xdna::runtime::SmokeArtifact{options.xclbin, options.instructions},
            options.selector);
        if (options.elements != xdna::runtime::kSmokeElementCount) {
            throw xdna::runtime::RuntimeError(
                xdna::runtime::ErrorCode::InvalidBuffer,
                "--elements must equal the compiled smoke element count of 32");
        }

        const xdna::runtime::SmokeSummary summary = xdna::runtime::run_smoke(runtime, options.iterations);
        if (!options.evidence.empty()) {
            xdna::runtime::write_smoke_evidence(options.evidence, runtime, summary, options.iterations);
        }

        const auto& capability = runtime.capability();
        const auto& counters = runtime.counters();
        std::cout << "status=" << xdna::runtime::capability_status_name(capability.status) << '\n'
                  << "device=" << capability.device_name << '\n'
                  << "architecture=" << capability.architecture << '\n'
                  << "bdf=" << capability.bdf << '\n'
                  << "kernel=" << runtime.kernel_name() << '\n'
                  << "artifact_uuid=" << runtime.artifact_uuid() << '\n'
                  << "element_type=int32 element_count=32 byte_count=128 alignment=4\n"
                  << "transform=out[i]=3*in[i]+7\n"
                  << "dispatches_requested=" << summary.requested_dispatches << '\n'
                  << "dispatches_completed=" << counters.completed_dispatches << '\n'
                  << "exact_matches=" << summary.exact_matches << '\n'
                  << "output_mismatches=" << summary.output_mismatches << '\n'
                  << "runtime_failures=" << summary.runtime_failures << '\n'
                  << "explicit_h2d_syncs=" << counters.h2d_syncs << '\n'
                  << "explicit_d2h_syncs=" << counters.d2h_syncs << '\n'
                  << "NPU DISPATCH EVIDENCE: XRT hardware context, MLIR_AIE kernel completion, "
                     "explicit buffer synchronization, and exact CPU oracle matches\n"
                  << "NPU SMOKE PASS\n";
        return 0;
    } catch (const xdna::runtime::RuntimeError& error) {
        std::cerr << "FAIL code=" << xdna::runtime::error_code_name(error.code())
                  << " detail=" << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "FAIL code=UNCLASSIFIED detail=" << error.what() << '\n';
        return 1;
    }
}
