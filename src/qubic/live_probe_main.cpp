#include "qubic/direct_node.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using xdna::bpp9000::Byte;
using xdna::qubic::Algorithm;
using xdna::qubic::ContextUpdate;
using xdna::qubic::NodeEndpoint;
using xdna::qubic::ReconnectPolicy;
using xdna::qubic::RuntimeConfig;
using xdna::qubic::SystemInfo;
using xdna::qubic::TaskIdentity;
using xdna::qubic::TransportTimeouts;
using xdna::qubic::WorkContext;
using xdna::qubic::WorkContextTracker;

constexpr std::string_view kCoreRevision =
    "a83f935406cd006b5b1a94971139e74d410ecb6d";
constexpr std::string_view kCoreReference = "v1.301.3";
constexpr std::string_view kTaskIdentitySource = "recorded-core-v1.301.3";

struct Options {
    RuntimeConfig config{};
    std::uint32_t repeat = 2U;
};

[[nodiscard]] std::uint64_t parse_unsigned(std::string_view text,
                                            std::string_view field,
                                            std::uint64_t minimum,
                                            std::uint64_t maximum)
{
    std::uint64_t value = 0U;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()
        || value < minimum || value > maximum) {
        throw std::runtime_error(std::string(field) + " is outside the allowed range");
    }
    return value;
}

[[nodiscard]] std::string utc_timestamp()
{
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
    if (::gmtime_r(&now, &utc) == nullptr) {
        throw std::runtime_error("UTC timestamp conversion failed");
    }
    std::array<char, 32U> buffer{};
    if (std::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0U) {
        throw std::runtime_error("UTC timestamp formatting failed");
    }
    return std::string(buffer.data());
}

[[nodiscard]] std::string hex_u64(std::uint64_t value)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string result(16U, '0');
    for (std::size_t index = 0U; index < result.size(); ++index) {
        result[result.size() - 1U - index] = digits[value & 0x0FU];
        value >>= 4U;
    }
    return result;
}

[[nodiscard]] std::string seed_fingerprint(const SystemInfo& info)
{
    // This is a short, non-secret diagnostic fingerprint. It is deliberately
    // not a seed dump and does not require the optional production crypto.
    std::uint64_t value = 1469598103934665603ULL;
    for (const Byte byte : info.mining_seed.bytes) {
        value ^= byte;
        value *= 1099511628211ULL;
    }
    return hex_u64(value);
}

[[nodiscard]] TaskIdentity recorded_production_task_identity()
{
    TaskIdentity identity;
    identity.shape = xdna::bpp9000::production_shape();
    identity.topology_hash.bytes = {
        0x13U, 0xE9U, 0x9DU, 0x5BU, 0x2FU, 0xCAU, 0x56U, 0xAAU,
        0x78U, 0x9CU, 0xB9U, 0x59U, 0x57U, 0x5FU, 0x48U, 0x39U,
        0x2FU, 0x1AU, 0x44U, 0x90U, 0x9AU, 0x8EU, 0xAFU, 0x27U,
        0xF2U, 0xDEU, 0x8FU, 0x8DU, 0x74U, 0xB0U, 0x7AU, 0x6BU,
    };
    identity.data_hash.bytes = {
        0x97U, 0x9CU, 0xDCU, 0x22U, 0x47U, 0xD2U, 0xCAU, 0x4EU,
        0xD3U, 0xD6U, 0x14U, 0xBFU, 0x27U, 0x89U, 0x63U, 0x84U,
        0xCBU, 0x1CU, 0x9CU, 0x3DU, 0x80U, 0x4AU, 0xF6U, 0xEDU,
        0xE6U, 0xB5U, 0x9FU, 0xC5U, 0x2CU, 0x0EU, 0x3DU, 0xFAU,
    };
    return identity;
}

[[nodiscard]] const char* context_update_name(ContextUpdate update) noexcept
{
    switch (update) {
    case ContextUpdate::Installed:
        return "INSTALLED";
    case ContextUpdate::Unchanged:
        return "UNCHANGED";
    case ContextUpdate::Advanced:
        return "ADVANCED";
    case ContextUpdate::RejectedOlderEpoch:
        return "REJECTED_OLDER_EPOCH";
    case ContextUpdate::RejectedOlderTick:
        return "REJECTED_OLDER_TICK";
    }
    return "UNKNOWN";
}

void print_help(const char* program)
{
    std::cout << "Usage: " << program << " [options]\n"
              << "  --host HOST          override XDNA_QUBIC_NODE_HOST\n"
              << "  --port PORT          override XDNA_QUBIC_NODE_PORT\n"
              << "  --timeout-ms MS      bounded connect/read/write timeout (1..60000)\n"
              << "  --attempts COUNT     bounded reconnect attempts (1..4)\n"
              << "  --repeat COUNT       system-info connections (1..4; default 2)\n"
              << "  --help               show this help\n";
}

[[nodiscard]] Options parse_options(int argc, char** argv)
{
    Options options;
    options.config = xdna::qubic::load_runtime_config_from_environment();
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        const auto require_value = [&](std::string_view option) -> std::string_view {
            if (index + 1 >= argc) {
                throw std::runtime_error(std::string(option) + " requires a value");
            }
            ++index;
            return argv[index];
        };
        if (argument == "--help") {
            print_help(argv[0]);
            std::exit(0);
        }
        if (argument == "--host") {
            options.config.endpoint.host = std::string(require_value(argument));
            if (options.config.endpoint.host.empty()) {
                throw std::runtime_error("--host cannot be empty");
            }
        } else if (argument == "--port") {
            options.config.endpoint.port = static_cast<std::uint16_t>(
                parse_unsigned(require_value(argument), "--port", 1U, 65535U));
        } else if (argument == "--timeout-ms") {
            const auto milliseconds = parse_unsigned(require_value(argument), "--timeout-ms", 1U, 60000U);
            const auto timeout = std::chrono::milliseconds(milliseconds);
            options.config.timeouts = TransportTimeouts{timeout, timeout, timeout};
        } else if (argument == "--attempts") {
            options.config.reconnect.max_attempts = static_cast<std::uint32_t>(
                parse_unsigned(require_value(argument), "--attempts", 1U, 4U));
        } else if (argument == "--repeat") {
            options.repeat = static_cast<std::uint32_t>(
                parse_unsigned(require_value(argument), "--repeat", 1U, 4U));
        } else {
            throw std::runtime_error("unknown option: " + std::string(argument));
        }
    }
    return options;
}

void print_system_info(std::uint32_t sample,
                       const NodeEndpoint& endpoint,
                       const SystemInfo& info,
                       const WorkContext& context,
                       ContextUpdate update,
                       bool previous_context_fresh)
{
    std::cout << "sample=" << sample << '\n'
              << "endpoint=" << endpoint.host << ':' << endpoint.port << '\n'
              << "protocol_core_revision=" << kCoreReference << '@' << kCoreRevision << '\n'
              << "request_frame_type=" << static_cast<unsigned int>(xdna::qubic::kRequestSystemInfo) << '\n'
              << "request_frame_bytes=" << xdna::qubic::kRequestResponseHeaderBytes << '\n'
              << "request_transmitted=true\n"
              << "response_frame_type=" << static_cast<unsigned int>(xdna::qubic::kRespondSystemInfo) << '\n'
              << "response_frame_bytes="
              << xdna::qubic::kRequestResponseHeaderBytes + xdna::qubic::kSystemInfoPayloadBytes << '\n'
              << "response_payload_bytes=" << xdna::qubic::kSystemInfoPayloadBytes << '\n'
              << "response_received=true\n"
              << "version=" << info.version << '\n'
              << "epoch=" << info.epoch << '\n'
              << "tick=" << info.tick << '\n'
              << "latest_created_tick=" << info.latest_created_tick << '\n'
              << "threshold=" << context.solution_threshold << "\n"
              << "mining_seed_nonzero=" << (xdna::bpp9000::is_valid_mining_seed(info.mining_seed) ? "true" : "false") << '\n'
              << "mining_seed_fingerprint_fnv1a64=" << seed_fingerprint(info) << '\n'
              << "algorithm=BPP9000(nonce[0]=1)\n"
              << "task_identity_source=" << kTaskIdentitySource << '\n'
              << "task_bytes_verified=false\n"
              << "live_task_identity_in_response=false\n"
              << "work_context=CONSTRUCTED_FROM_RECORDED_IDENTITY\n"
              << "work_context_windows=" << context.number_of_windows << '\n'
              << "context_update=" << context_update_name(update) << '\n'
              << "current_context_valid=true\n"
              << "previous_context_fresh=" << (previous_context_fresh ? "true" : "false") << '\n'
              << "stale_candidate_check=NO_CANDIDATE\n"
              << "live_submission=DISABLED\n"
              << "utc=" << utc_timestamp() << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const Options options = parse_options(argc, argv);
        if (options.config.endpoint.host.empty() || options.config.endpoint.port == 0U) {
            throw std::runtime_error("endpoint is empty or has port zero");
        }

        xdna::qubic::TcpConnectionFactory factory;
        xdna::qubic::DirectNodeClient client(
            factory, options.config.endpoint, options.config.timeouts, options.config.reconnect,
            options.config.read_only_limits);
        const TaskIdentity task = recorded_production_task_identity();
        WorkContextTracker tracker;
        std::optional<WorkContext> previous;
        for (std::uint32_t sample = 1U; sample <= options.repeat; ++sample) {
            const SystemInfo info = client.request_system_info();
            const WorkContext context = xdna::qubic::make_work_context(info, task, Algorithm::Bpp9000);
            const ContextUpdate update = tracker.observe(context);
            const bool previous_context_fresh = previous.has_value() && tracker.is_fresh(*previous);
            print_system_info(sample,
                              client.endpoint(),
                              info,
                              context,
                              update,
                              previous_context_fresh || !previous.has_value());
            previous = context;
        }
        std::cout << "reconnect_pass=" << (options.repeat >= 2U ? "true" : "NOT_RUN") << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "qubic_live_probe_error=" << error.what() << '\n';
        return 1;
    }
}
