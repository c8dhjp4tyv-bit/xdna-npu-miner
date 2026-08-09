#pragma once

#include "bpp9000/task.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xdna::qubic {

using bpp9000::Byte;
using bpp9000::Digest32;
using bpp9000::MiningSeed;
using bpp9000::Nonce;
using bpp9000::PublicKey;

constexpr std::size_t kRequestResponseHeaderBytes = 8U;
constexpr std::size_t kSystemInfoPayloadBytes = 128U;
constexpr std::size_t kBroadcastPayloadBytes = 228U;
constexpr std::size_t kEncryptedSolutionBytes = 68U;
constexpr std::size_t kSignatureBytes = 64U;
constexpr std::uint32_t kMaximumFrameBytes = 0x00FFFFFFU;

constexpr std::uint8_t kBroadcastMessage = 1U;
constexpr std::uint8_t kRequestSystemInfo = 46U;
constexpr std::uint8_t kRespondSystemInfo = 47U;
constexpr std::uint8_t kSolutionMessage = 0U;

enum class ProtocolErrorCode : std::uint8_t {
    Truncated,
    InvalidFrameSize,
    FrameTooLarge,
    WrongMessageType,
    InvalidPayloadSize,
    InvalidSystemInfo,
    UnsupportedAlgorithm,
    InvalidTask,
    InvalidSeed,
    InvalidThreshold,
    StaleContext,
    InvalidNonce,
    InvalidScore,
    ScoreMismatch,
    MissingSigningMaterial,
    CryptoUnavailable,
    InvalidGammaNonce,
    InvalidEndpoint,
};

class ProtocolError final : public std::runtime_error {
public:
    ProtocolError(ProtocolErrorCode code, std::string message)
        : std::runtime_error(std::move(message)), code_(code)
    {
    }

    [[nodiscard]] ProtocolErrorCode code() const noexcept
    {
        return code_;
    }

private:
    ProtocolErrorCode code_;
};

struct Frame {
    std::uint8_t type = 0U;
    std::uint32_t dejavu = 0U;
    std::vector<Byte> payload;

    friend bool operator==(const Frame&, const Frame&) = default;
};

[[nodiscard]] std::vector<Byte> serialize_frame(std::uint8_t type,
                                                 std::uint32_t dejavu,
                                                 std::span<const Byte> payload);
[[nodiscard]] Frame parse_frame(std::span<const Byte> encoded);

// Incremental decoder used by the TCP boundary. It never allocates based on
// an unbounded peer-controlled length and can consume arbitrary short reads.
class FrameDecoder {
public:
    void feed(std::span<const Byte> bytes);
    [[nodiscard]] std::optional<Frame> next();
    [[nodiscard]] bool empty() const noexcept
    {
        return bytes_.empty();
    }

private:
    std::vector<Byte> bytes_;
};

[[nodiscard]] std::vector<Byte> make_system_info_request(std::uint32_t dejavu = 0U);

struct SystemInfo {
    std::int16_t version = 0;
    std::uint16_t epoch = 0U;
    std::uint32_t tick = 0U;
    std::uint32_t initial_tick = 0U;
    std::uint32_t latest_created_tick = 0U;
    std::uint16_t initial_millisecond = 0U;
    std::uint8_t initial_second = 0U;
    std::uint8_t initial_minute = 0U;
    std::uint8_t initial_hour = 0U;
    std::uint8_t initial_day = 0U;
    std::uint8_t initial_month = 0U;
    std::uint8_t initial_year = 0U;
    std::uint32_t number_of_entities = 0U;
    std::uint32_t number_of_transactions = 0U;
    MiningSeed mining_seed{};
    std::int32_t solution_threshold = 0;
    std::uint64_t total_spectrum_amount = 0U;
    std::uint64_t current_entity_balance_dust_threshold = 0U;
    std::uint32_t target_tick_vote_signature = 0U;
    std::uint64_t computor_packet_signature = 0U;
    std::uint64_t solution_additional_threshold = 0U;
    std::uint64_t reserve2 = 0U;
    std::uint64_t reserve3 = 0U;
    std::uint64_t reserve4 = 0U;

    friend bool operator==(const SystemInfo&, const SystemInfo&) = default;
};

[[nodiscard]] SystemInfo parse_system_info(const Frame& frame);
[[nodiscard]] std::vector<Byte> serialize_system_info_payload(const SystemInfo& info);

enum class Algorithm : std::uint8_t {
    Bpp9000 = 1U,
};

[[nodiscard]] bool is_supported_algorithm(Algorithm algorithm) noexcept;

struct TaskIdentity {
    bpp9000::TaskShape shape{};
    Digest32 topology_hash{};
    Digest32 data_hash{};

    friend bool operator==(const TaskIdentity&, const TaskIdentity&) = default;
};

[[nodiscard]] TaskIdentity task_identity(const bpp9000::Task& task);

struct WorkContext {
    std::uint16_t epoch = 0U;
    std::uint32_t tick = 0U;
    MiningSeed mining_seed{};
    std::uint32_t solution_threshold = 0U;
    Algorithm algorithm = Algorithm::Bpp9000;
    TaskIdentity task{};
    std::uint64_t window_width = bpp9000::kProductionWindowWidth;
    std::uint32_t max_ticks = bpp9000::kProductionMaxTicks;
    std::uint64_t number_of_windows = 0U;

    friend bool operator==(const WorkContext&, const WorkContext&) = default;
};

[[nodiscard]] WorkContext make_work_context(const SystemInfo& info,
                                            const TaskIdentity& task,
                                            Algorithm algorithm = Algorithm::Bpp9000,
                                            std::uint64_t window_width = bpp9000::kProductionWindowWidth,
                                            std::uint32_t max_ticks = bpp9000::kProductionMaxTicks);
[[nodiscard]] WorkContext make_work_context(const SystemInfo& info,
                                            const bpp9000::Task& task,
                                            Algorithm algorithm = Algorithm::Bpp9000,
                                            std::uint64_t window_width = bpp9000::kProductionWindowWidth,
                                            std::uint32_t max_ticks = bpp9000::kProductionMaxTicks);

enum class ContextUpdate : std::uint8_t {
    Installed,
    Unchanged,
    Advanced,
    RejectedOlderEpoch,
    RejectedOlderTick,
};

class WorkContextTracker {
public:
    [[nodiscard]] ContextUpdate observe(const WorkContext& incoming);
    [[nodiscard]] bool has_current() const noexcept
    {
        return current_.has_value();
    }
    [[nodiscard]] const WorkContext& current() const;
    [[nodiscard]] bool is_fresh(const WorkContext& candidate) const noexcept;

private:
    std::optional<WorkContext> current_;
};

// The secret is intentionally not printable or serializable. Its destructor
// clears the bytes so accidental temporary lifetime does not leave the key in
// an ordinary heap object longer than necessary.
struct SigningSecret {
    std::array<Byte, 32U> bytes{};

    ~SigningSecret() noexcept
    {
        std::fill(bytes.begin(), bytes.end(), static_cast<Byte>(0U));
    }

    SigningSecret() = default;
    SigningSecret(const SigningSecret&) = delete;
    SigningSecret& operator=(const SigningSecret&) = delete;
    SigningSecret(SigningSecret&& other) noexcept
        : bytes(other.bytes)
    {
        std::fill(other.bytes.begin(), other.bytes.end(), static_cast<Byte>(0U));
    }
    SigningSecret& operator=(SigningSecret&& other) noexcept
    {
        if (this != &other) {
            bytes = other.bytes;
            std::fill(other.bytes.begin(), other.bytes.end(), static_cast<Byte>(0U));
        }
        return *this;
    }
};

struct SigningMaterial {
    PublicKey public_key{};
    SigningSecret secret{};

    SigningMaterial() = default;
    SigningMaterial(const SigningMaterial&) = delete;
    SigningMaterial& operator=(const SigningMaterial&) = delete;
    SigningMaterial(SigningMaterial&&) noexcept = default;
    SigningMaterial& operator=(SigningMaterial&&) noexcept = default;
};

struct CryptoProviderInfo {
    std::string provider;
    std::string license;
    bool production_ready = false;
};

// M6 does not copy Qubic's Anti-Military crypto implementation. A production
// provider must supply the independently reviewed K12/FourQ-compatible
// implementation through this interface before live submission is enabled.
class CryptoProvider {
public:
    virtual ~CryptoProvider() = default;
    [[nodiscard]] virtual CryptoProviderInfo info() const = 0;
    [[nodiscard]] virtual bool hash(std::span<const Byte> input,
                                    std::span<Byte> output) const noexcept = 0;
    [[nodiscard]] virtual bool sign(const SigningMaterial& material,
                                    std::span<const Byte> digest,
                                    std::span<Byte> signature) const noexcept = 0;
    [[nodiscard]] virtual bool derive_shared_key(const SigningMaterial& material,
                                                 const PublicKey& peer,
                                                 std::span<Byte> shared_key) const noexcept = 0;
};

class UnavailableCryptoProvider final : public CryptoProvider {
public:
    [[nodiscard]] CryptoProviderInfo info() const override;
    [[nodiscard]] bool hash(std::span<const Byte>, std::span<Byte>) const noexcept override
    {
        return false;
    }
    [[nodiscard]] bool sign(const SigningMaterial&,
                            std::span<const Byte>,
                            std::span<Byte>) const noexcept override
    {
        return false;
    }
    [[nodiscard]] bool derive_shared_key(const SigningMaterial&,
                                         const PublicKey&,
                                         std::span<Byte>) const noexcept override
    {
        return false;
    }
};

struct CandidateSolution {
    MiningSeed mining_seed{};
    Nonce nonce{};
    std::uint32_t score = bpp9000::kTimeoutScore;
};

struct ScoreEvidence {
    bpp9000::ScoreResult cpu{};
    bpp9000::ScoreResult npu{};
};

struct SubmissionInput {
    WorkContext candidate_context{};
    TaskIdentity candidate_task{};
    Algorithm candidate_algorithm = Algorithm::Bpp9000;
    PublicKey source_public_key{};
    PublicKey destination_public_key{};
    CandidateSolution candidate{};
    ScoreEvidence evidence{};
    const SigningMaterial* signing_material = nullptr;
    std::array<Byte, 32U> gamming_nonce{};
};

enum class SubmissionRejectReason : std::uint8_t {
    None,
    UnsupportedAlgorithm,
    TaskMismatch,
    ContextMismatch,
    StaleContext,
    InvalidSeed,
    InvalidNonce,
    CpuNpuMismatch,
    Timeout,
    BadScore,
    ThresholdExceeded,
    MissingSource,
    MissingDestination,
    MissingSigningMaterial,
    CryptoUnavailable,
    InvalidGammaNonce,
    LiveSubmissionDisabled,
};

struct SubmissionDecision {
    bool authorized = false;
    SubmissionRejectReason reason = SubmissionRejectReason::None;
    std::string detail;
};

[[nodiscard]] SubmissionDecision authorize_submission(const SubmissionInput& input,
                                                      const WorkContext& current_context);

struct DirectNodeSolution {
    PublicKey source_public_key{};
    PublicKey destination_public_key{};
    std::array<Byte, 32U> gamming_nonce{};
    CandidateSolution candidate{};
    std::array<Byte, kEncryptedSolutionBytes> encrypted_payload{};
    std::array<Byte, kSignatureBytes> signature{};
    std::vector<Byte> frame;
};

[[nodiscard]] DirectNodeSolution build_solution(const SubmissionInput& input,
                                                const CryptoProvider& crypto);

struct NodeEndpoint {
    std::string host;
    std::uint16_t port = 0U;
};

struct TransportTimeouts {
    std::chrono::milliseconds connect{3000};
    std::chrono::milliseconds read{3000};
    std::chrono::milliseconds write{3000};
};

struct ReconnectPolicy {
    std::uint32_t max_attempts = 3U;
    std::chrono::milliseconds delay{25};
};

class TransportError final : public std::runtime_error {
public:
    explicit TransportError(std::string message)
        : std::runtime_error(std::move(message))
    {
    }
};

class ByteStream {
public:
    virtual ~ByteStream() = default;
    [[nodiscard]] virtual std::size_t read_some(std::span<Byte> destination) = 0;
    [[nodiscard]] virtual std::size_t write_some(std::span<const Byte> source) = 0;
    virtual void close() noexcept = 0;
};

class ConnectionFactory {
public:
    virtual ~ConnectionFactory() = default;
    [[nodiscard]] virtual std::unique_ptr<ByteStream> connect(const NodeEndpoint& endpoint,
                                                               const TransportTimeouts& timeouts) = 0;
};

class TcpConnectionFactory final : public ConnectionFactory {
public:
    [[nodiscard]] std::unique_ptr<ByteStream> connect(const NodeEndpoint& endpoint,
                                                       const TransportTimeouts& timeouts) override;
};

class FramedConnection {
public:
    explicit FramedConnection(ByteStream& stream)
        : stream_(stream)
    {
    }

    [[nodiscard]] Frame read_frame();
    void write_frame(std::span<const Byte> frame);

private:
    ByteStream& stream_;
};

class DirectNodeClient {
public:
    DirectNodeClient(ConnectionFactory& factory,
                     NodeEndpoint endpoint,
                     TransportTimeouts timeouts = {},
                     ReconnectPolicy reconnect = {})
        : factory_(factory),
          endpoint_(std::move(endpoint)),
          timeouts_(timeouts),
          reconnect_(reconnect)
    {
    }

    [[nodiscard]] SystemInfo request_system_info();
    [[nodiscard]] bool submit_frame(std::span<const Byte> frame);
    [[nodiscard]] const NodeEndpoint& endpoint() const noexcept
    {
        return endpoint_;
    }

private:
    ConnectionFactory& factory_;
    NodeEndpoint endpoint_;
    TransportTimeouts timeouts_;
    ReconnectPolicy reconnect_;
};

struct AdapterSubmitResult {
    bool sent = false;
    SubmissionDecision decision{};
    std::string transport_error;
};

class DirectNodeAdapter {
public:
    DirectNodeAdapter(DirectNodeClient& client,
                      const CryptoProvider& crypto,
                      bool allow_live_submission = false)
        : client_(client), crypto_(crypto), allow_live_submission_(allow_live_submission)
    {
    }

    [[nodiscard]] AdapterSubmitResult submit(const SubmissionInput& input,
                                             const WorkContext& current_context);

private:
    DirectNodeClient& client_;
    const CryptoProvider& crypto_;
    bool allow_live_submission_ = false;
};

struct RuntimeConfig {
    NodeEndpoint endpoint{"127.0.0.1", 21850U};
    TransportTimeouts timeouts{};
    ReconnectPolicy reconnect{};
    std::optional<SigningMaterial> signing_material;
    bool allow_live_submission = false;

    [[nodiscard]] std::string redacted_summary() const;
};

[[nodiscard]] RuntimeConfig load_runtime_config_from_environment(
    std::string_view prefix = "XDNA_QUBIC_");

} // namespace xdna::qubic
