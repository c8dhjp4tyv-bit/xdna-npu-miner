#include "qubic/local_identity.hpp"
#include "qubic/production_crypto.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using xdna::bpp9000::Byte;
using xdna::qubic::ComputorList;
using xdna::qubic::DirectNodeClient;
using xdna::qubic::EntityInfo;
using xdna::qubic::K12FourQCryptoProvider;
using xdna::qubic::NodeEndpoint;
using xdna::qubic::PublicKey;
using xdna::qubic::SigningSecret;
using xdna::qubic::SystemInfo;
using xdna::qubic::TcpConnectionFactory;
using xdna::qubic::TransportError;
using xdna::qubic::TransportTimeouts;
using xdna::qubic::ReconnectPolicy;

constexpr std::string_view kArbitratorIdentity =
    "AFZPUAIYVPNUYGJRQVLUKOPPVLHAZQTGLYAAUUNBXFTVTAMSBKQBLEIEPCVJ";
constexpr std::uint64_t kMessageDisseminationThreshold = 1'000'000'000ULL;

[[nodiscard]] std::string public_key_hex(const PublicKey& public_key)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(public_key.bytes.size() * 2U);
    for (const Byte value : public_key.bytes) {
        result.push_back(digits[value >> 4U]);
        result.push_back(digits[value & 0x0FU]);
    }
    return result;
}

[[nodiscard]] bool parse_port(std::string_view text, std::uint16_t& port) noexcept
{
    std::uint64_t value = 0U;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()
        || value == 0U || value > 65535U) {
        return false;
    }
    port = static_cast<std::uint16_t>(value);
    return true;
}

[[nodiscard]] bool parse_arguments(int argc,
                                   char** argv,
                                   std::filesystem::path& secret_path,
                                   NodeEndpoint& endpoint)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--secret-path" && index + 1 < argc) {
            secret_path = argv[++index];
        } else if (argument == "--host" && index + 1 < argc) {
            endpoint.host = argv[++index];
        } else if (argument == "--port" && index + 1 < argc) {
            if (!parse_port(argv[++index], endpoint.port)) {
                return false;
            }
        } else if (argument == "--help") {
            return false;
        } else {
            return false;
        }
    }
    return !secret_path.empty() && !endpoint.host.empty() && endpoint.port != 0U;
}

[[nodiscard]] std::string_view classify_transport_reason(std::string_view detail) noexcept
{
    if (detail.find("request_deadline_exceeded") != std::string_view::npos) {
        return "request_deadline_exceeded";
    }
    if (detail.find("ignored_byte_ceiling_exceeded") != std::string_view::npos) {
        return "ignored_byte_ceiling_exceeded";
    }
    if (detail.find("ignored_frame_ceiling_exceeded") != std::string_view::npos) {
        return "ignored_frame_ceiling_exceeded";
    }
    if (detail.find("Resource temporarily unavailable") != std::string_view::npos) {
        return "transport_timeout";
    }
    return "transport_check_failed";
}

void print_unavailable(std::string_view stage, std::string_view reason)
{
    std::cout << "CHECK_UNAVAILABLE\n";
    std::cout << "stage=" << stage << '\n';
    std::cout << "reason=" << reason << '\n';
}

[[nodiscard]] bool all_computor_keys_nonzero(const ComputorList& computors) noexcept
{
    for (const PublicKey& public_key : computors.public_keys) {
        if (public_key.is_zero()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool energy_meets_threshold(const EntityInfo& entity) noexcept
{
    // Core's spectrum energy is incomingAmount - outgoingAmount. Amounts are
    // expected to be nonnegative; reject malformed signed values instead of
    // allowing an overflow or an inferred balance to authorize submission.
    if (entity.incoming_amount < 0 || entity.outgoing_amount < 0) {
        return false;
    }
    if (entity.outgoing_amount > std::numeric_limits<std::int64_t>::max()
        - static_cast<std::int64_t>(kMessageDisseminationThreshold)) {
        return false;
    }
    const std::int64_t required = entity.outgoing_amount
        + static_cast<std::int64_t>(kMessageDisseminationThreshold);
    return entity.incoming_amount >= required;
}

void print_safe_metadata(const NodeEndpoint& endpoint,
                         const PublicKey& source_public_key,
                         const SystemInfo& system_info,
                         const ComputorList& computors,
                         bool computors_epoch_matches,
                         bool computor_keys_nonzero,
                         bool signature_verified,
                         const EntityInfo& entity,
                         bool source_is_computor,
                         bool source_is_funded,
                         const PublicKey& destination)
{
    const std::string source_identity = xdna::qubic::public_identity_from_public_key(source_public_key);
    const std::string destination_identity = xdna::qubic::public_identity_from_public_key(destination);
    std::cout << "source_public_key_hex=" << public_key_hex(source_public_key) << '\n';
    std::cout << "source_identity=" << source_identity << '\n';
    std::cout << "endpoint=" << endpoint.host << ':' << endpoint.port << '\n';
    std::cout << "system_epoch=" << system_info.epoch << '\n';
    std::cout << "system_tick=" << system_info.tick << '\n';
    std::cout << "computors_epoch=" << computors.epoch << '\n';
    std::cout << "computor_epoch_matches_system=" << (computors_epoch_matches ? "true" : "false") << '\n';
    std::cout << "computor_count=" << computors.public_keys.size() << '\n';
    std::cout << "computor_keys_nonzero=" << (computor_keys_nonzero ? "true" : "false") << '\n';
    std::cout << "computors_signature_verified=" << (signature_verified ? "true" : "false") << '\n';
    std::cout << "entity_tick=" << entity.tick << '\n';
    std::cout << "entity_spectrum_index=" << entity.spectrum_index << '\n';
    std::cout << "entity_incoming_amount=" << entity.incoming_amount << '\n';
    std::cout << "entity_outgoing_amount=" << entity.outgoing_amount << '\n';
    std::cout << "entity_energy_threshold_met=" << (source_is_funded ? "true" : "false") << '\n';
    std::cout << "message_dissemination_threshold=" << kMessageDisseminationThreshold << '\n';
    std::cout << "source_is_current_computor=" << (source_is_computor ? "true" : "false") << '\n';
    std::cout << "destination_selection=first_verified_current_computor_key\n";
    std::cout << "destination_public_key_hex=" << public_key_hex(destination) << '\n';
    std::cout << "destination_identity=" << destination_identity << '\n';
    std::cout << "submission_performed=false\n";
    std::cout << "secret_output=never\n";
}

[[nodiscard]] int usage(const char* program)
{
    std::cerr << "usage: " << program
              << " --secret-path PATH [--host HOST] [--port PORT]\n";
    return 64;
}

} // namespace

int main(int argc, char** argv)
{
    NodeEndpoint endpoint{
        std::getenv("XDNA_QUBIC_NODE_HOST") == nullptr
            ? "corenet.qubic.li"
            : std::getenv("XDNA_QUBIC_NODE_HOST"),
        21841U,
    };
    if (const char* port = std::getenv("XDNA_QUBIC_NODE_PORT"); port != nullptr
        && !parse_port(port, endpoint.port)) {
        print_unavailable("configuration", "invalid_endpoint");
        return 3;
    }
    std::filesystem::path secret_path;
    if (!parse_arguments(argc, argv, secret_path, endpoint)) {
        return usage(argv[0]);
    }

    SigningSecret secret;
    std::string error;
    if (!xdna::qubic::load_signing_subseed_file(secret_path, secret, error)) {
        print_unavailable("local_identity", "local_identity_unavailable");
        std::cerr << error << '\n';
        return 3;
    }
    K12FourQCryptoProvider provider;
    PublicKey source_public_key{};
    if (!provider.derive_public_key(std::span<const Byte>(secret.bytes), source_public_key)) {
        print_unavailable("local_identity", "public_key_derivation_failed");
        return 3;
    }

    std::string stage = "system_info";
    try {
        const xdna::qubic::RuntimeConfig runtime = xdna::qubic::load_runtime_config_from_environment();
        TcpConnectionFactory factory;
        DirectNodeClient client(factory,
                                endpoint,
                                runtime.timeouts,
                                runtime.reconnect,
                                runtime.read_only_limits);
        const SystemInfo system_info = client.request_system_info();
        stage = "computors";
        const ComputorList computors = client.request_computors();
        const bool epoch_matches = computors.epoch == system_info.epoch;
        const bool keys_nonzero = all_computor_keys_nonzero(computors);

        PublicKey arbitrator_public_key{};
        if (!xdna::qubic::public_key_from_identity(kArbitratorIdentity, arbitrator_public_key)) {
            print_unavailable("computors", "arbitrator_identity_decode_failed");
            return 3;
        }
        const std::vector<Byte> computors_payload = xdna::qubic::serialize_computors_payload(computors);
        std::array<Byte, 32U> digest{};
        const bool digest_ok = provider.hash(
            std::span<const Byte>(computors_payload.data(), xdna::qubic::kComputorsSignedPayloadBytes),
            std::span<Byte>(digest));
        const bool signature_verified = digest_ok
            && provider.verify(arbitrator_public_key,
                               std::span<const Byte>(digest),
                               std::span<const Byte>(computors.signature));
        if (!epoch_matches || !keys_nonzero || !signature_verified) {
            print_unavailable("computors", !epoch_matches ? "stale_or_mismatched_epoch"
                : (!keys_nonzero ? "zero_computor_key" : "invalid_computor_signature"));
            return 3;
        }

        stage = "entity";
        const EntityInfo entity = client.request_entity(source_public_key);
        if (entity.public_key != source_public_key || entity.incoming_amount < 0 || entity.outgoing_amount < 0) {
            print_unavailable("entity", entity.public_key != source_public_key
                ? "response_key_mismatch" : "invalid_entity_amount");
            return 3;
        }
        const bool source_is_computor = xdna::qubic::contains_computor(computors, source_public_key);
        const bool source_is_funded = entity.spectrum_index >= 0 && energy_meets_threshold(entity);
        const bool authorized = source_is_computor || source_is_funded;
        const PublicKey destination = computors.public_keys.front();
        std::cout << (authorized ? "AUTHORIZED" : "NOT_AUTHORIZED") << '\n';
        print_safe_metadata(endpoint,
                            source_public_key,
                            system_info,
                            computors,
                            epoch_matches,
                            keys_nonzero,
                            signature_verified,
                            entity,
                            source_is_computor,
                            source_is_funded,
                            destination);
        return authorized ? 0 : 2;
    } catch (const xdna::qubic::ProtocolError& protocol_error) {
        print_unavailable(stage, "protocol_check_failed");
        std::cerr << protocol_error.what() << '\n';
        return 3;
    } catch (const TransportError& transport_error) {
        print_unavailable(stage, classify_transport_reason(transport_error.what()));
        std::cerr << transport_error.what() << '\n';
        return 3;
    } catch (const std::exception& exception) {
        print_unavailable(stage, "authorization_check_failed");
        std::cerr << exception.what() << '\n';
        return 3;
    }
}
