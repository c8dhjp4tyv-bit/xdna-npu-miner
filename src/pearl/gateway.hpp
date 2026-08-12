#pragma once

#include "pearl/reference.hpp"

#include <chrono>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace xdna::pearl {

enum class GatewayErrorCode : std::uint8_t {
    Transport,
    Timeout,
    Protocol,
    MalformedResponse,
    StaleJob,
    InvalidCandidate,
    UnsupportedCertificateVersion,
    GatewayRejected,
    ProverFailure,
    NodeRejected,
    Shutdown,
};

[[nodiscard]] const char* gateway_error_code_name(GatewayErrorCode code) noexcept;

class GatewayError final : public std::runtime_error {
public:
    GatewayError(GatewayErrorCode code, const std::string& message);

    [[nodiscard]] GatewayErrorCode code() const noexcept
    {
        return code_;
    }

private:
    GatewayErrorCode code_;
};

enum class GatewayTransport : std::uint8_t {
    Unix,
    LoopbackTcp,
};

struct GatewayEndpoint {
    GatewayTransport transport = GatewayTransport::Unix;
    std::string unix_path = "/tmp/pearlgw.sock";
    std::string host = "127.0.0.1";
    std::uint16_t port = 8337U;
};

struct GatewayClientConfig {
    GatewayEndpoint endpoint{};
    std::chrono::milliseconds timeout{2000};
    std::size_t max_message_bytes = 1U << 20U;
};

struct MiningJob {
    std::vector<std::uint8_t> incomplete_header_bytes;
    Digest target{};
    std::string target_decimal;
    CertificateVersion certificate_version = CertificateVersion::V1;
    std::string job_id;

    [[nodiscard]] std::string target_for_json() const
    {
        return target_decimal;
    }
};

// This is the immutable identity bound to every locally constructed
// candidate.  A job-id alone is insufficient because official template
// refreshes may change header, target, or certificate version independently.
struct MiningJobIdentity {
    std::string job_id;
    std::vector<std::uint8_t> incomplete_header_bytes;
    Digest target{};
    CertificateVersion certificate_version = CertificateVersion::V1;

    friend bool operator==(const MiningJobIdentity&, const MiningJobIdentity&) = default;
};

[[nodiscard]] MiningJobIdentity mining_job_identity(const MiningJob& job);
[[nodiscard]] bool same_mining_job_identity(const MiningJob& left,
                                             const MiningJob& right) noexcept;

struct SubmissionResult {
    bool accepted_by_gateway = false;
    std::string detail;
};

class GatewayClient final {
public:
    explicit GatewayClient(GatewayClientConfig config = {});

    [[nodiscard]] const GatewayClientConfig& config() const noexcept
    {
        return config_;
    }

    [[nodiscard]] MiningJob get_mining_info();
    [[nodiscard]] SubmissionResult submit_plain_proof(const PlainProof& proof,
                                                       const MiningJob& job);
    [[nodiscard]] SubmissionResult submit_official_plain_proof(
        std::span<const std::uint8_t> official_wire,
        const MiningJob& job);

    // A health check intentionally uses the same bounded getMiningInfo path so
    // it verifies both transport and protocol framing.
    void health_check();

private:
    GatewayClientConfig config_;
    std::uint64_t request_id_ = 0U;
};

[[nodiscard]] std::vector<std::uint8_t> base64_decode_strict(std::string_view value);
[[nodiscard]] std::string base64_encode(std::span<const std::uint8_t> bytes);

} // namespace xdna::pearl
