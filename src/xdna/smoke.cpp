#include "xdna/smoke.hpp"

#include "xdna/buffers.hpp"
#include "xdna/errors.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace xdna::runtime {
namespace {

[[nodiscard]] std::vector<std::int32_t> deterministic_input(std::uint64_t iteration)
{
    std::vector<std::int32_t> input(kSmokeElementCount);
    const auto base = static_cast<std::int32_t>((iteration % 100000U) * 17U);
    for (std::size_t index = 0U; index < input.size(); ++index) {
        input[index] = base + static_cast<std::int32_t>(index * 5U) - 31;
    }
    return input;
}

[[nodiscard]] std::string json_escape(const std::string& value)
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

void json_string(std::ostream& stream, const std::string& value)
{
    stream << '"' << json_escape(value) << '"';
}

} // namespace

SmokeSummary run_smoke(XdnaRuntime& runtime, std::uint64_t iterations)
{
    if (iterations == 0U) {
        throw RuntimeError(ErrorCode::InvalidArgument, "iterations must be greater than zero");
    }
    SmokeSummary summary;
    summary.requested_dispatches = iterations;

    for (std::uint64_t iteration = 0U; iteration < iterations; ++iteration) {
        const std::vector<std::int32_t> input = deterministic_input(iteration);
        std::vector<std::int32_t> expected(kSmokeElementCount);
        std::vector<std::int32_t> actual(kSmokeElementCount, std::numeric_limits<std::int32_t>::min());
        for (std::size_t index = 0U; index < input.size(); ++index) {
            expected[index] = smoke_cpu_transform(input[index]);
        }

        try {
            runtime.dispatch(input, actual);
        } catch (const RuntimeError&) {
            ++summary.runtime_failures;
            throw;
        }

        if (actual != expected) {
            ++summary.output_mismatches;
            throw RuntimeError(
                ErrorCode::OutputMismatch,
                "exact CPU oracle comparison failed at dispatch " + std::to_string(iteration));
        }
        ++summary.exact_matches;
    }
    return summary;
}

void write_smoke_evidence(const std::filesystem::path& path,
                          const XdnaRuntime& runtime,
                          const SmokeSummary& summary,
                          std::uint64_t iterations)
{
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    const CapabilityReport& capability = runtime.capability();
    const RuntimeCounters& counters = runtime.counters();
    std::ofstream stream(path);
    if (!stream) {
        throw RuntimeError(ErrorCode::InvalidArgument, "cannot write evidence file: " + path.string());
    }

    stream << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"milestone\": \"M2\",\n"
           << "  \"hardware\": {\n"
           << "    \"status\": ";
    json_string(stream, capability_status_name(capability.status));
    stream << ",\n    \"device_name\": ";
    json_string(stream, capability.device_name);
    stream << ",\n    \"architecture\": ";
    json_string(stream, capability.architecture);
    stream << ",\n    \"bdf\": ";
    json_string(stream, capability.bdf);
    stream << ",\n    \"device_node\": ";
    json_string(stream, capability.device_node);
    stream << ",\n    \"firmware_version\": ";
    json_string(stream, capability.firmware_version);
    stream << ",\n    \"xrt_version\": ";
    json_string(stream, capability.xrt_version);
    stream << ",\n    \"xrt_hash\": ";
    json_string(stream, capability.xrt_hash);
    stream << ",\n    \"amdxdna_version\": ";
    json_string(stream, capability.amdxdna_version);
    stream << "\n  },\n  \"artifact\": {\n    \"xclbin\": ";
    json_string(stream, runtime.artifact().xclbin.string());
    stream << ",\n    \"instructions\": ";
    json_string(stream, runtime.artifact().instructions.string());
    stream << ",\n    \"uuid\": ";
    json_string(stream, runtime.artifact_uuid());
    stream << ",\n    \"kernel\": ";
    json_string(stream, runtime.kernel_name());
    stream << "\n  },\n  \"workload\": {\n"
           << "    \"element_type\": \"int32\",\n"
           << "    \"element_count\": " << kSmokeElementCount << ",\n"
           << "    \"byte_count\": " << kSmokeBufferBytes << ",\n"
           << "    \"alignment_bytes\": " << kSmokeAlignmentBytes << ",\n"
           << "    \"transform\": \"out[i] = 3 * in[i] + 7\",\n"
           << "    \"iterations\": " << iterations << "\n"
           << "  },\n  \"dispatch\": {\n"
           << "    \"requested\": " << summary.requested_dispatches << ",\n"
           << "    \"completed\": " << summary.exact_matches << ",\n"
           << "    \"output_mismatches\": " << summary.output_mismatches << ",\n"
           << "    \"runtime_failures\": " << summary.runtime_failures << ",\n"
           << "    \"xrt_dispatches\": " << counters.dispatches << ",\n"
           << "    \"xrt_completed_dispatches\": " << counters.completed_dispatches << ",\n"
           << "    \"explicit_h2d_syncs\": " << counters.h2d_syncs << ",\n"
           << "    \"explicit_d2h_syncs\": " << counters.d2h_syncs << "\n"
           << "  },\n  \"evidence\": {\n"
           << "    \"hardware_context_created\": true,\n"
           << "    \"xrt_kernel_wait_completed\": "
           << (counters.completed_dispatches == counters.dispatches && counters.dispatches > 0U ? "true" : "false")
           << ",\n    \"input_dependent_exact_outputs\": "
           << (summary.exact_matches == summary.requested_dispatches ? "true" : "false")
           << ",\n    \"silent_cpu_fallback\": false\n"
           << "  }\n}\n";
}

} // namespace xdna::runtime
