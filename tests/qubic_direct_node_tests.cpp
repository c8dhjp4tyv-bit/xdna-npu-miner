#include "qubic/direct_node.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using xdna::bpp9000::Byte;
using xdna::bpp9000::ScoreResult;
using xdna::bpp9000::ScoreStatus;
using xdna::qubic::Algorithm;
using xdna::qubic::ByteStream;
using xdna::qubic::CandidateSolution;
using xdna::qubic::ConnectionFactory;
using xdna::qubic::CryptoProvider;
using xdna::qubic::CryptoProviderInfo;
using xdna::qubic::DirectNodeAdapter;
using xdna::qubic::DirectNodeClient;
using xdna::qubic::Frame;
using xdna::qubic::FrameDecoder;
using xdna::qubic::NodeEndpoint;
using xdna::qubic::ProtocolError;
using xdna::qubic::ProtocolErrorCode;
using xdna::qubic::ReconnectPolicy;
using xdna::qubic::RuntimeConfig;
using xdna::qubic::SigningMaterial;
using xdna::qubic::SubmissionInput;
using xdna::qubic::SubmissionRejectReason;
using xdna::qubic::SystemInfo;
using xdna::qubic::TaskIdentity;
using xdna::qubic::TransportError;
using xdna::qubic::TransportTimeouts;
using xdna::qubic::UnavailableCryptoProvider;
using xdna::qubic::WorkContext;
using xdna::qubic::WorkContextTracker;

void expect(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Function>
void expect_protocol(Function&& function, ProtocolErrorCode code, const char* message)
{
    try {
        function();
    } catch (const ProtocolError& error) {
        expect(error.code() == code, message);
        return;
    }
    throw std::runtime_error(message);
}

template <typename Function>
void expect_transport(Function&& function, const char* message)
{
    try {
        function();
    } catch (const TransportError&) {
        return;
    }
    throw std::runtime_error(message);
}

TaskIdentity make_task_identity(std::uint64_t sequence_length = 10U)
{
    TaskIdentity identity;
    identity.shape.input_trits = 18U;
    identity.shape.output_trits = 1U;
    identity.shape.sequence_length = sequence_length;
    identity.shape.population = 64U;
    identity.shape.neighbors = 3U;
    for (std::size_t index = 0U; index < identity.topology_hash.bytes.size(); ++index) {
        identity.topology_hash.bytes[index] = static_cast<Byte>(index + 1U);
        identity.data_hash.bytes[index] = static_cast<Byte>(0xA0U + index);
    }
    return identity;
}

SystemInfo make_system_info()
{
    SystemInfo info;
    info.version = 301;
    info.epoch = 7U;
    info.tick = 100U;
    info.initial_tick = 1U;
    info.latest_created_tick = 99U;
    info.initial_millisecond = 123U;
    info.initial_second = 2U;
    info.initial_minute = 3U;
    info.initial_hour = 4U;
    info.initial_day = 5U;
    info.initial_month = 6U;
    info.initial_year = 26U;
    info.number_of_entities = 11U;
    info.number_of_transactions = 12U;
    for (std::size_t index = 0U; index < info.mining_seed.bytes.size(); ++index) {
        info.mining_seed.bytes[index] = static_cast<Byte>(0x40U + index);
    }
    info.solution_threshold = 2;
    info.total_spectrum_amount = 13U;
    info.current_entity_balance_dust_threshold = 14U;
    info.target_tick_vote_signature = 15U;
    info.computor_packet_signature = 16U;
    info.solution_additional_threshold = 17U;
    info.reserve2 = 18U;
    info.reserve3 = 19U;
    info.reserve4 = 20U;
    return info;
}

WorkContext make_context()
{
    return xdna::qubic::make_work_context(
        make_system_info(), make_task_identity(), Algorithm::Bpp9000, 4U, 8U);
}

SigningMaterial make_signing_material(const xdna::qubic::PublicKey& public_key)
{
    SigningMaterial material;
    material.public_key = public_key;
    for (std::size_t index = 0U; index < material.secret.bytes.size(); ++index) {
        material.secret.bytes[index] = static_cast<Byte>(0xF0U - index);
    }
    return material;
}

class TestCryptoProvider final : public CryptoProvider {
public:
    [[nodiscard]] CryptoProviderInfo info() const override
    {
        return CryptoProviderInfo{"test-only-deterministic", "test fixture", false};
    }

    [[nodiscard]] bool hash(std::span<const Byte> input,
                            std::span<Byte> output) const noexcept override
    {
        for (std::size_t index = 0U; index < output.size(); ++index) {
            const Byte source = input.empty() ? static_cast<Byte>(0U) : input[index % input.size()];
            output[index] = static_cast<Byte>(source ^ static_cast<Byte>((index * 29U + input.size()) & 0xFFU));
        }
        if (input.size() == 64U && output.size() == 32U) {
            output[0U] = 0U;
        }
        return true;
    }

    [[nodiscard]] bool sign(const SigningMaterial& material,
                            std::span<const Byte> digest,
                            std::span<Byte> signature) const noexcept override
    {
        for (std::size_t index = 0U; index < signature.size(); ++index) {
            const Byte digest_byte = digest.empty() ? static_cast<Byte>(0U) : digest[index % digest.size()];
            signature[index] = static_cast<Byte>(digest_byte ^ material.secret.bytes[index % material.secret.bytes.size()]);
        }
        return true;
    }

    [[nodiscard]] bool derive_shared_key(const SigningMaterial&,
                                         const xdna::qubic::PublicKey&,
                                         std::span<Byte> shared_key) const noexcept override
    {
        std::fill(shared_key.begin(), shared_key.end(), static_cast<Byte>(0U));
        return true;
    }
};

struct MemoryStream final : ByteStream {
    MemoryStream(std::vector<Byte> incoming_bytes,
                 std::vector<Byte>& written_bytes,
                 std::size_t read_chunk_bytes = 1U,
                 std::size_t write_chunk_bytes = 2U,
                 std::chrono::milliseconds empty_read_delay = std::chrono::milliseconds(0),
                 bool timeout_on_empty_read = false)
        : incoming(std::move(incoming_bytes)),
          written(written_bytes),
          read_chunk(read_chunk_bytes),
          write_chunk(write_chunk_bytes),
          empty_delay(empty_read_delay),
          timeout_on_empty(timeout_on_empty_read)
    {
    }

    void set_read_timeout(std::chrono::milliseconds timeout) override
    {
        last_read_timeout = timeout;
    }

    [[nodiscard]] std::size_t read_some(std::span<Byte> destination) override
    {
        if (!response_dejavu_prepared) {
            FrameDecoder decoder;
            decoder.feed(written);
            (void)decoder.next();
            const auto request = decoder.next();
            if (request.has_value()) {
                for (std::size_t offset = 0U; offset + xdna::qubic::kRequestResponseHeaderBytes <= incoming.size();) {
                    const std::size_t size = static_cast<std::size_t>(incoming[offset])
                        | (static_cast<std::size_t>(incoming[offset + 1U]) << 8U)
                        | (static_cast<std::size_t>(incoming[offset + 2U]) << 16U);
                    if (size < xdna::qubic::kRequestResponseHeaderBytes || size > incoming.size() - offset) {
                        break;
                    }
                    if (incoming[offset + 3U] == xdna::qubic::kRespondSystemInfo
                        || incoming[offset + 3U] == xdna::qubic::kBroadcastComputors
                        || incoming[offset + 3U] == xdna::qubic::kRespondEntity) {
                        for (std::size_t byte = 0U; byte < 4U; ++byte) {
                            incoming[offset + 4U + byte] = static_cast<Byte>(
                                (request->dejavu >> (byte * 8U)) & 0xFFU);
                        }
                    }
                    offset += size;
                }
            }
            response_dejavu_prepared = true;
        }
        if (read_offset == incoming.size()) {
            if (empty_delay.count() > 0) {
                std::this_thread::sleep_for(empty_delay);
            }
            if (timeout_on_empty) {
                throw TransportError("injected read timeout");
            }
            return 0U;
        }
        const std::size_t count = std::min({destination.size(), read_chunk, incoming.size() - read_offset});
        std::copy_n(incoming.begin() + static_cast<std::ptrdiff_t>(read_offset),
                    count,
                    destination.begin());
        read_offset += count;
        return count;
    }

    [[nodiscard]] std::size_t write_some(std::span<const Byte> source) override
    {
        const std::size_t count = std::min(source.size(), write_chunk);
        written.insert(written.end(), source.begin(), source.begin() + static_cast<std::ptrdiff_t>(count));
        return count;
    }

    void close() noexcept override
    {
        closed = true;
    }

    std::vector<Byte> incoming;
    std::vector<Byte>& written;
    std::size_t read_chunk = 1U;
    std::size_t write_chunk = 2U;
    std::size_t read_offset = 0U;
    std::chrono::milliseconds last_read_timeout{0};
    std::chrono::milliseconds empty_delay{0};
    bool timeout_on_empty = false;
    bool response_dejavu_prepared = false;
    bool closed = false;
};

class MemoryFactory final : public ConnectionFactory {
public:
    explicit MemoryFactory(std::vector<Byte> response)
        : response_(std::move(response))
    {
    }

    [[nodiscard]] std::unique_ptr<ByteStream> connect(const NodeEndpoint&,
                                                       const TransportTimeouts&) override
    {
        ++connects;
        if (failures_before_success > 0U) {
            --failures_before_success;
            throw TransportError("injected connection failure");
        }
        writes.emplace_back();
        return std::make_unique<MemoryStream>(response_, writes.back(), read_chunk, write_chunk,
                                              empty_delay, timeout_on_empty);
    }

    std::vector<Byte> response_;
    std::vector<std::vector<Byte>> writes;
    std::size_t read_chunk = 1U;
    std::size_t write_chunk = 2U;
    std::chrono::milliseconds empty_delay{0};
    bool timeout_on_empty = false;
    std::size_t failures_before_success = 0U;
    std::size_t connects = 0U;
};

SubmissionInput make_submission(const WorkContext& context)
{
    SubmissionInput input;
    input.candidate_context = context;
    input.candidate_task = context.task;
    input.candidate_algorithm = Algorithm::Bpp9000;
    input.source_public_key.bytes.fill(0x11U);
    input.destination_public_key.bytes.fill(0x22U);
    input.candidate.mining_seed = context.mining_seed;
    input.candidate.nonce.bytes.fill(0x33U);
    input.candidate.nonce.bytes[0U] = 1U;
    input.candidate.nonce.bytes[1U] = 1U;
    input.candidate.nonce.bytes[2U] = 0U;
    input.candidate.score = 2U;
    input.evidence.cpu = ScoreResult{2U, ScoreStatus::Settled, 6U, 42U};
    input.evidence.npu = input.evidence.cpu;
    input.gamming_nonce.fill(0x44U);
    return input;
}

void test_frame_and_incremental_decoder()
{
    const std::array<Byte, 3U> payload{1U, 2U, 3U};
    const std::vector<Byte> encoded = xdna::qubic::serialize_frame(99U, 0xAABBCCDDU, payload);
    expect(encoded.size() == 11U && encoded[0U] == 11U, "frame uses a three-byte little-endian size");
    const Frame parsed = xdna::qubic::parse_frame(encoded);
    expect(parsed.type == 99U && parsed.dejavu == 0xAABBCCDDU && parsed.payload == std::vector<Byte>(payload.begin(), payload.end()),
           "frame parser preserves header and payload");

    FrameDecoder decoder;
    for (const Byte value : encoded) {
        decoder.feed(std::span<const Byte>(&value, 1U));
        expect(!decoder.next().has_value() || value == encoded.back(), "partial frame does not produce a false result");
    }
    decoder.feed(encoded);
    const auto decoded = decoder.next();
    expect(decoded.has_value() && *decoded == parsed, "incremental decoder returns the exact frame");
    expect(!decoder.next().has_value(), "incremental decoder is empty after consuming a frame");

    std::vector<Byte> malformed(8U, 0U);
    malformed[0U] = 7U;
    expect_protocol([&] { (void)xdna::qubic::parse_frame(malformed); },
                    ProtocolErrorCode::InvalidFrameSize,
                    "parser rejects a frame smaller than its header");
}

void test_system_info_and_context()
{
    const SystemInfo expected = make_system_info();
    const std::vector<Byte> payload = xdna::qubic::serialize_system_info_payload(expected);
    const Frame frame{xdna::qubic::kRespondSystemInfo, 0U, payload};
    const SystemInfo actual = xdna::qubic::parse_system_info(frame);
    expect(actual == expected, "system-info parser round trips every packed field");
    const WorkContext context = xdna::qubic::make_work_context(
        actual, make_task_identity(), Algorithm::Bpp9000, 4U, 8U);
    expect(context.epoch == expected.epoch && context.tick == expected.tick
               && context.number_of_windows == 6U && context.solution_threshold == 2U,
           "work context extracts epoch, tick, seed, threshold and task shape");

    SystemInfo negative = expected;
    negative.solution_threshold = -1;
    expect_protocol([&] {
        (void)xdna::qubic::make_work_context(negative, make_task_identity());
    }, ProtocolErrorCode::InvalidThreshold, "negative system threshold is rejected");

    Frame wrong_type{xdna::qubic::kRequestSystemInfo, 0U, payload};
    expect_protocol([&] { (void)xdna::qubic::parse_system_info(wrong_type); },
                    ProtocolErrorCode::WrongMessageType,
                    "system-info parser rejects the request message type");
}

void test_context_freshness()
{
    const WorkContext first = make_context();
    WorkContextTracker tracker;
    expect(tracker.observe(first) == xdna::qubic::ContextUpdate::Installed, "tracker installs first context");
    expect(tracker.is_fresh(first), "installed context is fresh");

    WorkContext older = first;
    older.tick = first.tick - 1U;
    expect(tracker.observe(older) == xdna::qubic::ContextUpdate::RejectedOlderTick,
           "tracker rejects an older same-seed tick");

    WorkContext newer = first;
    newer.tick = first.tick + 1U;
    expect(tracker.observe(newer) == xdna::qubic::ContextUpdate::Advanced, "tracker accepts a newer tick");
    expect(tracker.is_fresh(first), "same-seed work remains fresh while the tick advances");

    WorkContext new_seed = newer;
    new_seed.mining_seed.bytes[0U] ^= 0xFFU;
    expect(tracker.observe(new_seed) == xdna::qubic::ContextUpdate::Advanced,
           "tracker installs a same-epoch seed change");
    expect(!tracker.is_fresh(first), "old-seed work is stale after a seed change");
}

void test_submission_gates_and_no_send()
{
    const WorkContext context = make_context();
    SubmissionInput valid = make_submission(context);
    SigningMaterial material = make_signing_material(valid.source_public_key);
    valid.signing_material = &material;

    const auto authorized = xdna::qubic::authorize_submission(valid, context);
    expect(authorized.authorized, "exact CPU/NPU finite threshold result is authorized");

    auto check_rejected = [&](SubmissionInput candidate,
                              SubmissionRejectReason reason,
                              const char* message) {
        const auto decision = xdna::qubic::authorize_submission(candidate, context);
        expect(!decision.authorized && decision.reason == reason, message);
        MemoryFactory factory({});
        DirectNodeClient client(factory, NodeEndpoint{"mock", 1234U}, {}, ReconnectPolicy{1U, std::chrono::milliseconds(0)});
        TestCryptoProvider crypto;
        DirectNodeAdapter adapter(client, crypto);
        const auto result = adapter.submit(candidate, context);
        expect(!result.sent && factory.connects == 0U, "rejected work sends zero network messages");
    };

    SubmissionInput mismatch = valid;
    mismatch.evidence.npu.score = 1U;
    check_rejected(mismatch, SubmissionRejectReason::CpuNpuMismatch, "CPU/NPU mismatch is rejected");

    SubmissionInput stale = valid;
    stale.candidate.mining_seed.bytes[0U] ^= 0x01U;
    check_rejected(stale, SubmissionRejectReason::InvalidSeed, "stale seed is rejected");

    SubmissionInput bad_nonce = valid;
    bad_nonce.candidate.nonce.bytes[0U] = 0U;
    check_rejected(bad_nonce, SubmissionRejectReason::InvalidNonce, "invalid canonical nonce is rejected");

    SubmissionInput bad_algorithm = valid;
    bad_algorithm.candidate_algorithm = static_cast<Algorithm>(99U);
    check_rejected(bad_algorithm, SubmissionRejectReason::UnsupportedAlgorithm, "unsupported algorithm is rejected");

    SubmissionInput bad_task = valid;
    bad_task.candidate_task.data_hash.bytes[0U] ^= 0x01U;
    check_rejected(bad_task, SubmissionRejectReason::TaskMismatch, "task mismatch is rejected");

    SubmissionInput bad_threshold = valid;
    bad_threshold.candidate.score = 3U;
    bad_threshold.evidence.cpu.score = 3U;
    bad_threshold.evidence.npu.score = 3U;
    check_rejected(bad_threshold, SubmissionRejectReason::ThresholdExceeded, "threshold violation is rejected");

    SubmissionInput timeout = valid;
    timeout.candidate.score = xdna::bpp9000::kTimeoutScore;
    timeout.evidence.cpu = ScoreResult{xdna::bpp9000::kTimeoutScore, ScoreStatus::Timeout, 6U, 8U};
    timeout.evidence.npu = timeout.evidence.cpu;
    check_rejected(timeout, SubmissionRejectReason::Timeout, "timeout sentinel is rejected");

    SubmissionInput malformed_context = valid;
    malformed_context.candidate_context.solution_threshold = 1U;
    check_rejected(malformed_context, SubmissionRejectReason::ContextMismatch, "malformed context is rejected");
}

void test_deterministic_solution_serialization()
{
    const WorkContext context = make_context();
    SubmissionInput input = make_submission(context);
    SigningMaterial material = make_signing_material(input.source_public_key);
    input.signing_material = &material;
    TestCryptoProvider crypto;
    const auto first = xdna::qubic::build_solution(input, crypto);
    const auto second = xdna::qubic::build_solution(input, crypto);
    expect(first.frame == second.frame, "solution serialization is deterministic for fixed inputs");
    expect(first.frame.size() == xdna::qubic::kRequestResponseHeaderBytes + xdna::qubic::kBroadcastPayloadBytes,
           "solution frame has the exact direct-node wire size");
    const Frame frame = xdna::qubic::parse_frame(first.frame);
    expect(frame.type == xdna::qubic::kBroadcastMessage
               && frame.payload.size() == xdna::qubic::kBroadcastPayloadBytes,
           "solution uses BROADCAST_MESSAGE and the expected payload");
    expect(frame.payload[96U + 68U] == first.signature[0U],
           "signature follows source, destination, nonce and encrypted solution payload");

    UnavailableCryptoProvider unavailable;
    expect_protocol([&] { (void)xdna::qubic::build_solution(input, unavailable); },
                    ProtocolErrorCode::CryptoUnavailable,
                    "live solution construction fails closed without a provider");
}

void test_mock_system_info_and_bounded_reconnect()
{
    const SystemInfo expected = make_system_info();
    const std::vector<Byte> system_info = xdna::qubic::serialize_frame(
        xdna::qubic::kRespondSystemInfo,
        0U,
        xdna::qubic::serialize_system_info_payload(expected));
    const std::array<Byte, xdna::qubic::kExchangePublicPeersPayloadBytes> peer_payload{};
    const std::array<Byte, 0U> broadcast_payload{};
    std::vector<Byte> response = xdna::qubic::serialize_frame(
        xdna::qubic::kExchangePublicPeers,
        0U,
        peer_payload);
    const std::vector<Byte> broadcast = xdna::qubic::serialize_frame(
        xdna::qubic::kBroadcastMessage,
        0U,
        broadcast_payload);
    const std::vector<Byte> unsolicited_request = xdna::qubic::serialize_frame(
        14U,
        0U,
        {});
    response.insert(response.end(), broadcast.begin(), broadcast.end());
    response.insert(response.end(), unsolicited_request.begin(), unsolicited_request.end());
    response.insert(response.end(), system_info.begin(), system_info.end());
    MemoryFactory factory(response);
    factory.failures_before_success = 2U;
    DirectNodeClient client(factory,
                            NodeEndpoint{"mock", 1234U},
                            TransportTimeouts{},
                            ReconnectPolicy{3U, std::chrono::milliseconds(0)});
    const SystemInfo actual = client.request_system_info();
    expect(actual == expected && factory.connects == 3U, "system-info request uses bounded reconnect");
    expect(factory.writes.size() == 1U, "only the successful connection writes a request");
    FrameDecoder decoder;
    decoder.feed(factory.writes[0U]);
    const auto handshake = decoder.next();
    const auto request = decoder.next();
    expect(handshake.has_value() && handshake->type == xdna::qubic::kExchangePublicPeers
               && handshake->payload.size() == xdna::qubic::kExchangePublicPeersPayloadBytes,
           "mock integration sends the direct-node peer-exchange handshake");
    expect(request.has_value() && request->type == xdna::qubic::kRequestSystemInfo
               && request->payload.empty() && request->dejavu != 0U,
           "mock integration sends the exact empty system-info request with a nonzero dejavu");
    expect(!decoder.next().has_value(), "mock system-info request contains only handshake and request frames");
}

void test_read_only_demultiplexing_allows_many_unsolicited_frames()
{
    const SystemInfo expected = make_system_info();
    std::vector<Byte> response;
    // This deliberately exceeds the former 64-frame limit. These are known
    // asynchronous BROADCAST_TICK frames, followed by the requested response.
    for (std::uint32_t index = 0U; index < 200U; ++index) {
        const std::array<Byte, 4U> tick_payload{
            static_cast<Byte>(index & 0xFFU), 0U, 0U, 0U};
        const std::vector<Byte> unsolicited = xdna::qubic::serialize_frame(
            3U, index + 1U, tick_payload);
        response.insert(response.end(), unsolicited.begin(), unsolicited.end());
    }
    const std::vector<Byte> desired = xdna::qubic::serialize_frame(
        xdna::qubic::kRespondSystemInfo,
        0U,
        xdna::qubic::serialize_system_info_payload(expected));
    response.insert(response.end(), desired.begin(), desired.end());

    MemoryFactory factory(response);
    const xdna::qubic::ReadOnlyRequestLimits limits{
        std::chrono::milliseconds(1000), 64U * 1024U, 512U};
    DirectNodeClient client(factory,
                            NodeEndpoint{"mock", 1234U},
                            TransportTimeouts{},
                            ReconnectPolicy{1U, std::chrono::milliseconds(0)},
                            limits);
    expect(client.request_system_info() == expected,
           "requested response succeeds after more than 64 valid unsolicited frames");
}

void test_read_only_demultiplexing_resource_and_deadline_limits()
{
    const std::array<Byte, 16U> payload{};
    const std::vector<Byte> unsolicited = xdna::qubic::serialize_frame(3U, 1U, payload);

    MemoryFactory byte_factory(unsolicited);
    DirectNodeClient byte_client(
        byte_factory, NodeEndpoint{"mock", 1234U}, TransportTimeouts{},
        ReconnectPolicy{1U, std::chrono::milliseconds(0)},
        xdna::qubic::ReadOnlyRequestLimits{std::chrono::milliseconds(1000), 16U, 512U});
    expect_transport([&] { (void)byte_client.request_system_info(); },
                     "ignored-byte ceiling fails closed before accepting an absent response");

    std::vector<Byte> many_frames;
    for (std::uint32_t index = 0U; index < 3U; ++index) {
        const std::vector<Byte> frame = xdna::qubic::serialize_frame(3U, index + 1U, {});
        many_frames.insert(many_frames.end(), frame.begin(), frame.end());
    }
    MemoryFactory frame_factory(many_frames);
    DirectNodeClient frame_client(
        frame_factory, NodeEndpoint{"mock", 1234U}, TransportTimeouts{},
        ReconnectPolicy{1U, std::chrono::milliseconds(0)},
        xdna::qubic::ReadOnlyRequestLimits{std::chrono::milliseconds(1000), 1024U, 2U});
    expect_transport([&] { (void)frame_client.request_system_info(); },
                     "high defensive frame ceiling remains a fail-closed DoS guard");

    MemoryFactory timeout_factory({});
    timeout_factory.empty_delay = std::chrono::milliseconds(20);
    timeout_factory.timeout_on_empty = true;
    DirectNodeClient timeout_client(
        timeout_factory, NodeEndpoint{"mock", 1234U}, TransportTimeouts{},
        ReconnectPolicy{1U, std::chrono::milliseconds(0)},
        xdna::qubic::ReadOnlyRequestLimits{std::chrono::milliseconds(10), 1024U, 32U});
    expect_transport([&] { (void)timeout_client.request_system_info(); },
                     "absolute request deadline fails closed after a read timeout");
}

void test_mock_system_info_failure_modes()
{
    const std::vector<Byte> wrong_type = xdna::qubic::serialize_frame(
        99U, 0U, {});
    MemoryFactory wrong_factory(wrong_type);
    DirectNodeClient wrong_client(wrong_factory,
                                  NodeEndpoint{"mock", 1234U},
                                  TransportTimeouts{},
                                  ReconnectPolicy{1U, std::chrono::milliseconds(0)});
    expect_protocol([&] { (void)wrong_client.request_system_info(); },
                    ProtocolErrorCode::WrongMessageType,
                    "live probe path rejects a wrong response frame type");

    const std::vector<Byte> truncated{
        136U, 0U, 0U, xdna::qubic::kRespondSystemInfo, 0U, 0U, 0U, 0U, 1U,
    };
    MemoryFactory truncated_factory(truncated);
    DirectNodeClient truncated_client(truncated_factory,
                                      NodeEndpoint{"mock", 1234U},
                                      TransportTimeouts{},
                                      ReconnectPolicy{1U, std::chrono::milliseconds(0)});
    expect_transport([&] { (void)truncated_client.request_system_info(); },
                     "live probe path rejects a truncated response payload");

    MemoryFactory timeout_factory({});
    DirectNodeClient timeout_client(timeout_factory,
                                    NodeEndpoint{"mock", 1234U},
                                    TransportTimeouts{},
                                    ReconnectPolicy{1U, std::chrono::milliseconds(0)});
    expect_transport([&] { (void)timeout_client.request_system_info(); },
                     "live probe path fails closed when the response stream ends");
}

void test_current_computor_and_entity_wire_parsers()
{
    xdna::qubic::ComputorList expected_computors;
    expected_computors.epoch = 42U;
    for (std::size_t index = 0U; index < expected_computors.public_keys.size(); ++index) {
        expected_computors.public_keys[index].bytes.fill(static_cast<Byte>(index + 1U));
    }
    expected_computors.signature.fill(0xA5U);
    const std::vector<Byte> computor_payload = xdna::qubic::serialize_computors_payload(expected_computors);
    const Frame computor_frame{xdna::qubic::kBroadcastComputors, 9U, computor_payload};
    expect(xdna::qubic::parse_computors(computor_frame) == expected_computors,
           "current computor parser round trips the exact signed payload");
    expect(xdna::qubic::contains_computor(expected_computors, expected_computors.public_keys[17U]),
           "current computor membership lookup finds a public key");
    const Frame computor_request = xdna::qubic::parse_frame(xdna::qubic::make_computors_request(7U));
    expect(computor_request.type == xdna::qubic::kRequestComputors
               && computor_request.payload.empty() && computor_request.dejavu == 7U,
           "current computor request has the exact empty payload");

    auto write_u32 = [](std::vector<Byte>& bytes, std::size_t offset, std::uint32_t value) {
        bytes[offset] = static_cast<Byte>(value & 0xFFU);
        bytes[offset + 1U] = static_cast<Byte>((value >> 8U) & 0xFFU);
        bytes[offset + 2U] = static_cast<Byte>((value >> 16U) & 0xFFU);
        bytes[offset + 3U] = static_cast<Byte>((value >> 24U) & 0xFFU);
    };
    auto write_u64 = [&write_u32](std::vector<Byte>& bytes, std::size_t offset, std::uint64_t value) {
        write_u32(bytes, offset, static_cast<std::uint32_t>(value & 0xFFFFFFFFULL));
        write_u32(bytes, offset + 4U, static_cast<std::uint32_t>(value >> 32U));
    };
    xdna::qubic::EntityInfo expected_entity;
    expected_entity.public_key.bytes.fill(0x11U);
    expected_entity.incoming_amount = 1234567890123LL;
    expected_entity.outgoing_amount = 34567890123LL;
    expected_entity.number_of_incoming_transfers = 3U;
    expected_entity.number_of_outgoing_transfers = 4U;
    expected_entity.latest_incoming_transfer_tick = 5U;
    expected_entity.latest_outgoing_transfer_tick = 6U;
    expected_entity.tick = 7U;
    expected_entity.spectrum_index = 8;
    for (std::size_t index = 0U; index < expected_entity.siblings.size(); ++index) {
        expected_entity.siblings[index].bytes.fill(static_cast<Byte>(0x20U + index));
    }
    std::vector<Byte> entity_payload(xdna::qubic::kEntityPayloadBytes, 0U);
    std::copy(expected_entity.public_key.bytes.begin(),
              expected_entity.public_key.bytes.end(),
              entity_payload.begin());
    write_u64(entity_payload, 32U, static_cast<std::uint64_t>(expected_entity.incoming_amount));
    write_u64(entity_payload, 40U, static_cast<std::uint64_t>(expected_entity.outgoing_amount));
    write_u32(entity_payload, 48U, expected_entity.number_of_incoming_transfers);
    write_u32(entity_payload, 52U, expected_entity.number_of_outgoing_transfers);
    write_u32(entity_payload, 56U, expected_entity.latest_incoming_transfer_tick);
    write_u32(entity_payload, 60U, expected_entity.latest_outgoing_transfer_tick);
    write_u32(entity_payload, 64U, expected_entity.tick);
    write_u32(entity_payload, 68U, static_cast<std::uint32_t>(expected_entity.spectrum_index));
    for (std::size_t index = 0U; index < expected_entity.siblings.size(); ++index) {
        std::copy(expected_entity.siblings[index].bytes.begin(),
                  expected_entity.siblings[index].bytes.end(),
                  entity_payload.begin() + static_cast<std::ptrdiff_t>(72U + index * 32U));
    }
    const Frame entity_frame{xdna::qubic::kRespondEntity, 10U, entity_payload};
    expect(xdna::qubic::parse_entity(entity_frame) == expected_entity,
           "current entity parser round trips the exact 840-byte response");
    const Frame entity_request = xdna::qubic::parse_frame(
        xdna::qubic::make_entity_request(expected_entity.public_key, 11U));
    expect(entity_request.type == xdna::qubic::kRequestEntity
               && entity_request.payload.size() == expected_entity.public_key.bytes.size()
               && entity_request.dejavu == 11U,
           "current entity request carries the requested public key");

    MemoryFactory computor_factory(xdna::qubic::serialize_frame(
        xdna::qubic::kBroadcastComputors, 12U, computor_payload));
    DirectNodeClient computor_client(computor_factory,
                                    NodeEndpoint{"mock", 1234U},
                                    TransportTimeouts{},
                                    ReconnectPolicy{1U, std::chrono::milliseconds(0)});
    expect(computor_client.request_computors() == expected_computors,
           "mock current computor request parses the bounded response");
    FrameDecoder computor_decoder;
    computor_decoder.feed(computor_factory.writes[0U]);
    (void)computor_decoder.next();
    const auto computor_request_frame = computor_decoder.next();
    expect(computor_request_frame.has_value()
               && computor_request_frame->type == xdna::qubic::kRequestComputors
               && computor_request_frame->payload.empty(),
           "mock current computor request sends type 11 after handshake");

    MemoryFactory entity_factory(xdna::qubic::serialize_frame(
        xdna::qubic::kRespondEntity, 13U, entity_payload));
    DirectNodeClient entity_client(entity_factory,
                                   NodeEndpoint{"mock", 1234U},
                                   TransportTimeouts{},
                                   ReconnectPolicy{1U, std::chrono::milliseconds(0)});
    expect(entity_client.request_entity(expected_entity.public_key) == expected_entity,
           "mock current entity request parses the bounded response");
    FrameDecoder entity_decoder;
    entity_decoder.feed(entity_factory.writes[0U]);
    (void)entity_decoder.next();
    const auto entity_request_frame = entity_decoder.next();
    expect(entity_request_frame.has_value()
               && entity_request_frame->type == xdna::qubic::kRequestEntity
               && entity_request_frame->payload.size() == expected_entity.public_key.bytes.size()
               && entity_request_frame->payload
                   == std::vector<Byte>(expected_entity.public_key.bytes.begin(),
                                        expected_entity.public_key.bytes.end()),
           "mock current entity request sends type 31 with the source key");
}

void test_mock_solution_submission()
{
    const WorkContext context = make_context();
    SubmissionInput input = make_submission(context);
    SigningMaterial material = make_signing_material(input.source_public_key);
    input.signing_material = &material;
    TestCryptoProvider crypto;
    MemoryFactory factory({});
    DirectNodeClient client(factory,
                            NodeEndpoint{"mock", 1234U},
                            TransportTimeouts{},
                            ReconnectPolicy{1U, std::chrono::milliseconds(0)});
    DirectNodeAdapter adapter(client, crypto, true);
    const auto result = adapter.submit(input, context);
    expect(result.sent && result.decision.authorized && factory.connects == 1U,
           "mock adapter submits only an authorized CPU-verified solution");
    FrameDecoder decoder;
    decoder.feed(factory.writes[0U]);
    const auto handshake = decoder.next();
    const auto submitted = decoder.next();
    expect(handshake.has_value() && handshake->type == xdna::qubic::kExchangePublicPeers,
           "mock adapter sends the direct-node peer-exchange handshake");
    expect(submitted.has_value() && submitted->type == xdna::qubic::kBroadcastMessage
               && submitted->payload.size() == xdna::qubic::kBroadcastPayloadBytes,
           "mock adapter writes the deterministic solution frame");
    expect(!decoder.next().has_value(), "mock adapter writes no extra frames");
}

void test_secret_safe_configuration()
{
    RuntimeConfig config;
    config.endpoint = NodeEndpoint{"example.invalid", 1234U};
    config.signing_material.emplace();
    config.signing_material->public_key.bytes.fill(0x11U);
    config.signing_material->secret.bytes.fill(0xEEU);
    const std::string summary = config.redacted_summary();
    expect(summary.find("configured") != std::string::npos, "configuration reports only signing presence");
    expect(summary.find("ee") == std::string::npos && summary.find("EE") == std::string::npos,
           "configuration summary does not expose signing secret bytes");
}

} // namespace

int main()
{
    try {
        test_frame_and_incremental_decoder();
        test_system_info_and_context();
        test_context_freshness();
        test_submission_gates_and_no_send();
        test_deterministic_solution_serialization();
        test_mock_system_info_and_bounded_reconnect();
        test_read_only_demultiplexing_allows_many_unsolicited_frames();
        test_read_only_demultiplexing_resource_and_deadline_limits();
        test_mock_system_info_failure_modes();
        test_current_computor_and_entity_wire_parsers();
        test_mock_solution_submission();
        test_secret_safe_configuration();
        std::cout << "PASS qubic_direct_node\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL qubic_direct_node: " << error.what() << '\n';
        return 1;
    }
}
