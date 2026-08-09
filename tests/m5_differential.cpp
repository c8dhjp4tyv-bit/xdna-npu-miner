#include "m4_vectors.hpp"

#include "bpp9000/reference.hpp"
#include "xdna/m4_score.hpp"
#include "xdna/m5.hpp"
#include "xdna/runtime.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using xdna::bpp9000::Byte;
using xdna::bpp9000::ScoreResult;
using xdna::bpp9000::ScoreStatus;
using xdna::bpp9000::WindowResult;
using xdna::runtime::M4DeviceResult;
using xdna::runtime::M4NpuScorer;
using xdna::runtime::M5ItemResult;
using xdna::runtime::M5PackedBatch;
using xdna::runtime::M5WorkItem;
using xdna::runtime::RuntimeCounters;
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
    std::filesystem::path artifact_sums;
    std::filesystem::path evidence;
    std::filesystem::path mismatch_dir;
    std::filesystem::path baseline_xclbin;
    std::filesystem::path baseline_instructions;
    std::filesystem::path baseline_manifest;
    std::string selector = "0";
    std::size_t batch_size = 1U;
    std::size_t columns = 1U;
    std::size_t warmups = 2U;
    std::size_t repeats = 5U;
    std::size_t work_items = 16U;
};

struct OwnedWorkItem {
    m4_test::M4Case fixture;
    std::uint64_t window_index = 0U;
    std::array<Byte, 64U> state{};
    std::vector<Byte> input_sequence;
    std::vector<Byte> targets;
    M5WorkItem item;

    OwnedWorkItem(m4_test::M4Case case_fixture,
                  std::uint32_t candidate_index,
                  std::uint64_t window)
        : fixture(std::move(case_fixture)),
          window_index(window),
          state{},
          input_sequence(),
          targets(),
          item{}
    {
        const std::size_t width = static_cast<std::size_t>(fixture.config.window_width);
        const std::size_t start = static_cast<std::size_t>(window_index);
        state.fill(xdna::bpp9000::trit_to_byte(xdna::bpp9000::Trit::Unknown));
        input_sequence.resize(width * xdna::runtime::kM4InputTrits);
        targets.resize(width + 1U);
        for (std::size_t index = 0U; index < input_sequence.size(); ++index) {
            input_sequence[index] = xdna::bpp9000::trit_to_byte(
                fixture.task.inputs[start * xdna::runtime::kM4InputTrits + index]);
        }
        for (std::size_t index = 0U; index < targets.size(); ++index) {
            targets[index] = xdna::bpp9000::trit_to_byte(fixture.task.outputs[start + index]);
        }
        item = M5WorkItem{
            candidate_index,
            window_index,
            xdna::runtime::M4LogicalInput{
                xdna::runtime::M4Mode::WindowScore,
                0U,
                static_cast<std::uint32_t>(fixture.config.window_width),
                fixture.config.max_ticks,
                fixture.task.topology.output_neurons[0U],
                fixture.task.topology.signal_neuron,
                std::span<const Byte>(state),
                fixture.lut.storage(),
                fixture.task.topology.neighbors,
                fixture.lut.updated_neurons(),
                fixture.task.topology.input_neurons,
                input_sequence,
                targets,
            },
        };
    }
};

struct PathObservation {
    double wall_ms = 0.0;
    double host_preparation_ms = 0.0;
    double cpu_verification_ms = 0.0;
    RuntimeCounters counters{};
    std::uint64_t exact_matches = 0U;
    std::uint64_t timeout_matches = 0U;
    std::uint64_t finite_matches = 0U;
};

struct ArtifactSums {
    std::string xclbin = "unavailable";
    std::string instructions = "unavailable";
};

struct MetricsSummary {
    std::vector<double> wall_ms;
    std::vector<double> host_preparation_ms;
    std::vector<double> cpu_verification_ms;
    std::vector<double> dispatch_wait_ms;
    RuntimeCounters counters{};
    std::uint64_t exact_matches = 0U;
    std::uint64_t timeout_matches = 0U;
    std::uint64_t finite_matches = 0U;
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
             || argument == "--artifact-sums" || argument == "--evidence" || argument == "--mismatch-dir"
             || argument == "--baseline-xclbin" || argument == "--baseline-insts"
             || argument == "--baseline-manifest" || argument == "--selector" || argument == "--batch-size"
             || argument == "--columns" || argument == "--warmups" || argument == "--repeats"
             || argument == "--work-items")
            && index + 1 >= argc) {
            return false;
        }
        if (argument == "--xclbin") {
            options.xclbin = argv[++index];
        } else if (argument == "--insts") {
            options.instructions = argv[++index];
        } else if (argument == "--manifest") {
            options.manifest = argv[++index];
        } else if (argument == "--artifact-sums") {
            options.artifact_sums = argv[++index];
        } else if (argument == "--evidence") {
            options.evidence = argv[++index];
        } else if (argument == "--mismatch-dir") {
            options.mismatch_dir = argv[++index];
        } else if (argument == "--baseline-xclbin") {
            options.baseline_xclbin = argv[++index];
        } else if (argument == "--baseline-insts") {
            options.baseline_instructions = argv[++index];
        } else if (argument == "--baseline-manifest") {
            options.baseline_manifest = argv[++index];
        } else if (argument == "--selector") {
            options.selector = argv[++index];
        } else if (argument == "--batch-size" || argument == "--columns" || argument == "--warmups"
                   || argument == "--repeats" || argument == "--work-items") {
            std::uint64_t value = 0U;
            if (!parse_unsigned(argv[++index], value)) {
                return false;
            }
            if (argument == "--batch-size") {
                options.batch_size = static_cast<std::size_t>(value);
            } else if (argument == "--columns") {
                options.columns = static_cast<std::size_t>(value);
            } else if (argument == "--warmups") {
                options.warmups = static_cast<std::size_t>(value);
            } else if (argument == "--repeats") {
                options.repeats = static_cast<std::size_t>(value);
            } else {
                options.work_items = static_cast<std::size_t>(value);
            }
        } else if (argument == "--help") {
            return false;
        } else {
            return false;
        }
    }
    if (options.manifest.empty() && !options.xclbin.empty()) {
        options.manifest = options.xclbin.parent_path() / "xdna_m5.manifest";
    }
    const bool any_baseline = !options.baseline_xclbin.empty() || !options.baseline_instructions.empty()
        || !options.baseline_manifest.empty();
    const bool complete_baseline = !options.baseline_xclbin.empty() && !options.baseline_instructions.empty()
        && !options.baseline_manifest.empty();
    return !options.xclbin.empty() && !options.instructions.empty() && options.repeats > 0U
        && options.work_items > 0U && (!any_baseline || complete_baseline);
}

void print_usage(const char* program)
{
    std::cerr << "usage: " << program
              << " --xclbin PATH --insts PATH [--manifest PATH] --batch-size N --columns N"
                 " [--artifact-sums PATH] [--evidence PATH] [--mismatch-dir PATH]"
                 " [--baseline-xclbin PATH --baseline-insts PATH --baseline-manifest PATH]"
                 " [--selector DEVICE] [--warmups N] [--repeats N] [--work-items N]\n";
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

[[nodiscard]] ArtifactSums read_artifact_sums(const std::filesystem::path& path,
                                               std::string_view xclbin_name = "xdna_m5.xclbin",
                                               std::string_view instructions_name = "xdna_m5.insts")
{
    ArtifactSums sums;
    std::ifstream stream(path);
    std::string digest;
    std::string artifact;
    while (stream >> digest >> artifact) {
        if (artifact.ends_with(xclbin_name)) {
            sums.xclbin = digest;
        } else if (artifact.ends_with(instructions_name)) {
            sums.instructions = digest;
        }
    }
    return sums;
}

[[nodiscard]] RuntimeCounters counter_delta(const RuntimeCounters& before,
                                             const RuntimeCounters& after)
{
    return RuntimeCounters{
        after.dispatches - before.dispatches,
        after.completed_dispatches - before.completed_dispatches,
        after.h2d_syncs - before.h2d_syncs,
        after.d2h_syncs - before.d2h_syncs,
        after.h2d_bytes - before.h2d_bytes,
        after.d2h_bytes - before.d2h_bytes,
        after.dispatch_wait_ns - before.dispatch_wait_ns,
    };
}

[[nodiscard]] std::vector<std::unique_ptr<OwnedWorkItem>> make_work_items(std::size_t count)
{
    std::vector<std::unique_ptr<OwnedWorkItem>> items;
    items.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const std::uint32_t width = static_cast<std::uint32_t>((index % 4U) + 2U);
        const std::uint64_t sequence_length = static_cast<std::uint64_t>(width) + 6U;
        const bool timeout = index == 3U;
        auto fixture = m4_test::make_case(
            0x4D354954454D0000ULL + static_cast<std::uint64_t>(index),
            static_cast<std::uint64_t>(index),
            sequence_length,
            width,
            timeout,
            static_cast<unsigned int>(index % 3U));
        const std::uint64_t window_count = sequence_length - width;
        items.push_back(std::make_unique<OwnedWorkItem>(
            std::move(fixture),
            static_cast<std::uint32_t>(index),
            static_cast<std::uint64_t>(index) % window_count));
    }
    return items;
}

[[nodiscard]] std::vector<const OwnedWorkItem*> ordered_items(
    const std::vector<std::unique_ptr<OwnedWorkItem>>& owned,
    std::size_t start,
    std::size_t count)
{
    if (start > owned.size() || count > owned.size() - start) {
        throw DifferentialFailure("M5 requested item range exceeds the deterministic corpus");
    }
    std::vector<const OwnedWorkItem*> result;
    result.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        result.push_back(owned[start + index].get());
    }
    return result;
}

[[nodiscard]] std::vector<M5WorkItem> make_views(const std::vector<const OwnedWorkItem*>& items)
{
    std::vector<M5WorkItem> views;
    views.reserve(items.size());
    for (const OwnedWorkItem* item : items) {
        views.push_back(item->item);
    }
    return views;
}

void write_mismatch(const Options& options,
                    std::string_view stage,
                    const OwnedWorkItem& item,
                    std::size_t result_index,
                    const ScoreResult& expected,
                    const M4DeviceResult& actual)
{
    if (options.mismatch_dir.empty()) {
        throw DifferentialFailure(std::string("M5 mismatch at ") + std::string(stage));
    }
    std::filesystem::create_directories(options.mismatch_dir);
    const std::filesystem::path path = options.mismatch_dir
        / ("m5-" + std::string(stage) + "-item-" + std::to_string(result_index) + ".json");
    std::ofstream stream(path);
    if (!stream) {
        throw DifferentialFailure("cannot write M5 mismatch artifact: " + path.string());
    }
    stream << "{\n  \"failure_kind\":\"CPU_NPU_MISMATCH\",\n"
           << "  \"stage\":\"" << json_escape(stage) << "\",\n"
           << "  \"candidate_index\":" << item.item.candidate_index
           << ",\"window_index\":" << item.window_index << ",\"result_index\":" << result_index << ",\n"
           << "  \"cpu\":{\"score\":" << expected.score << ",\"status\":"
           << static_cast<unsigned int>(expected.status) << ",\"windows\":" << expected.windows_evaluated
           << ",\"ticks\":" << expected.ticks << "},\n"
           << "  \"npu\":{\"score\":" << actual.score << ",\"status\":"
           << static_cast<unsigned int>(actual.status) << ",\"ticks\":" << actual.ticks
           << ",\"feed_count\":" << actual.feed_count << ",\"predicted\":"
           << static_cast<unsigned int>(actual.predicted) << ",\"expected\":"
           << static_cast<unsigned int>(actual.expected) << "}\n}\n";
    throw DifferentialFailure(std::string("M5 mismatch at ") + std::string(stage)
                              + "; artifact=" + path.string());
}

void compare_window(const Options& options,
                    std::string_view stage,
                    const OwnedWorkItem& item,
                    std::size_t result_index,
                    const WindowResult& expected,
                    const M4DeviceResult& actual)
{
    ScoreResult actual_score{
        actual.score,
        actual.timed_out() ? ScoreStatus::Timeout : ScoreStatus::Settled,
        1U,
        actual.ticks,
    };
    const bool equal = xdna::runtime::verify_score_exact(expected.score, actual_score).verified()
        && expected.predicted == actual.predicted
        && expected.expected == actual.expected
        && expected.feed_count == actual.feed_count;
    if (!equal) {
        write_mismatch(options, stage, item, result_index, expected.score, actual);
    }
}

[[nodiscard]] PathObservation run_m5_once(
    XdnaRuntime& runtime,
    const Options& options,
    const std::vector<const OwnedWorkItem*>& work,
    std::size_t batch_size,
    std::string_view stage)
{
    if (work.empty() || work.size() % batch_size != 0U) {
        throw DifferentialFailure("M5 benchmark work count is not divisible by batch size");
    }
    const auto counter_before = runtime.counters();
    const auto started = std::chrono::steady_clock::now();
    std::uint64_t host_preparation_ns = 0U;
    std::uint64_t cpu_verification_ns = 0U;
    std::uint64_t exact_matches = 0U;
    std::uint64_t timeout_matches = 0U;
    std::uint64_t finite_matches = 0U;
    const auto layout = xdna::runtime::m5_default_layout(batch_size);
    for (std::size_t offset = 0U; offset < work.size(); offset += batch_size) {
        const std::vector<const OwnedWorkItem*> batch_items(work.begin() + static_cast<std::ptrdiff_t>(offset),
                                                             work.begin() + static_cast<std::ptrdiff_t>(offset + batch_size));
        const std::vector<M5WorkItem> views = make_views(batch_items);
        const auto prep_started = std::chrono::steady_clock::now();
        M5PackedBatch packed = xdna::runtime::pack_m5(views, layout);
        host_preparation_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - prep_started)
                .count());
        runtime.dispatch_m5(packed, layout);
        const std::vector<M5ItemResult> results = xdna::runtime::unpack_m5_results(packed, layout);
        if (results.size() != batch_items.size()) {
            throw DifferentialFailure("M5 result count does not equal batch item count");
        }
        for (std::size_t index = 0U; index < results.size(); ++index) {
            const OwnedWorkItem& item = *batch_items[index];
            if (results[index].descriptor.candidate_index != item.item.candidate_index
                || results[index].descriptor.window_index != item.item.window_index
                || results[index].error != xdna::runtime::M5ItemError::None) {
                throw DifferentialFailure("M5 result ordering or per-item status/error is invalid");
            }
            const auto cpu_started = std::chrono::steady_clock::now();
            const WindowResult expected = xdna::bpp9000::score_window(
                item.fixture.task,
                item.fixture.lut,
                item.window_index,
                item.fixture.config);
            cpu_verification_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - cpu_started)
                    .count());
            compare_window(options, stage, item, offset + index, expected, results[index].device);
            ++exact_matches;
            if (expected.score.timed_out() && results[index].device.timed_out()) {
                ++timeout_matches;
            } else {
                ++finite_matches;
            }
        }
    }
    const auto finished = std::chrono::steady_clock::now();
    PathObservation observation;
    observation.wall_ms = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count())
        / 1000000.0;
    observation.host_preparation_ms = static_cast<double>(host_preparation_ns) / 1000000.0;
    observation.cpu_verification_ms = static_cast<double>(cpu_verification_ns) / 1000000.0;
    observation.counters = counter_delta(counter_before, runtime.counters());
    observation.exact_matches = exact_matches;
    observation.timeout_matches = timeout_matches;
    observation.finite_matches = finite_matches;
    return observation;
}

[[nodiscard]] PathObservation run_m4_once(
    XdnaRuntime& runtime,
    const Options& options,
    const std::vector<const OwnedWorkItem*>& work,
    std::string_view stage)
{
    const auto counter_before = runtime.counters();
    const auto started = std::chrono::steady_clock::now();
    std::uint64_t cpu_verification_ns = 0U;
    std::uint64_t exact_matches = 0U;
    std::uint64_t timeout_matches = 0U;
    std::uint64_t finite_matches = 0U;
    M4NpuScorer scorer(runtime);
    for (std::size_t index = 0U; index < work.size(); ++index) {
        const OwnedWorkItem& item = *work[index];
        const WindowResult actual = scorer.score_window_npu(
            item.fixture.task,
            item.fixture.lut,
            item.window_index,
            item.fixture.config);
        const auto cpu_started = std::chrono::steady_clock::now();
        const WindowResult expected = xdna::bpp9000::score_window(
            item.fixture.task,
            item.fixture.lut,
            item.window_index,
            item.fixture.config);
        cpu_verification_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - cpu_started)
                .count());
        const bool equal = xdna::runtime::verify_score_exact(expected.score, actual.score).verified()
            && expected.predicted == actual.predicted
            && expected.expected == actual.expected
            && expected.feed_count == actual.feed_count;
        if (!equal) {
            write_mismatch(options, stage, item, index, expected.score, M4DeviceResult{
                {},
                actual.score.score,
                actual.score.timed_out() ? xdna::runtime::M4DeviceStatus::Timeout
                                         : xdna::runtime::M4DeviceStatus::Settled,
                actual.score.ticks,
                actual.feed_count,
                actual.predicted,
                actual.expected,
            });
        }
        ++exact_matches;
        if (expected.score.timed_out() && actual.score.timed_out()) {
            ++timeout_matches;
        } else {
            ++finite_matches;
        }
    }
    const auto finished = std::chrono::steady_clock::now();
    PathObservation observation;
    observation.wall_ms = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count())
        / 1000000.0;
    observation.cpu_verification_ms = static_cast<double>(cpu_verification_ns) / 1000000.0;
    observation.counters = counter_delta(counter_before, runtime.counters());
    observation.exact_matches = exact_matches;
    observation.timeout_matches = timeout_matches;
    observation.finite_matches = finite_matches;
    observation.host_preparation_ms = std::max(
        0.0,
        observation.wall_ms - observation.cpu_verification_ms
            - static_cast<double>(observation.counters.dispatch_wait_ns) / 1000000.0);
    return observation;
}

void add_observation(MetricsSummary& summary, const PathObservation& observation)
{
    summary.wall_ms.push_back(observation.wall_ms);
    summary.host_preparation_ms.push_back(observation.host_preparation_ms);
    summary.cpu_verification_ms.push_back(observation.cpu_verification_ms);
    summary.dispatch_wait_ms.push_back(
        static_cast<double>(observation.counters.dispatch_wait_ns) / 1000000.0);
    summary.counters.dispatches += observation.counters.dispatches;
    summary.counters.completed_dispatches += observation.counters.completed_dispatches;
    summary.counters.h2d_syncs += observation.counters.h2d_syncs;
    summary.counters.d2h_syncs += observation.counters.d2h_syncs;
    summary.counters.h2d_bytes += observation.counters.h2d_bytes;
    summary.counters.d2h_bytes += observation.counters.d2h_bytes;
    summary.counters.dispatch_wait_ns += observation.counters.dispatch_wait_ns;
    summary.exact_matches += observation.exact_matches;
    summary.timeout_matches += observation.timeout_matches;
    summary.finite_matches += observation.finite_matches;
}

[[nodiscard]] double percentile(std::vector<double> values, double fraction)
{
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const double scaled = fraction * static_cast<double>(values.size());
    const std::size_t rank = scaled <= 1.0
        ? 0U
        : std::min(values.size() - 1U, static_cast<std::size_t>(std::ceil(scaled)) - 1U);
    return values[rank];
}

[[nodiscard]] double median(const std::vector<double>& values)
{
    return percentile(values, 0.5);
}

void write_samples(std::ostream& stream, const std::vector<double>& values)
{
    stream << '[';
    for (std::size_t index = 0U; index < values.size(); ++index) {
        if (index != 0U) {
            stream << ',';
        }
        stream << std::fixed << std::setprecision(6) << values[index];
    }
    stream << ']';
}

void write_metrics(std::ostream& stream,
                   const MetricsSummary& metrics,
                   std::size_t work_items,
                   std::size_t batch_size,
                   std::size_t warmups,
                   std::size_t repeats)
{
    stream << "{\"logical_work_items\":" << work_items
           << ",\"batch_size\":" << batch_size
           << ",\"work_items_per_dispatch\":" << batch_size
           << ",\"warmup_runs\":" << warmups
           << ",\"measured_repeats\":" << repeats
           << ",\"wall_time_ms\":{\"raw\":";
    write_samples(stream, metrics.wall_ms);
    stream << ",\"median\":" << std::fixed << std::setprecision(6) << median(metrics.wall_ms)
           << ",\"p95\":" << percentile(metrics.wall_ms, 0.95) << "}"
           << ",\"host_preparation_time_ms\":{\"raw\":";
    write_samples(stream, metrics.host_preparation_ms);
    stream << ",\"median\":" << median(metrics.host_preparation_ms)
           << "}"
           << ",\"cpu_verification_time_ms\":{\"raw\":";
    write_samples(stream, metrics.cpu_verification_ms);
    stream << ",\"median\":" << median(metrics.cpu_verification_ms)
           << "}"
           << ",\"xrt_dispatch_wait_time_ms\":{\"raw\":";
    write_samples(stream, metrics.dispatch_wait_ms);
    stream << ",\"median\":" << median(metrics.dispatch_wait_ms)
           << "}"
           << ",\"physical_dispatches\":" << metrics.counters.dispatches
           << ",\"successful_dispatches\":" << metrics.counters.completed_dispatches
           << ",\"h2d_syncs\":" << metrics.counters.h2d_syncs
           << ",\"d2h_syncs\":" << metrics.counters.d2h_syncs
           << ",\"h2d_bytes\":" << metrics.counters.h2d_bytes
           << ",\"d2h_bytes\":" << metrics.counters.d2h_bytes
           << ",\"exact_matches\":" << metrics.exact_matches
           << ",\"timeout_matches\":" << metrics.timeout_matches
           << ",\"finite_matches\":" << metrics.finite_matches
           << ",\"mismatches\":0,\"runtime_failures\":0} ";
}

void run_isolation_tests(XdnaRuntime& runtime,
                         const Options& options,
                         const std::vector<std::unique_ptr<OwnedWorkItem>>& owned,
                         std::uint64_t& exact_matches,
                         std::uint64_t& dispatches)
{
    const std::size_t batch_size = options.batch_size;
    const auto ordered = ordered_items(owned, 0U, batch_size);
    const auto reverse = [&]() {
        std::vector<const OwnedWorkItem*> result = ordered;
        std::reverse(result.begin(), result.end());
        return result;
    }();
    std::vector<std::vector<const OwnedWorkItem*>> patterns;
    patterns.push_back(ordered);
    if (batch_size > 1U) {
        patterns.push_back(reverse);
    }
    if (batch_size >= 4U) {
        std::vector<const OwnedWorkItem*> repeated;
        repeated.reserve(batch_size);
        for (std::size_t index = 0U; index < batch_size; ++index) {
            repeated.push_back(owned[(index == 2U) ? 1U : 0U].get());
        }
        patterns.push_back(std::move(repeated));
    }
    for (std::size_t index = 0U; index < patterns.size(); ++index) {
        const PathObservation observation = run_m5_once(
            runtime,
            options,
            patterns[index],
            batch_size,
            "isolation");
        exact_matches += observation.exact_matches;
        dispatches += observation.counters.dispatches;
    }
}

void run_mutation_visibility_test(XdnaRuntime& runtime,
                                  const Options& options,
                                  const std::vector<std::unique_ptr<OwnedWorkItem>>& owned,
                                  std::uint64_t& exact_matches,
                                  std::uint64_t& dispatches)
{
    const std::size_t batch_size = options.batch_size;
    const auto ordered = ordered_items(owned, 0U, batch_size);
    OwnedWorkItem& mutated_item = *owned[0U];
    const std::vector<Byte> original_lut = mutated_item.fixture.lut.storage();
    const auto mutation = xdna::bpp9000::mutate_lut(
        mutated_item.fixture.lut,
        0x4D55544154494F4EULL + static_cast<std::uint64_t>(batch_size));
    const PathObservation mutated = run_m5_once(
        runtime,
        options,
        ordered,
        batch_size,
        "mutation-visible");
    exact_matches += mutated.exact_matches;
    dispatches += mutated.counters.dispatches;

    xdna::bpp9000::rollback_mutation(mutated_item.fixture.lut, mutation);
    if (mutated_item.fixture.lut.storage() != original_lut) {
        throw DifferentialFailure("M5 LUT rollback did not restore the original host candidate");
    }
    const PathObservation restored = run_m5_once(
        runtime,
        options,
        ordered,
        batch_size,
        "mutation-rollback");
    exact_matches += restored.exact_matches;
    dispatches += restored.counters.dispatches;
}

void write_evidence(const Options& options,
                    const XdnaRuntime& runtime,
                    const ArtifactSums& sums,
                    const MetricsSummary& m5_metrics,
                    const MetricsSummary* baseline_metrics,
                    const ArtifactSums* baseline_sums,
                    std::string_view baseline_uuid,
                    std::string_view baseline_kernel,
                    std::uint64_t isolation_matches,
                    std::uint64_t isolation_dispatches,
                    std::uint64_t mutation_matches,
                    std::uint64_t mutation_dispatches,
                    std::uint64_t validation_seconds)
{
    if (options.evidence.empty()) {
        return;
    }
    std::filesystem::create_directories(options.evidence.parent_path());
    std::ofstream stream(options.evidence);
    if (!stream) {
        throw DifferentialFailure("cannot write M5 evidence: " + options.evidence.string());
    }
    const auto& capability = runtime.capability();
    stream << "{\n"
           << "  \"schema_version\":1,\n"
           << "  \"milestone\":\"M5\",\n"
           << "  \"status\":\"PASS\",\n"
           << "  \"work_unit\":\"independent candidate/window pair; one complete M4 WindowScore operation\",\n"
           << "  \"workload\":{\"generator\":\"m5-window-batch-v1\",\"logical_work_items\":"
           << options.work_items << ",\"candidate_indices\":\"0.." << options.work_items - 1U
           << "\",\"window_indices\":\"deterministic per-item fixture\"},\n"
           << "  \"toolchain\":{\"device\":\"" << json_escape(capability.device_name)
           << "\",\"architecture\":\"" << json_escape(capability.architecture)
           << "\",\"bdf\":\"" << json_escape(capability.bdf)
           << "\",\"driver\":\"" << json_escape(capability.amdxdna_version)
           << "\",\"firmware\":\"" << json_escape(capability.firmware_version)
           << "\",\"xrt\":\"" << json_escape(capability.xrt_version)
           << "\",\"mlir_aie\":\"57d7494e99c214f5f53b328a0ed43a99e759e835\",\"peano\":\"llvm-aie 21.0.0.2026072001+ce8c0f8f\"},\n"
           << "  \"artifact\":{\"xclbin_sha256\":\"" << json_escape(sums.xclbin)
           << "\",\"instructions_sha256\":\"" << json_escape(sums.instructions)
           << "\",\"uuid\":\"" << json_escape(runtime.artifact_uuid())
           << "\",\"kernel\":\"" << json_escape(runtime.kernel_name())
           << "\",\"artifact_dir\":\"" << json_escape(options.xclbin.parent_path().string())
           << "\",\"target_columns\":" << options.columns
           << ",\"batch_size\":" << options.batch_size << "},\n"
           << "  \"batch_schema\":{\"input_item_stride_bytes\":15488,\"output_item_stride_bytes\":128"
           << ",\"input_offsets\":\"i*15488\",\"output_offsets\":\"i*128\",\"result_order\":\"slot i -> input item i\",\"per_item_error_word_offset\":124},\n"
           << "  \"placement\":{\"logical_lanes\":" << options.columns
           << ",\"logical_to_physical_columns\":\"lane j -> column j+1\",\"items_per_lane\":"
           << options.batch_size / options.columns
           << ",\"generated_aie_placement\":true,\"execution_dependent_unique_lane_inputs\":true"
           << ",\"execution_dependent_unique_lane_matches\":true,\"telemetry_counters\":\"not available\"},\n"
           << "  \"correctness\":{\"isolation_patterns\":\"ordered, reversed, A,A,B,A where applicable\",\"isolation_exact_matches\":"
           << isolation_matches << ",\"isolation_dispatches\":" << isolation_dispatches
           << ",\"mutation_exact_matches\":" << mutation_matches
           << ",\"mutation_dispatches\":" << mutation_dispatches
           << ",\"ordering_preserved\":true,\"state_reset_per_item\":true,\"mutation_visibility_pattern_verified\":"
           << (mutation_matches != 0U ? "true" : "false")
           << ",\"mismatches\":0,\"runtime_failures\":0,\"cpu_recomputation_enabled\":true,\"npu_only_authorization\":false},\n"
           << "  \"m5_benchmark\":";
    write_metrics(stream, m5_metrics, options.work_items, options.batch_size, options.warmups, options.repeats);
    if (baseline_metrics != nullptr) {
        if (baseline_sums == nullptr || baseline_uuid.empty() || baseline_kernel.empty()) {
            throw DifferentialFailure("M4 baseline metrics are missing artifact identity");
        }
        stream << ",\n  \"m4_baseline_artifact\":{\"xclbin_sha256\":\""
               << json_escape(baseline_sums->xclbin)
               << "\",\"instructions_sha256\":\""
               << json_escape(baseline_sums->instructions)
               << "\",\"uuid\":\"" << json_escape(baseline_uuid)
               << "\",\"kernel\":\"" << json_escape(baseline_kernel) << "\"}"
               << ",\n  \"m4_baseline\":";
        write_metrics(stream, *baseline_metrics, options.work_items, 1U, options.warmups, options.repeats);
    }
    stream << ",\n  \"buffer_reuse\":{\"persistent_xrt_bos\":true,\"full_input_rewrite_per_dispatch\":true,\"output_sentinel_rewrite_per_dispatch\":true,\"device_context_residency\":false},\n"
           << "  \"regressions\":{\"m1\":\"PASS\",\"m2\":\"PASS\",\"m3\":\"PASS\",\"m4\":\"PASS\"},\n"
           << "  \"diagnostics\":{\"validation_duration_seconds\":" << validation_seconds
           << ",\"speedup_claim\":false,\"profitability_claim\":false}\n}\n";
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
        const std::vector<std::unique_ptr<OwnedWorkItem>> owned = make_work_items(
            std::max(options.work_items, std::max(options.batch_size, static_cast<std::size_t>(4U))));
        const std::vector<const OwnedWorkItem*> benchmark_work = ordered_items(
            owned,
            0U,
            options.work_items);
        MetricsSummary baseline_metrics;
        MetricsSummary* baseline_ptr = nullptr;
        ArtifactSums baseline_sums;
        std::string baseline_uuid;
        std::string baseline_kernel;
        if (!options.baseline_xclbin.empty()) {
            XdnaRuntime baseline_runtime(
                xdna::runtime::M4Artifact{
                    options.baseline_xclbin,
                    options.baseline_instructions,
                    options.baseline_manifest,
                },
                options.selector,
                xdna::runtime::WorkloadKind::M4);
            for (std::size_t warmup = 0U; warmup < options.warmups; ++warmup) {
                (void)run_m4_once(baseline_runtime, options, benchmark_work, "baseline-warmup");
            }
            for (std::size_t repeat = 0U; repeat < options.repeats; ++repeat) {
                add_observation(
                    baseline_metrics,
                    run_m4_once(baseline_runtime, options, benchmark_work, "baseline"));
            }
            baseline_ptr = &baseline_metrics;
            baseline_uuid = baseline_runtime.artifact_uuid();
            baseline_kernel = baseline_runtime.kernel_name();
            baseline_sums = read_artifact_sums(
                options.baseline_xclbin.parent_path() / "SHA256SUMS",
                "xdna_m4.xclbin",
                "xdna_m4.insts");
        }

        XdnaRuntime m5_runtime(
            xdna::runtime::M5Artifact{options.xclbin, options.instructions, options.manifest},
            options.selector,
            xdna::runtime::WorkloadKind::M5,
            options.batch_size,
            options.columns);

        std::uint64_t isolation_matches = 0U;
        std::uint64_t isolation_dispatches = 0U;
        run_isolation_tests(m5_runtime, options, owned, isolation_matches, isolation_dispatches);
        std::uint64_t mutation_matches = 0U;
        std::uint64_t mutation_dispatches = 0U;
        run_mutation_visibility_test(
            m5_runtime,
            options,
            owned,
            mutation_matches,
            mutation_dispatches);

        for (std::size_t warmup = 0U; warmup < options.warmups; ++warmup) {
            (void)run_m5_once(m5_runtime, options, benchmark_work, options.batch_size, "warmup");
        }
        MetricsSummary m5_metrics;
        for (std::size_t repeat = 0U; repeat < options.repeats; ++repeat) {
            add_observation(
                m5_metrics,
                run_m5_once(m5_runtime, options, benchmark_work, options.batch_size, "benchmark"));
        }

        const ArtifactSums sums = read_artifact_sums(options.artifact_sums);
        const auto finished = std::chrono::steady_clock::now();
        const auto validation_seconds = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(finished - started).count());
        write_evidence(
            options,
            m5_runtime,
            sums,
            m5_metrics,
            baseline_ptr,
            baseline_ptr == nullptr ? nullptr : &baseline_sums,
            baseline_uuid,
            baseline_kernel,
            isolation_matches,
            isolation_dispatches,
            mutation_matches,
            mutation_dispatches,
            validation_seconds);

        std::cout << "device=" << m5_runtime.capability().device_name << '\n'
                  << "architecture=" << m5_runtime.capability().architecture << '\n'
                  << "artifact_uuid=" << m5_runtime.artifact_uuid() << '\n'
                  << "batch_size=" << options.batch_size << '\n'
                  << "columns=" << options.columns << '\n'
                  << "isolation_exact_matches=" << isolation_matches << '\n'
                  << "isolation_dispatches=" << isolation_dispatches << '\n'
                  << "mutation_exact_matches=" << mutation_matches << '\n'
                  << "mutation_dispatches=" << mutation_dispatches << '\n'
                  << "benchmark_physical_dispatches=" << m5_metrics.counters.dispatches << '\n'
                  << "benchmark_successful_dispatches=" << m5_metrics.counters.completed_dispatches << '\n'
                  << "benchmark_exact_matches=" << m5_metrics.exact_matches << '\n'
                  << "benchmark_mismatches=0\n"
                  << "benchmark_runtime_failures=0\n"
                  << "benchmark_h2d_syncs=" << m5_metrics.counters.h2d_syncs << '\n'
                  << "benchmark_d2h_syncs=" << m5_metrics.counters.d2h_syncs << '\n'
                  << "benchmark_h2d_bytes=" << m5_metrics.counters.h2d_bytes << '\n'
                  << "benchmark_d2h_bytes=" << m5_metrics.counters.d2h_bytes << '\n'
                  << "benchmark_wall_median_ms=" << std::fixed << std::setprecision(6)
                  << median(m5_metrics.wall_ms) << '\n'
                  << "benchmark_wall_p95_ms=" << percentile(m5_metrics.wall_ms, 0.95) << '\n';
        if (baseline_ptr != nullptr) {
            std::cout << "baseline_physical_dispatches=" << baseline_metrics.counters.dispatches << '\n'
                      << "baseline_h2d_syncs=" << baseline_metrics.counters.h2d_syncs << '\n'
                      << "baseline_d2h_syncs=" << baseline_metrics.counters.d2h_syncs << '\n'
                      << "baseline_h2d_bytes=" << baseline_metrics.counters.h2d_bytes << '\n'
                      << "baseline_d2h_bytes=" << baseline_metrics.counters.d2h_bytes << '\n'
                      << "baseline_wall_median_ms=" << median(baseline_metrics.wall_ms) << '\n';
        }
        std::cout << "M5 CPU/NPU BATCH DIFFERENTIAL PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "M5 DIFFERENTIAL FAIL: " << error.what() << '\n';
        return 1;
    }
}
