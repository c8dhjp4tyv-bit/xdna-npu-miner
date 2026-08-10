#include "qubic/local_identity.hpp"
#include "qubic/production_crypto.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using xdna::bpp9000::Byte;
using xdna::qubic::ComputorList;
using xdna::qubic::DirectNodeClient;
using xdna::qubic::EntityInfo;
using xdna::qubic::K12FourQCryptoProvider;
using xdna::qubic::NodeEndpoint;
using xdna::qubic::ProtocolError;
using xdna::qubic::PublicKey;
using xdna::qubic::ReadOnlyRequestDiagnostics;
using xdna::qubic::ReconnectPolicy;
using xdna::qubic::RuntimeConfig;
using xdna::qubic::SigningSecret;
using xdna::qubic::SystemInfo;
using xdna::qubic::TcpConnectionFactory;
using xdna::qubic::TransportError;

constexpr std::array<std::string_view, 8U> kSequences{
    "A_qubic_live_probe_system_info",
    "B_m6_authorization_system_info_stage",
    "C_system_info_stop",
    "D_computors_stop",
    "E_entity_stop",
    "F_system_info_computors",
    "G_computors_system_info",
    "H_system_info_computors_entity",
};

struct Options {
    RuntimeConfig config{};
    std::uint32_t repeat = 3U;
    std::string sequence = "all";
    std::filesystem::path secret_path;
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

[[nodiscard]] bool has_entity_step(std::string_view sequence) noexcept
{
    return sequence == "E_entity_stop" || sequence == "H_system_info_computors_entity";
}

[[nodiscard]] std::string_view sequence_name(std::string_view sequence)
{
    for (const std::string_view candidate : kSequences) {
        if (candidate == sequence) {
            return candidate;
        }
    }
    throw std::runtime_error("unknown diagnostic sequence: " + std::string(sequence));
}

[[nodiscard]] Options parse_options(int argc, char** argv)
{
    Options options;
    options.config = xdna::qubic::load_runtime_config_from_environment();
    const char* default_secret = std::getenv("XDNA_M6_SECRET_PATH");
    options.secret_path = default_secret == nullptr
        ? std::filesystem::path(".local-secrets/m6-signing-subseed")
        : std::filesystem::path(default_secret);

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        const auto require_value = [&](std::string_view option) -> std::string_view {
            if (index + 1 >= argc) {
                throw std::runtime_error(std::string(option) + " requires a value");
            }
            ++index;
            return argv[index];
        };
        if (argument == "--host") {
            options.config.endpoint.host = std::string(require_value(argument));
        } else if (argument == "--port") {
            options.config.endpoint.port = static_cast<std::uint16_t>(
                parse_unsigned(require_value(argument), "--port", 1U, 65535U));
        } else if (argument == "--timeout-ms") {
            const auto value = parse_unsigned(require_value(argument), "--timeout-ms", 1U, 60000U);
            const auto timeout = std::chrono::milliseconds(value);
            options.config.timeouts = {timeout, timeout, timeout};
        } else if (argument == "--deadline-ms") {
            options.config.read_only_limits.deadline = std::chrono::milliseconds(
                parse_unsigned(require_value(argument), "--deadline-ms", 1U, 120000U));
        } else if (argument == "--attempts") {
            options.config.reconnect.max_attempts = static_cast<std::uint32_t>(
                parse_unsigned(require_value(argument), "--attempts", 1U, 8U));
        } else if (argument == "--repeat") {
            options.repeat = static_cast<std::uint32_t>(
                parse_unsigned(require_value(argument), "--repeat", 1U, 5U));
        } else if (argument == "--sequence") {
            options.sequence = std::string(require_value(argument));
        } else if (argument == "--secret-path") {
            options.secret_path = require_value(argument);
        } else if (argument == "--help") {
            std::cout << "Usage: " << argv[0]
                      << " [--sequence NAME|all] [--repeat 1..5] [--secret-path PATH]"
                      << " [--host HOST] [--port PORT] [--timeout-ms MS]"
                      << " [--deadline-ms MS] [--attempts COUNT]\n";
            std::exit(0);
        } else {
            throw std::runtime_error("unknown option: " + std::string(argument));
        }
    }
    if (options.config.endpoint.host.empty() || options.config.endpoint.port == 0U) {
        throw std::runtime_error("endpoint is empty or has port zero");
    }
    if (options.sequence != "all") {
        (void)sequence_name(options.sequence);
    }
    return options;
}

void print_configuration(const RuntimeConfig& config)
{
    std::cout << "configuration_endpoint=" << config.endpoint.host << ':' << config.endpoint.port << '\n'
              << "configuration_connect_timeout_ms=" << config.timeouts.connect.count() << '\n'
              << "configuration_read_timeout_ms=" << config.timeouts.read.count() << '\n'
              << "configuration_write_timeout_ms=" << config.timeouts.write.count() << '\n'
              << "configuration_absolute_deadline_ms=" << config.read_only_limits.deadline.count() << '\n'
              << "configuration_attempts=" << config.reconnect.max_attempts << '\n'
              << "configuration_max_ignored_bytes=" << config.read_only_limits.maximum_ignored_bytes << '\n'
              << "configuration_max_ignored_frames=" << config.read_only_limits.maximum_ignored_frames << '\n'
              << "configuration_handshake=exchange_public_peers_then_request_without_sleep\n"
              << "configuration_request_client_lifetime=one_direct_node_client_per_sequence_repeat\n"
              << "configuration_tcp_connection_lifetime=one_fresh_connection_per_query\n";
}

void print_diagnostics(std::string_view sequence,
                      std::uint32_t repeat,
                      std::uint32_t request_index,
                      const ReadOnlyRequestDiagnostics& diagnostics,
                      std::string_view result,
                      std::string_view error)
{
    std::cout << "matrix_sequence=" << sequence << '\n'
              << "matrix_repeat=" << repeat << '\n'
              << "matrix_request_index=" << request_index << '\n'
              << "request_type=" << static_cast<unsigned int>(diagnostics.request_type) << '\n'
              << "request_dejavu=" << diagnostics.request_dejavu << '\n'
              << "desired_response_type=" << static_cast<unsigned int>(diagnostics.desired_response_type) << '\n'
              << "accepted_response_dejavu=";
    if (diagnostics.accepted_response_dejavu.has_value()) {
        std::cout << *diagnostics.accepted_response_dejavu;
    } else {
        std::cout << "none";
    }
    std::cout << '\n'
              << "connection_attempts=" << diagnostics.connection_attempts << '\n'
              << "connections_opened=" << diagnostics.connections_opened << '\n'
              << "ignored_frame_count=" << diagnostics.ignored_frames << '\n'
              << "ignored_byte_count=" << diagnostics.ignored_bytes << '\n'
              << "elapsed_ms=" << diagnostics.elapsed_ms << '\n'
              << "same_type_wrong_dejavu_frames=" << diagnostics.same_type_wrong_dejavu_frames << '\n'
              << "deadline_result=" << (diagnostics.deadline_exceeded ? "deadline_exceeded" : "not_deadline_exceeded") << '\n'
              << "result=" << result << '\n';
    for (std::size_t type = 0U; type < diagnostics.ignored_type_counts.size(); ++type) {
        if (diagnostics.ignored_type_counts[type] != 0U) {
            std::cout << "ignored_type_" << type << '=' << diagnostics.ignored_type_counts[type] << '\n';
        }
    }
    if (!error.empty()) {
        std::cout << "error=" << error << '\n';
    }
}

[[nodiscard]] bool load_entity_public_key(const std::filesystem::path& path,
                                          PublicKey& public_key)
{
    SigningSecret secret;
    std::string error;
    if (!xdna::qubic::load_signing_subseed_file(path, secret, error)) {
        std::cout << "entity_query_key=unavailable\n"
                  << "entity_query_key_reason=local_identity_unavailable\n";
        return false;
    }
    K12FourQCryptoProvider provider;
    if (!provider.derive_public_key(std::span<const Byte>(secret.bytes), public_key)) {
        std::cout << "entity_query_key=unavailable\n"
                  << "entity_query_key_reason=public_key_derivation_failed\n";
        return false;
    }
    std::cout << "entity_query_key=derived_from_owner_only_file\n";
    return true;
}

int run_sequence(const Options& options,
                 std::string_view sequence,
                 const PublicKey* entity_public_key)
{
    std::cout << "begin_sequence=" << sequence << '\n';
    int failures = 0;
    for (std::uint32_t repeat = 1U; repeat <= options.repeat; ++repeat) {
        const int failures_before_repeat = failures;
        TcpConnectionFactory factory;
        DirectNodeClient client(factory,
                                options.config.endpoint,
                                options.config.timeouts,
                                options.config.reconnect,
                                options.config.read_only_limits);
        std::uint32_t request_index = 0U;
        const auto run_system_info = [&]() {
            ++request_index;
            ReadOnlyRequestDiagnostics diagnostics;
            try {
                const SystemInfo info = client.request_system_info(&diagnostics);
                (void)info;
                print_diagnostics(sequence, repeat, request_index, diagnostics, "PASS", {});
            } catch (const ProtocolError& error) {
                ++failures;
                print_diagnostics(sequence, repeat, request_index, diagnostics, "PROTOCOL_FAILURE", error.what());
            } catch (const TransportError& error) {
                ++failures;
                print_diagnostics(sequence, repeat, request_index, diagnostics, "TRANSPORT_FAILURE", error.what());
            }
        };
        const auto run_computors = [&]() {
            ++request_index;
            ReadOnlyRequestDiagnostics diagnostics;
            try {
                const ComputorList computors = client.request_computors(&diagnostics);
                (void)computors;
                print_diagnostics(sequence, repeat, request_index, diagnostics, "PASS", {});
            } catch (const ProtocolError& error) {
                ++failures;
                print_diagnostics(sequence, repeat, request_index, diagnostics, "PROTOCOL_FAILURE", error.what());
            } catch (const TransportError& error) {
                ++failures;
                print_diagnostics(sequence, repeat, request_index, diagnostics, "TRANSPORT_FAILURE", error.what());
            }
        };
        const auto run_entity = [&]() {
            ++request_index;
            ReadOnlyRequestDiagnostics diagnostics;
            if (entity_public_key == nullptr) {
                ++failures;
                print_diagnostics(sequence, repeat, request_index, diagnostics, "UNAVAILABLE",
                                  "owner-only entity query public key was not available");
                return;
            }
            try {
                const EntityInfo entity = client.request_entity(*entity_public_key, &diagnostics);
                (void)entity;
                print_diagnostics(sequence, repeat, request_index, diagnostics, "PASS", {});
            } catch (const ProtocolError& error) {
                ++failures;
                print_diagnostics(sequence, repeat, request_index, diagnostics, "PROTOCOL_FAILURE", error.what());
            } catch (const TransportError& error) {
                ++failures;
                print_diagnostics(sequence, repeat, request_index, diagnostics, "TRANSPORT_FAILURE", error.what());
            }
        };

        if (sequence == "A_qubic_live_probe_system_info"
            || sequence == "B_m6_authorization_system_info_stage"
            || sequence == "C_system_info_stop") {
            run_system_info();
        } else if (sequence == "D_computors_stop") {
            run_computors();
        } else if (sequence == "E_entity_stop") {
            run_entity();
        } else if (sequence == "F_system_info_computors") {
            run_system_info();
            if (request_index == 1U && failures == failures_before_repeat) {
                run_computors();
            }
        } else if (sequence == "G_computors_system_info") {
            run_computors();
            if (request_index == 1U && failures == failures_before_repeat) {
                run_system_info();
            }
        } else if (sequence == "H_system_info_computors_entity") {
            run_system_info();
            if (request_index == 1U && failures == failures_before_repeat) {
                run_computors();
            }
            if (request_index == 2U && failures == failures_before_repeat) {
                run_entity();
            }
        }
        std::cout << "end_repeat=" << repeat << " sequence=" << sequence << '\n';
    }
    std::cout << "end_sequence=" << sequence << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const Options options = parse_options(argc, argv);
        print_configuration(options.config);

        PublicKey entity_public_key{};
        const bool entity_key_available = options.sequence == "all"
            ? load_entity_public_key(options.secret_path, entity_public_key)
            : (has_entity_step(options.sequence) && load_entity_public_key(options.secret_path, entity_public_key));
        const PublicKey* entity_key = entity_key_available ? &entity_public_key : nullptr;

        int failures = 0;
        if (options.sequence == "all") {
            for (const std::string_view sequence : kSequences) {
                failures += run_sequence(options, sequence, entity_key);
            }
        } else {
            failures = run_sequence(options, sequence_name(options.sequence), entity_key);
        }
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "m6_read_only_diagnostics_error=" << error.what() << '\n';
        return 2;
    }
}
