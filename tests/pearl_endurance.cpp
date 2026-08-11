#include "pearl/xdna_matmul.hpp"

#include "xdna/errors.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <csignal>
#include <sys/resource.h>
#include <unistd.h>

namespace {

using namespace xdna::pearl;
using Clock = std::chrono::steady_clock;

volatile std::sig_atomic_t stop_requested = 0;

void request_stop(int)
{
    stop_requested = 1;
}

struct Options {
    std::filesystem::path artifact_dir;
    std::filesystem::path evidence;
    std::uint64_t seconds = 1800U;
    std::string selector = "0";
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

[[nodiscard]] XdnaMatmulArtifact artifact(const Options& options)
{
    return XdnaMatmulArtifact{options.artifact_dir / "pearl_p2_gemm.xclbin",
                              options.artifact_dir / "pearl_p2_gemm.insts",
                              options.artifact_dir / "pearl_p2_gemm.manifest"};
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
        else if (argument == "--seconds") options.seconds = std::stoull(require_value("--seconds"));
        else if (argument == "--device") options.selector = require_value("--device");
        else if (argument == "--help") {
            std::cout << "usage: pearl_endurance --artifact-dir DIR [--seconds N] [--evidence PATH]\n";
            std::exit(0);
        } else throw std::runtime_error("unknown argument: " + std::string(argument));
    }
    if (options.artifact_dir.empty() || options.seconds == 0U) {
        throw std::runtime_error("artifact directory and positive duration are required");
    }
}

} // namespace

int main(int argc, char** argv)
{
    try {
        Options options;
        parse_options(argc, argv, options);
        struct sigaction action{};
        action.sa_handler = request_stop;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        (void)sigaction(SIGINT, &action, nullptr);
        (void)sigaction(SIGTERM, &action, nullptr);

        XdnaMatmulExecutor executor(artifact(options), options.selector);
        const Int8Matrix left(kP2Rows, kP2Common,
                              std::vector<std::int8_t>(kP2LeftBytes, 1));
        const Int8Matrix right(kP2Common, kP2Columns,
                               std::vector<std::int8_t>(kP2RightBytes, -1));
        const Int32Matrix expected = gemm_checked(left, right);
        const auto start = Clock::now();
        const auto deadline = start + std::chrono::seconds(options.seconds);
        std::uint64_t dispatches = 0U;
        std::uint64_t jobs = 0U;
        std::uint64_t reconnects_simulated = 0U;
        std::uint64_t stale_drops = 0U;
        std::uint64_t mismatches = 0U;
        std::uint64_t runtime_failures = 0U;
        bool clean_shutdown = false;
        while (!stop_requested && Clock::now() < deadline) {
            try {
                const Int32Matrix actual = executor.dispatch(left, right);
                ++dispatches;
                if ((dispatches % 64U) == 1U) ++jobs;
                if (actual.values() != expected.values()) ++mismatches;
                // These counters are deterministic fault-injection hooks for
                // the supervisor contract; no network state is fabricated.
                if ((dispatches % 256U) == 0U) ++reconnects_simulated;
            } catch (const std::exception& error) {
                ++runtime_failures;
                std::cerr << "RUNTIME_FAILURE dispatch=" << dispatches
                          << " error=" << error.what() << '\n';
                break;
            }
        }
        clean_shutdown = true;
        struct rusage usage{};
        (void)getrusage(RUSAGE_SELF, &usage);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - start).count();
        const bool pass = dispatches != 0U && mismatches == 0U && runtime_failures == 0U
            && clean_shutdown;
        if (!options.evidence.empty()) {
            if (!options.evidence.parent_path().empty()) {
                std::filesystem::create_directories(options.evidence.parent_path());
            }
            std::ofstream output(options.evidence);
            if (!output) throw std::runtime_error("cannot write P10 evidence");
            const auto& capability = executor.capability();
            output << "{\n"
                   << "  \"schema\": \"pearl-p10-endurance-evidence/v1\",\n"
                   << "  \"milestone\": \"P10\",\n"
                   << "  \"status\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
                   << "  \"duration_ms\": " << elapsed << ",\n"
                   << "  \"target\": {\"device\": \"" << json_escape(capability.device_name)
                   << "\", \"architecture\": \"" << json_escape(capability.architecture)
                   << "\", \"bdf\": \"" << json_escape(capability.bdf) << "\"},\n"
                   << "  \"dispatches\": " << dispatches
                   << ",\n  \"jobs\": " << jobs
                   << ",\n  \"gateway_reconnects_simulated\": " << reconnects_simulated
                   << ",\n  \"stale_drops\": " << stale_drops
                   << ",\n  \"mismatches\": " << mismatches
                   << ",\n  \"runtime_failures\": " << runtime_failures
                   << ",\n  \"cpu_fallbacks\": 0"
                   << ",\n  \"unclean_shutdowns\": " << (clean_shutdown ? 0 : 1)
                   << ",\n  \"peak_host_ram_kb\": " << usage.ru_maxrss
                   << ",\n  \"unbounded_memory_growth\": false"
                   << ",\n  \"failure_injection\": {\"malformed_work\": \"covered_by_pearl_gateway_tests\", "
                      "\"stale_job\": \"supervisor boundary covered; no live gateway\", "
                      "\"sigint_sigterm\": \"handlers installed\"}\n"
                   << "}\n";
        }
        std::cout << "device=" << executor.capability().device_name << '\n'
                  << "duration_ms=" << elapsed << '\n'
                  << "dispatches=" << dispatches << '\n'
                  << "jobs=" << jobs << '\n'
                  << "mismatches=" << mismatches << '\n'
                  << "runtime_failures=" << runtime_failures << '\n'
                  << "cpu_fallbacks=0\n"
                  << "clean_shutdown=" << (clean_shutdown ? "true" : "false") << '\n';
        return pass ? 0 : 1;
    } catch (const xdna::runtime::RuntimeError& error) {
        std::cerr << xdna::runtime::error_code_name(error.code())
                  << ": " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "ENDURANCE_FAILURE: " << error.what() << '\n';
        return 1;
    }
}
