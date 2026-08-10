#include "qubic/direct_node.hpp"

#include <array>
#include <atomic>
#include <charconv>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include <limits>

namespace xdna::qubic {
namespace {

[[nodiscard]] std::uint16_t read_u16(std::span<const Byte> bytes, std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 2U) {
        throw ProtocolError(ProtocolErrorCode::Truncated, "truncated little-endian uint16");
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U);
}

[[nodiscard]] std::uint32_t read_u32(std::span<const Byte> bytes, std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 4U) {
        throw ProtocolError(ProtocolErrorCode::Truncated, "truncated little-endian uint32");
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U)
        | (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

[[nodiscard]] std::uint64_t read_u64(std::span<const Byte> bytes, std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 8U) {
        throw ProtocolError(ProtocolErrorCode::Truncated, "truncated little-endian uint64");
    }
    std::uint64_t value = 0U;
    for (std::size_t index = 0U; index < 8U; ++index) {
        value |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8U);
    }
    return value;
}

void write_u16(std::span<Byte> bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset] = static_cast<Byte>(value & 0xFFU);
    bytes[offset + 1U] = static_cast<Byte>((value >> 8U) & 0xFFU);
}

void write_u32(std::span<Byte> bytes, std::size_t offset, std::uint32_t value)
{
    bytes[offset] = static_cast<Byte>(value & 0xFFU);
    bytes[offset + 1U] = static_cast<Byte>((value >> 8U) & 0xFFU);
    bytes[offset + 2U] = static_cast<Byte>((value >> 16U) & 0xFFU);
    bytes[offset + 3U] = static_cast<Byte>((value >> 24U) & 0xFFU);
}

void write_u64(std::span<Byte> bytes, std::size_t offset, std::uint64_t value)
{
    for (std::size_t index = 0U; index < 8U; ++index) {
        bytes[offset + index] = static_cast<Byte>((value >> (index * 8U)) & 0xFFU);
    }
}

void append_bytes(std::vector<Byte>& destination, std::span<const Byte> source)
{
    destination.insert(destination.end(), source.begin(), source.end());
}

[[nodiscard]] bool score_equal(const bpp9000::ScoreResult& left,
                               const bpp9000::ScoreResult& right) noexcept
{
    return left.score == right.score && left.status == right.status
        && left.windows_evaluated == right.windows_evaluated && left.ticks == right.ticks;
}

[[nodiscard]] SubmissionDecision reject(SubmissionRejectReason reason,
                                         std::string detail)
{
    return SubmissionDecision{false, reason, std::move(detail)};
}

[[nodiscard]] bool same_work_parameters(const WorkContext& left,
                                        const WorkContext& right) noexcept
{
    return left.algorithm == right.algorithm && left.task == right.task
        && left.window_width == right.window_width && left.max_ticks == right.max_ticks
        && left.number_of_windows == right.number_of_windows;
}

[[nodiscard]] std::string last_transport_error(std::string_view operation,
                                               std::string_view detail)
{
    std::string message(operation);
    message += " failed after bounded reconnect attempts";
    if (!detail.empty()) {
        message += ": ";
        message += detail;
    }
    return message;
}

[[nodiscard]] std::uint32_t next_dejavu() noexcept
{
    static std::atomic<std::uint32_t> counter{1U};
    const std::uint32_t value = counter.fetch_add(1U, std::memory_order_relaxed);
    if (value != 0U) {
        return value;
    }
    return counter.fetch_add(1U, std::memory_order_relaxed);
}

[[nodiscard]] bool is_unsolicited_network_frame(const Frame& frame) noexcept
{
    // A direct node sends its peer-exchange frame as soon as a connection is
    // accepted. Once handshaked, it may also stream ordinary network traffic
    // on the same TCP connection. These are not responses to our request.
    switch (frame.type) {
    case kExchangePublicPeers:
    case kBroadcastMessage:
    case kBroadcastComputors:
    case 3U:  // BROADCAST_TICK
    case 8U:  // BROADCAST_FUTURE_TICK_DATA
    case 24U: // BROADCAST_TRANSACTION
    case 68U: // BROADCAST_CUSTOM_MINING_TASK
    case 69U: // BROADCAST_CUSTOM_MINING_SOLUTION
    // A peer can also put ordinary request traffic on this stream. These
    // are not responses to the bounded request made by this client.
    case 11U:  // REQUEST_COMPUTORS
    case 14U:  // REQUEST_QUORUM_TICK
    case 16U:  // REQUEST_TICK_DATA
    case 26U:  // REQUEST_TRANSACTION_INFO
    case 27U:  // REQUEST_CURRENT_TICK_INFO
    case 29U:  // REQUEST_TICK_TRANSACTIONS
    case 31U:  // REQUEST_ENTITY
    case 33U:  // REQUEST_CONTRACT_IPO
    case 36U:  // REQUEST_ISSUED_ASSETS
    case 38U:  // REQUEST_OWNED_ASSETS
    case 40U:  // REQUEST_POSSESSED_ASSETS
    case 42U:  // REQUEST_CONTRACT_FUNCTION
    case 44U:  // REQUEST_LOG
    case 46U:  // REQUEST_SYSTEM_INFO
    case 48U:  // REQUEST_LOG_ID_RANGE_FROM_TX
    case 50U:  // REQUEST_ALL_LOG_ID_RANGES_FROM_TX
    case 52U:  // REQUEST_ASSETS
    case 56U:  // REQUEST_PRUNING_LOG
    case 58U:  // REQUEST_LOG_STATE_DIGEST
    case 64U:  // REQUEST_ACTIVE_IPOS
    case 66U:  // REQUEST_ORACLE_DATA
    case 70U:  // REQUEST_REVENUE_DATA
    case 201U: // REQUEST_TX_STATUS
        return true;
    default:
        return false;
    }
}

[[nodiscard]] int timeout_milliseconds(std::chrono::milliseconds timeout)
{
    if (timeout.count() <= 0) {
        return 1;
    }
    const auto maximum = static_cast<std::chrono::milliseconds>(std::numeric_limits<int>::max());
    if (timeout > maximum) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(timeout.count());
}

class TcpByteStream final : public ByteStream {
public:
    TcpByteStream(int fd, TransportTimeouts timeouts)
        : fd_(fd)
    {
        timeval receive_timeout{};
        const auto read_ms = timeout_milliseconds(timeouts.read);
        receive_timeout.tv_sec = static_cast<decltype(receive_timeout.tv_sec)>(read_ms / 1000);
        receive_timeout.tv_usec = static_cast<decltype(receive_timeout.tv_usec)>((read_ms % 1000) * 1000);
        (void)::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout));

        timeval send_timeout{};
        const auto write_ms = timeout_milliseconds(timeouts.write);
        send_timeout.tv_sec = static_cast<decltype(send_timeout.tv_sec)>(write_ms / 1000);
        send_timeout.tv_usec = static_cast<decltype(send_timeout.tv_usec)>((write_ms % 1000) * 1000);
        (void)::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));
    }

    ~TcpByteStream() override
    {
        close();
    }

    [[nodiscard]] std::size_t read_some(std::span<Byte> destination) override
    {
        if (destination.empty()) {
            return 0U;
        }
        const ssize_t result = ::recv(fd_, destination.data(), destination.size(), 0);
        if (result == 0) {
            return 0U;
        }
        if (result < 0) {
            if (errno == EINTR) {
                return read_some(destination);
            }
            throw TransportError(std::string("TCP read failed: ") + std::strerror(errno));
        }
        return static_cast<std::size_t>(result);
    }

    [[nodiscard]] std::size_t write_some(std::span<const Byte> source) override
    {
        if (source.empty()) {
            return 0U;
        }
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif
        const ssize_t result = ::send(fd_, source.data(), source.size(), flags);
        if (result < 0) {
            if (errno == EINTR) {
                return write_some(source);
            }
            throw TransportError(std::string("TCP write failed: ") + std::strerror(errno));
        }
        if (result == 0) {
            throw TransportError("TCP write returned zero bytes");
        }
        return static_cast<std::size_t>(result);
    }

    void close() noexcept override
    {
        if (fd_ >= 0) {
            (void)::close(fd_);
            fd_ = -1;
        }
    }

    [[nodiscard]] static std::unique_ptr<TcpByteStream> open(const NodeEndpoint& endpoint,
                                                             const TransportTimeouts& timeouts)
    {
        if (endpoint.host.empty() || endpoint.port == 0U) {
            throw TransportError("TCP endpoint is empty or has port zero");
        }

        const std::string service = std::to_string(endpoint.port);
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        addrinfo* addresses = nullptr;
        const int lookup = ::getaddrinfo(endpoint.host.c_str(), service.c_str(), &hints, &addresses);
        if (lookup != 0) {
            throw TransportError(std::string("TCP address lookup failed: ") + ::gai_strerror(lookup));
        }

        std::string error = "no TCP address connected";
        const int poll_timeout = timeout_milliseconds(timeouts.connect);
        int connected_fd = -1;
        for (addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
            const int fd = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
            if (fd < 0) {
                error = std::string("TCP socket creation failed: ") + std::strerror(errno);
                continue;
            }
            const int old_flags = ::fcntl(fd, F_GETFL, 0);
            if (old_flags < 0 || ::fcntl(fd, F_SETFL, old_flags | O_NONBLOCK) < 0) {
                error = std::string("TCP nonblocking setup failed: ") + std::strerror(errno);
                (void)::close(fd);
                continue;
            }

            const int result = ::connect(fd, address->ai_addr, address->ai_addrlen);
            if (result < 0 && errno != EINPROGRESS) {
                error = std::string("TCP connect failed: ") + std::strerror(errno);
                (void)::close(fd);
                continue;
            }
            if (result < 0) {
                pollfd descriptor{fd, POLLOUT, 0};
                const int ready = ::poll(&descriptor, 1U, poll_timeout);
                if (ready <= 0 || (descriptor.revents & POLLOUT) == 0) {
                    error = ready == 0 ? "TCP connect timed out" : "TCP connect poll failed";
                    (void)::close(fd);
                    continue;
                }
                int socket_error = 0;
                socklen_t socket_error_size = sizeof(socket_error);
                if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_size) != 0
                    || socket_error != 0) {
                    error = socket_error == 0 ? "TCP connect status failed" : std::strerror(socket_error);
                    (void)::close(fd);
                    continue;
                }
            }
            if (::fcntl(fd, F_SETFL, old_flags) < 0) {
                error = std::string("TCP blocking setup failed: ") + std::strerror(errno);
                (void)::close(fd);
                continue;
            }
            connected_fd = fd;
            break;
        }
        ::freeaddrinfo(addresses);
        if (connected_fd < 0) {
            throw TransportError(error);
        }
        return std::make_unique<TcpByteStream>(connected_fd, timeouts);
    }

private:
    int fd_ = -1;
};

[[nodiscard]] bool parse_hex_byte(char high, char low, Byte& value) noexcept
{
    const auto nibble = [](char character) -> int {
        if (character >= '0' && character <= '9') {
            return character - '0';
        }
        if (character >= 'a' && character <= 'f') {
            return character - 'a' + 10;
        }
        if (character >= 'A' && character <= 'F') {
            return character - 'A' + 10;
        }
        return -1;
    };
    const int high_value = nibble(high);
    const int low_value = nibble(low);
    if (high_value < 0 || low_value < 0) {
        return false;
    }
    value = static_cast<Byte>((high_value << 4) | low_value);
    return true;
}

[[nodiscard]] std::array<Byte, 32U> parse_hex_32(std::string_view text,
                                                 std::string_view field)
{
    if (text.size() != 64U) {
        throw ProtocolError(ProtocolErrorCode::InvalidEndpoint,
                            std::string(field) + " must contain exactly 64 hex characters");
    }
    std::array<Byte, 32U> value{};
    for (std::size_t index = 0U; index < value.size(); ++index) {
        if (!parse_hex_byte(text[index * 2U], text[index * 2U + 1U], value[index])) {
            throw ProtocolError(ProtocolErrorCode::InvalidEndpoint,
                                std::string(field) + " contains a non-hex character");
        }
    }
    return value;
}

[[nodiscard]] std::uint64_t parse_unsigned_env(std::string_view text,
                                                std::string_view field)
{
    std::uint64_t value = 0U;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        throw ProtocolError(ProtocolErrorCode::InvalidEndpoint,
                            std::string(field) + " is not an unsigned integer");
    }
    return value;
}

[[nodiscard]] const char* environment_value(std::string_view name)
{
    std::string owned(name);
    return std::getenv(owned.c_str());
}

[[nodiscard]] Frame request_read_only(ConnectionFactory& factory,
                                      const NodeEndpoint& endpoint,
                                      const TransportTimeouts& timeouts,
                                      const ReconnectPolicy& reconnect,
                                      std::span<const Byte> request,
                                      std::uint8_t response_type,
                                      std::string_view operation)
{
    if (endpoint.host.empty() || endpoint.port == 0U) {
        throw ProtocolError(ProtocolErrorCode::InvalidEndpoint,
                            std::string(operation) + " endpoint is not configured");
    }
    const std::uint32_t attempts = reconnect.max_attempts == 0U ? 1U : reconnect.max_attempts;
    std::string last_error;
    for (std::uint32_t attempt = 0U; attempt < attempts; ++attempt) {
        try {
            std::unique_ptr<ByteStream> stream = factory.connect(endpoint, timeouts);
            if (!stream) {
                throw TransportError("connection factory returned no stream");
            }
            FramedConnection connection(*stream);
            connection.write_frame(make_exchange_public_peers(next_dejavu()));
            connection.write_frame(request);
            constexpr std::uint32_t kMaximumIgnoredFrames = 64U;
            for (std::uint32_t ignored = 0U; ignored < kMaximumIgnoredFrames; ++ignored) {
                const Frame response = connection.read_frame();
                if (response.type == response_type) {
                    stream->close();
                    return response;
                }
                if (!is_unsolicited_network_frame(response)) {
                    throw ProtocolError(ProtocolErrorCode::WrongMessageType,
                                        std::string(operation)
                                            + " received unexpected response frame type "
                                            + std::to_string(response.type));
                }
            }
            throw TransportError(std::string(operation)
                                 + " response was not received before the unsolicited-frame limit");
        } catch (const ProtocolError&) {
            throw;
        } catch (const TransportError& error) {
            last_error = error.what();
            if (attempt + 1U < attempts && reconnect.delay.count() > 0) {
                std::this_thread::sleep_for(reconnect.delay);
            }
        }
    }
    throw TransportError(last_transport_error(operation, last_error));
}

} // namespace

std::vector<Byte> serialize_frame(std::uint8_t type,
                                  std::uint32_t dejavu,
                                  std::span<const Byte> payload)
{
    if (payload.size() > static_cast<std::size_t>(kMaximumFrameBytes) - kRequestResponseHeaderBytes) {
        throw ProtocolError(ProtocolErrorCode::FrameTooLarge, "frame payload exceeds the three-byte size field");
    }
    const auto total = kRequestResponseHeaderBytes + payload.size();
    std::vector<Byte> encoded(total, 0U);
    encoded[0U] = static_cast<Byte>(total & 0xFFU);
    encoded[1U] = static_cast<Byte>((total >> 8U) & 0xFFU);
    encoded[2U] = static_cast<Byte>((total >> 16U) & 0xFFU);
    encoded[3U] = type;
    write_u32(std::span<Byte>(encoded), 4U, dejavu);
    std::copy(payload.begin(), payload.end(), encoded.begin() + static_cast<std::ptrdiff_t>(kRequestResponseHeaderBytes));
    return encoded;
}

Frame parse_frame(std::span<const Byte> encoded)
{
    if (encoded.size() < kRequestResponseHeaderBytes) {
        throw ProtocolError(ProtocolErrorCode::Truncated, "frame is shorter than its eight-byte header");
    }
    const std::uint32_t declared_size = static_cast<std::uint32_t>(encoded[0U])
        | (static_cast<std::uint32_t>(encoded[1U]) << 8U)
        | (static_cast<std::uint32_t>(encoded[2U]) << 16U);
    if (declared_size < kRequestResponseHeaderBytes) {
        throw ProtocolError(ProtocolErrorCode::InvalidFrameSize, "frame declares a size smaller than its header");
    }
    if (declared_size > kMaximumFrameBytes) {
        throw ProtocolError(ProtocolErrorCode::FrameTooLarge, "frame declares an unsupported size");
    }
    if (declared_size != encoded.size()) {
        throw ProtocolError(ProtocolErrorCode::InvalidFrameSize, "frame has trailing or truncated bytes");
    }
    Frame frame;
    frame.type = encoded[3U];
    frame.dejavu = read_u32(encoded, 4U);
    frame.payload.assign(encoded.begin() + static_cast<std::ptrdiff_t>(kRequestResponseHeaderBytes), encoded.end());
    return frame;
}

void FrameDecoder::feed(std::span<const Byte> bytes)
{
    if (bytes.size() > static_cast<std::size_t>(kMaximumFrameBytes) - bytes_.size()) {
        throw ProtocolError(ProtocolErrorCode::FrameTooLarge, "incremental frame buffer exceeds its hard limit");
    }
    bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
}

std::optional<Frame> FrameDecoder::next()
{
    if (bytes_.size() < kRequestResponseHeaderBytes) {
        return std::nullopt;
    }
    const std::uint32_t declared_size = static_cast<std::uint32_t>(bytes_[0U])
        | (static_cast<std::uint32_t>(bytes_[1U]) << 8U)
        | (static_cast<std::uint32_t>(bytes_[2U]) << 16U);
    if (declared_size < kRequestResponseHeaderBytes) {
        throw ProtocolError(ProtocolErrorCode::InvalidFrameSize, "incremental frame has an invalid size");
    }
    if (declared_size > kMaximumFrameBytes) {
        throw ProtocolError(ProtocolErrorCode::FrameTooLarge, "incremental frame exceeds the size limit");
    }
    if (bytes_.size() < declared_size) {
        return std::nullopt;
    }
    const auto end = bytes_.begin() + static_cast<std::ptrdiff_t>(declared_size);
    Frame frame = parse_frame(std::span<const Byte>(bytes_.data(), declared_size));
    bytes_.erase(bytes_.begin(), end);
    return frame;
}

std::vector<Byte> make_system_info_request(std::uint32_t dejavu)
{
    return serialize_frame(kRequestSystemInfo, dejavu, {});
}

std::vector<Byte> make_exchange_public_peers(std::uint32_t dejavu)
{
    const std::array<Byte, kExchangePublicPeersPayloadBytes> no_public_peers{};
    return serialize_frame(kExchangePublicPeers, dejavu, no_public_peers);
}

std::vector<Byte> make_computors_request(std::uint32_t dejavu)
{
    return serialize_frame(kRequestComputors, dejavu, {});
}

std::vector<Byte> make_entity_request(const PublicKey& public_key, std::uint32_t dejavu)
{
    return serialize_frame(kRequestEntity, dejavu, public_key.bytes);
}

SystemInfo parse_system_info(const Frame& frame)
{
    if (frame.type != kRespondSystemInfo) {
        throw ProtocolError(ProtocolErrorCode::WrongMessageType, "frame is not RESPOND_SYSTEM_INFO");
    }
    if (frame.payload.size() != kSystemInfoPayloadBytes) {
        throw ProtocolError(ProtocolErrorCode::InvalidPayloadSize, "system-info payload is not 128 bytes");
    }
    const std::span<const Byte> bytes(frame.payload);
    SystemInfo info;
    info.version = static_cast<std::int16_t>(read_u16(bytes, 0U));
    info.epoch = read_u16(bytes, 2U);
    info.tick = read_u32(bytes, 4U);
    info.initial_tick = read_u32(bytes, 8U);
    info.latest_created_tick = read_u32(bytes, 12U);
    info.initial_millisecond = read_u16(bytes, 16U);
    info.initial_second = bytes[18U];
    info.initial_minute = bytes[19U];
    info.initial_hour = bytes[20U];
    info.initial_day = bytes[21U];
    info.initial_month = bytes[22U];
    info.initial_year = bytes[23U];
    info.number_of_entities = read_u32(bytes, 24U);
    info.number_of_transactions = read_u32(bytes, 28U);
    std::copy_n(bytes.begin() + 32, info.mining_seed.bytes.size(), info.mining_seed.bytes.begin());
    info.solution_threshold = static_cast<std::int32_t>(read_u32(bytes, 64U));
    info.total_spectrum_amount = read_u64(bytes, 68U);
    info.current_entity_balance_dust_threshold = read_u64(bytes, 76U);
    info.target_tick_vote_signature = read_u32(bytes, 84U);
    info.computor_packet_signature = read_u64(bytes, 88U);
    info.solution_additional_threshold = read_u64(bytes, 96U);
    info.reserve2 = read_u64(bytes, 104U);
    info.reserve3 = read_u64(bytes, 112U);
    info.reserve4 = read_u64(bytes, 120U);
    return info;
}

std::vector<Byte> serialize_system_info_payload(const SystemInfo& info)
{
    std::vector<Byte> bytes(kSystemInfoPayloadBytes, 0U);
    const std::span<Byte> output(bytes);
    write_u16(output, 0U, static_cast<std::uint16_t>(info.version));
    write_u16(output, 2U, info.epoch);
    write_u32(output, 4U, info.tick);
    write_u32(output, 8U, info.initial_tick);
    write_u32(output, 12U, info.latest_created_tick);
    write_u16(output, 16U, info.initial_millisecond);
    output[18U] = info.initial_second;
    output[19U] = info.initial_minute;
    output[20U] = info.initial_hour;
    output[21U] = info.initial_day;
    output[22U] = info.initial_month;
    output[23U] = info.initial_year;
    write_u32(output, 24U, info.number_of_entities);
    write_u32(output, 28U, info.number_of_transactions);
    std::copy(info.mining_seed.bytes.begin(), info.mining_seed.bytes.end(), output.begin() + 32);
    write_u32(output, 64U, static_cast<std::uint32_t>(info.solution_threshold));
    write_u64(output, 68U, info.total_spectrum_amount);
    write_u64(output, 76U, info.current_entity_balance_dust_threshold);
    write_u32(output, 84U, info.target_tick_vote_signature);
    write_u64(output, 88U, info.computor_packet_signature);
    write_u64(output, 96U, info.solution_additional_threshold);
    write_u64(output, 104U, info.reserve2);
    write_u64(output, 112U, info.reserve3);
    write_u64(output, 120U, info.reserve4);
    return bytes;
}

ComputorList parse_computors(const Frame& frame)
{
    if (frame.type != kBroadcastComputors) {
        throw ProtocolError(ProtocolErrorCode::WrongMessageType,
                            "frame is not BROADCAST_COMPUTORS");
    }
    if (frame.payload.size() != kComputorsPayloadBytes) {
        throw ProtocolError(ProtocolErrorCode::InvalidPayloadSize,
                            "computors payload has an unexpected size");
    }
    const std::span<const Byte> bytes(frame.payload);
    ComputorList computors;
    computors.epoch = read_u16(bytes, 0U);
    std::size_t offset = 2U;
    for (PublicKey& public_key : computors.public_keys) {
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                    public_key.bytes.size(),
                    public_key.bytes.begin());
        offset += public_key.bytes.size();
    }
    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                computors.signature.size(),
                computors.signature.begin());
    return computors;
}

std::vector<Byte> serialize_computors_payload(const ComputorList& computors)
{
    std::vector<Byte> bytes(kComputorsPayloadBytes, 0U);
    const std::span<Byte> output(bytes);
    write_u16(output, 0U, computors.epoch);
    std::size_t offset = 2U;
    for (const PublicKey& public_key : computors.public_keys) {
        std::copy(public_key.bytes.begin(),
                  public_key.bytes.end(),
                  output.begin() + static_cast<std::ptrdiff_t>(offset));
        offset += public_key.bytes.size();
    }
    std::copy(computors.signature.begin(),
              computors.signature.end(),
              output.begin() + static_cast<std::ptrdiff_t>(offset));
    return bytes;
}

bool contains_computor(const ComputorList& computors, const PublicKey& public_key) noexcept
{
    return std::any_of(computors.public_keys.begin(),
                       computors.public_keys.end(),
                       [&public_key](const PublicKey& candidate) { return candidate == public_key; });
}

EntityInfo parse_entity(const Frame& frame)
{
    if (frame.type != kRespondEntity) {
        throw ProtocolError(ProtocolErrorCode::WrongMessageType,
                            "frame is not RESPOND_ENTITY");
    }
    if (frame.payload.size() != kEntityPayloadBytes) {
        throw ProtocolError(ProtocolErrorCode::InvalidPayloadSize,
                            "entity payload has an unexpected size");
    }
    const std::span<const Byte> bytes(frame.payload);
    EntityInfo entity;
    std::copy_n(bytes.begin(), entity.public_key.bytes.size(), entity.public_key.bytes.begin());
    entity.incoming_amount = static_cast<std::int64_t>(read_u64(bytes, 32U));
    entity.outgoing_amount = static_cast<std::int64_t>(read_u64(bytes, 40U));
    entity.number_of_incoming_transfers = read_u32(bytes, 48U);
    entity.number_of_outgoing_transfers = read_u32(bytes, 52U);
    entity.latest_incoming_transfer_tick = read_u32(bytes, 56U);
    entity.latest_outgoing_transfer_tick = read_u32(bytes, 60U);
    entity.tick = read_u32(bytes, 64U);
    entity.spectrum_index = static_cast<std::int32_t>(read_u32(bytes, 68U));
    std::size_t offset = 72U;
    for (PublicKey& sibling : entity.siblings) {
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                    sibling.bytes.size(),
                    sibling.bytes.begin());
        offset += sibling.bytes.size();
    }
    return entity;
}

bool is_supported_algorithm(Algorithm algorithm) noexcept
{
    return algorithm == Algorithm::Bpp9000;
}

TaskIdentity task_identity(const bpp9000::Task& task)
{
    return TaskIdentity{task.header.shape, task.header.topology_hash, task.header.data_hash};
}

WorkContext make_work_context(const SystemInfo& info,
                              const TaskIdentity& task,
                              Algorithm algorithm,
                              std::uint64_t window_width,
                              std::uint32_t max_ticks)
{
    if (!is_supported_algorithm(algorithm)) {
        throw ProtocolError(ProtocolErrorCode::UnsupportedAlgorithm, "the requested algorithm is not supported");
    }
    if (!bpp9000::is_valid_mining_seed(info.mining_seed)) {
        throw ProtocolError(ProtocolErrorCode::InvalidSeed, "system info contains a zero mining seed");
    }
    if (info.solution_threshold < 0) {
        throw ProtocolError(ProtocolErrorCode::InvalidThreshold, "system info contains a negative threshold");
    }
    if (task.shape.sequence_length == 0U || window_width == 0U
        || window_width >= task.shape.sequence_length || max_ticks <= window_width) {
        throw ProtocolError(ProtocolErrorCode::InvalidTask, "task shape and score configuration are incompatible");
    }
    const std::uint64_t windows = task.shape.sequence_length - window_width;
    const auto threshold = static_cast<std::uint64_t>(info.solution_threshold);
    if (threshold > windows) {
        throw ProtocolError(ProtocolErrorCode::InvalidThreshold,
                            "system-info threshold exceeds the finite score range");
    }
    WorkContext context;
    context.epoch = info.epoch;
    context.tick = info.tick;
    context.mining_seed = info.mining_seed;
    context.solution_threshold = static_cast<std::uint32_t>(threshold);
    context.algorithm = algorithm;
    context.task = task;
    context.window_width = window_width;
    context.max_ticks = max_ticks;
    context.number_of_windows = windows;
    return context;
}

WorkContext make_work_context(const SystemInfo& info,
                              const bpp9000::Task& task,
                              Algorithm algorithm,
                              std::uint64_t window_width,
                              std::uint32_t max_ticks)
{
    return make_work_context(info, task_identity(task), algorithm, window_width, max_ticks);
}

ContextUpdate WorkContextTracker::observe(const WorkContext& incoming)
{
    if (!current_.has_value()) {
        current_ = incoming;
        return ContextUpdate::Installed;
    }
    const WorkContext& current = *current_;
    if (incoming.epoch < current.epoch) {
        return ContextUpdate::RejectedOlderEpoch;
    }
    if (incoming.epoch == current.epoch && incoming.mining_seed == current.mining_seed
        && incoming.tick < current.tick) {
        return ContextUpdate::RejectedOlderTick;
    }
    if (incoming == current) {
        return ContextUpdate::Unchanged;
    }
    current_ = incoming;
    return ContextUpdate::Advanced;
}

const WorkContext& WorkContextTracker::current() const
{
    if (!current_.has_value()) {
        throw ProtocolError(ProtocolErrorCode::StaleContext, "no current system-info context is installed");
    }
    return *current_;
}

bool WorkContextTracker::is_fresh(const WorkContext& candidate) const noexcept
{
    if (!current_.has_value()) {
        return false;
    }
    const WorkContext& current = *current_;
    return same_work_parameters(candidate, current) && candidate.epoch == current.epoch
        && candidate.mining_seed == current.mining_seed
        && candidate.solution_threshold == current.solution_threshold
        && candidate.tick <= current.tick;
}

CryptoProviderInfo UnavailableCryptoProvider::info() const
{
    return CryptoProviderInfo{"unavailable", "none selected", false};
}

SubmissionDecision authorize_submission(const SubmissionInput& input,
                                        const WorkContext& current_context)
{
    if (!is_supported_algorithm(input.candidate_algorithm)
        || input.candidate_algorithm != current_context.algorithm
        || input.candidate_context.algorithm != input.candidate_algorithm) {
        return reject(SubmissionRejectReason::UnsupportedAlgorithm, "algorithm is not the current BPP9000 algorithm");
    }
    if (!(input.candidate_task == current_context.task)
        || !(input.candidate_context.task == current_context.task)) {
        return reject(SubmissionRejectReason::TaskMismatch, "candidate task identity differs from current task");
    }
    if (!same_work_parameters(input.candidate_context, current_context)
        || input.candidate_context.epoch != current_context.epoch
        || input.candidate_context.mining_seed != current_context.mining_seed
        || input.candidate_context.solution_threshold != current_context.solution_threshold
        || input.candidate_context.tick > current_context.tick) {
        return reject(SubmissionRejectReason::ContextMismatch, "candidate context is not current-compatible");
    }
    if (!bpp9000::is_valid_mining_seed(input.candidate.mining_seed)
        || input.candidate.mining_seed != current_context.mining_seed) {
        return reject(SubmissionRejectReason::InvalidSeed, "candidate seed is stale or invalid");
    }
    if (!bpp9000::is_canonical_nonce(input.candidate.nonce)) {
        return reject(SubmissionRejectReason::InvalidNonce, "candidate nonce is not canonical BPP9000");
    }
    if (!score_equal(input.evidence.cpu, input.evidence.npu)) {
        return reject(SubmissionRejectReason::CpuNpuMismatch, "CPU and NPU score evidence differs");
    }
    if (input.evidence.cpu.timed_out() || input.evidence.cpu.score == bpp9000::kTimeoutScore) {
        return reject(SubmissionRejectReason::Timeout, "timeout sentinel cannot be submitted");
    }
    if (input.evidence.cpu.status != bpp9000::ScoreStatus::Settled
        || input.evidence.cpu.windows_evaluated != current_context.number_of_windows
        || input.evidence.cpu.score != input.candidate.score
        || input.evidence.npu.score != input.candidate.score) {
        return reject(SubmissionRejectReason::BadScore, "candidate score is not an exact finite full-score result");
    }
    if (static_cast<std::uint64_t>(input.candidate.score) > current_context.number_of_windows) {
        return reject(SubmissionRejectReason::BadScore, "candidate score exceeds the finite score range");
    }
    if (input.candidate.score > current_context.solution_threshold) {
        return reject(SubmissionRejectReason::ThresholdExceeded, "candidate score is above the current threshold");
    }
    if (input.source_public_key.is_zero()) {
        return reject(SubmissionRejectReason::MissingSource, "source public key is zero");
    }
    if (input.destination_public_key.is_zero()) {
        return reject(SubmissionRejectReason::MissingDestination, "destination public key is zero");
    }
    if (input.signing_material == nullptr
        || input.signing_material->public_key != input.source_public_key) {
        return reject(SubmissionRejectReason::MissingSigningMaterial, "signing material is unavailable or mismatched");
    }
    return SubmissionDecision{true, SubmissionRejectReason::None, "authorized"};
}

DirectNodeSolution build_solution(const SubmissionInput& input,
                                  const CryptoProvider& crypto)
{
    const SubmissionDecision decision = authorize_submission(input, input.candidate_context);
    if (!decision.authorized) {
        switch (decision.reason) {
        case SubmissionRejectReason::UnsupportedAlgorithm:
            throw ProtocolError(ProtocolErrorCode::UnsupportedAlgorithm, decision.detail);
        case SubmissionRejectReason::TaskMismatch:
        case SubmissionRejectReason::ContextMismatch:
            throw ProtocolError(ProtocolErrorCode::InvalidTask, decision.detail);
        case SubmissionRejectReason::InvalidSeed:
            throw ProtocolError(ProtocolErrorCode::InvalidSeed, decision.detail);
        case SubmissionRejectReason::InvalidNonce:
            throw ProtocolError(ProtocolErrorCode::InvalidNonce, decision.detail);
        case SubmissionRejectReason::CpuNpuMismatch:
            throw ProtocolError(ProtocolErrorCode::ScoreMismatch, decision.detail);
        case SubmissionRejectReason::Timeout:
        case SubmissionRejectReason::BadScore:
        case SubmissionRejectReason::ThresholdExceeded:
            throw ProtocolError(ProtocolErrorCode::InvalidScore, decision.detail);
        case SubmissionRejectReason::MissingSigningMaterial:
            throw ProtocolError(ProtocolErrorCode::MissingSigningMaterial, decision.detail);
        default:
            throw ProtocolError(ProtocolErrorCode::InvalidTask, decision.detail);
        }
    }

    std::array<Byte, 32U> shared_key{};
    if (input.source_public_key == input.destination_public_key) {
        if (!crypto.derive_shared_key(*input.signing_material,
                                      input.destination_public_key,
                                      std::span<Byte>(shared_key))) {
            throw ProtocolError(ProtocolErrorCode::CryptoUnavailable,
                                "crypto provider cannot derive the encrypted direct-node shared key");
        }
    }

    std::array<Byte, 64U> shared_key_and_nonce{};
    std::copy(shared_key.begin(), shared_key.end(), shared_key_and_nonce.begin());
    std::copy(input.gamming_nonce.begin(), input.gamming_nonce.end(), shared_key_and_nonce.begin() + 32);
    std::array<Byte, 32U> gamming_key{};
    if (!crypto.hash(std::span<const Byte>(shared_key_and_nonce), std::span<Byte>(gamming_key))) {
        throw ProtocolError(ProtocolErrorCode::CryptoUnavailable, "crypto provider cannot derive gamming key");
    }
    if (gamming_key[0U] != kSolutionMessage) {
        throw ProtocolError(ProtocolErrorCode::InvalidGammaNonce,
                            "gamming nonce does not select the solution message type");
    }

    std::array<Byte, kEncryptedSolutionBytes> encrypted{};
    std::array<Byte, kEncryptedSolutionBytes> gamma{};
    if (!crypto.hash(std::span<const Byte>(gamming_key), std::span<Byte>(gamma))) {
        throw ProtocolError(ProtocolErrorCode::CryptoUnavailable, "crypto provider cannot derive message gamma");
    }
    for (std::size_t index = 0U; index < 32U; ++index) {
        encrypted[index] = input.candidate.mining_seed.bytes[index] ^ gamma[index];
        encrypted[index + 32U] = input.candidate.nonce.bytes[index] ^ gamma[index + 32U];
    }
    for (std::size_t index = 0U; index < 4U; ++index) {
        encrypted[index + 64U] = static_cast<Byte>((input.candidate.score >> (index * 8U)) & 0xFFU)
            ^ gamma[index + 64U];
    }

    std::vector<Byte> body;
    body.reserve(132U);
    append_bytes(body, input.source_public_key.bytes);
    append_bytes(body, input.destination_public_key.bytes);
    append_bytes(body, input.gamming_nonce);
    append_bytes(body, encrypted);

    std::array<Byte, 32U> digest{};
    if (!crypto.hash(std::span<const Byte>(body), std::span<Byte>(digest))) {
        throw ProtocolError(ProtocolErrorCode::CryptoUnavailable, "crypto provider cannot hash solution body");
    }
    std::array<Byte, kSignatureBytes> signature{};
    if (!crypto.sign(*input.signing_material,
                     std::span<const Byte>(digest),
                     std::span<Byte>(signature))) {
        throw ProtocolError(ProtocolErrorCode::CryptoUnavailable, "crypto provider cannot sign solution body");
    }

    std::vector<Byte> payload = body;
    append_bytes(payload, signature);
    DirectNodeSolution solution;
    solution.source_public_key = input.source_public_key;
    solution.destination_public_key = input.destination_public_key;
    solution.gamming_nonce = input.gamming_nonce;
    solution.candidate = input.candidate;
    solution.encrypted_payload = encrypted;
    solution.signature = signature;
    solution.frame = serialize_frame(kBroadcastMessage, 0U, payload);
    return solution;
}

std::unique_ptr<ByteStream> TcpConnectionFactory::connect(const NodeEndpoint& endpoint,
                                                          const TransportTimeouts& timeouts)
{
    return TcpByteStream::open(endpoint, timeouts);
}

Frame FramedConnection::read_frame()
{
    std::array<Byte, kRequestResponseHeaderBytes> header{};
    std::size_t offset = 0U;
    while (offset < header.size()) {
        const std::size_t count = stream_.read_some(std::span<Byte>(header).subspan(offset));
        if (count == 0U) {
            throw TransportError("connection closed during frame header");
        }
        offset += count;
    }
    const std::uint32_t declared_size = static_cast<std::uint32_t>(header[0U])
        | (static_cast<std::uint32_t>(header[1U]) << 8U)
        | (static_cast<std::uint32_t>(header[2U]) << 16U);
    if (declared_size < kRequestResponseHeaderBytes) {
        throw ProtocolError(ProtocolErrorCode::InvalidFrameSize, "peer frame is smaller than its header");
    }
    if (declared_size > kMaximumFrameBytes) {
        throw ProtocolError(ProtocolErrorCode::FrameTooLarge, "peer frame exceeds the size limit");
    }
    std::vector<Byte> encoded(declared_size, 0U);
    std::copy(header.begin(), header.end(), encoded.begin());
    offset = kRequestResponseHeaderBytes;
    while (offset < encoded.size()) {
        const std::size_t count = stream_.read_some(std::span<Byte>(encoded).subspan(offset));
        if (count == 0U) {
            throw TransportError("connection closed during frame payload");
        }
        offset += count;
    }
    return parse_frame(encoded);
}

void FramedConnection::write_frame(std::span<const Byte> frame)
{
    (void)parse_frame(frame);
    std::size_t offset = 0U;
    while (offset < frame.size()) {
        const std::size_t count = stream_.write_some(frame.subspan(offset));
        if (count == 0U) {
            throw TransportError("connection closed during frame write");
        }
        offset += count;
    }
}

SystemInfo DirectNodeClient::request_system_info()
{
    const std::vector<Byte> request = make_system_info_request(next_dejavu());
    const Frame response = request_read_only(factory_,
                                              endpoint_,
                                              timeouts_,
                                              reconnect_,
                                              request,
                                              kRespondSystemInfo,
                                              "system-info request");
    return parse_system_info(response);
}

ComputorList DirectNodeClient::request_computors()
{
    const std::vector<Byte> request = make_computors_request(next_dejavu());
    const Frame response = request_read_only(factory_,
                                              endpoint_,
                                              timeouts_,
                                              reconnect_,
                                              request,
                                              kBroadcastComputors,
                                              "computors request");
    return parse_computors(response);
}

EntityInfo DirectNodeClient::request_entity(const PublicKey& public_key)
{
    if (public_key.is_zero()) {
        throw ProtocolError(ProtocolErrorCode::InvalidEndpoint,
                            "entity request public key must be nonzero");
    }
    const std::vector<Byte> request = make_entity_request(public_key, next_dejavu());
    const Frame response = request_read_only(factory_,
                                              endpoint_,
                                              timeouts_,
                                              reconnect_,
                                              request,
                                              kRespondEntity,
                                              "entity request");
    return parse_entity(response);
}

bool DirectNodeClient::submit_frame(std::span<const Byte> frame)
{
    (void)parse_frame(frame);
    if (endpoint_.host.empty() || endpoint_.port == 0U) {
        throw ProtocolError(ProtocolErrorCode::InvalidEndpoint, "direct-node endpoint is not configured");
    }
    const std::uint32_t attempts = reconnect_.max_attempts == 0U ? 1U : reconnect_.max_attempts;
    std::string last_error;
    for (std::uint32_t attempt = 0U; attempt < attempts; ++attempt) {
        try {
            std::unique_ptr<ByteStream> stream = factory_.connect(endpoint_, timeouts_);
            if (!stream) {
                throw TransportError("connection factory returned no stream");
            }
            FramedConnection connection(*stream);
            connection.write_frame(make_exchange_public_peers(next_dejavu()));
            connection.write_frame(frame);
            stream->close();
            return true;
        } catch (const ProtocolError&) {
            throw;
        } catch (const TransportError& error) {
            last_error = error.what();
            if (attempt + 1U < attempts && reconnect_.delay.count() > 0) {
                std::this_thread::sleep_for(reconnect_.delay);
            }
        }
    }
    throw TransportError(last_transport_error("solution submission", last_error));
}

AdapterSubmitResult DirectNodeAdapter::submit(const SubmissionInput& input,
                                              const WorkContext& current_context)
{
    AdapterSubmitResult result;
    result.decision = authorize_submission(input, current_context);
    if (!result.decision.authorized) {
        return result;
    }
    if (!allow_live_submission_) {
        result.sent = false;
        result.decision = SubmissionDecision{
            false,
            SubmissionRejectReason::LiveSubmissionDisabled,
            "live submission is disabled by runtime configuration",
        };
        return result;
    }
    try {
        const DirectNodeSolution solution = build_solution(input, crypto_);
        result.sent = client_.submit_frame(solution.frame);
    } catch (const ProtocolError& error) {
        result.sent = false;
        if (error.code() == ProtocolErrorCode::CryptoUnavailable) {
            result.decision = SubmissionDecision{false, SubmissionRejectReason::CryptoUnavailable, error.what()};
        } else if (error.code() == ProtocolErrorCode::InvalidGammaNonce) {
            result.decision = SubmissionDecision{false, SubmissionRejectReason::InvalidGammaNonce, error.what()};
        } else {
            result.decision = SubmissionDecision{false, SubmissionRejectReason::BadScore, error.what()};
        }
    } catch (const TransportError& error) {
        result.transport_error = error.what();
    }
    return result;
}

std::string RuntimeConfig::redacted_summary() const
{
    std::string summary = "endpoint=" + endpoint.host + ":" + std::to_string(endpoint.port);
    summary += " signing_material=";
    summary += signing_material.has_value() ? "configured" : "unconfigured";
    summary += " allow_live_submission=";
    summary += allow_live_submission ? "true" : "false";
    return summary;
}

RuntimeConfig load_runtime_config_from_environment(std::string_view prefix)
{
    RuntimeConfig config;
    const std::string host_name = std::string(prefix) + "NODE_HOST";
    const std::string port_name = std::string(prefix) + "NODE_PORT";
    if (const char* host = environment_value(host_name); host != nullptr) {
        config.endpoint.host = host;
    }
    if (const char* port = environment_value(port_name); port != nullptr) {
        const std::uint64_t value = parse_unsigned_env(port, port_name);
        if (value == 0U || value > 65535U) {
            throw ProtocolError(ProtocolErrorCode::InvalidEndpoint, "node port is outside 1..65535");
        }
        config.endpoint.port = static_cast<std::uint16_t>(value);
    }

    const std::string public_name = std::string(prefix) + "SIGNING_PUBLIC_KEY_HEX";
    const std::string secret_name = std::string(prefix) + "SIGNING_SECRET_HEX";
    const char* public_key = environment_value(public_name);
    const char* secret = environment_value(secret_name);
    if ((public_key == nullptr) != (secret == nullptr)) {
        throw ProtocolError(ProtocolErrorCode::MissingSigningMaterial,
                            "signing public key and secret must be configured together");
    }
    if (public_key != nullptr && secret != nullptr) {
        SigningMaterial material;
        material.public_key.bytes = parse_hex_32(public_key, public_name);
        material.secret.bytes = parse_hex_32(secret, secret_name);
        config.signing_material.emplace(std::move(material));
    }

    const std::string allow_name = std::string(prefix) + "ALLOW_LIVE_SUBMISSION";
    if (const char* allow = environment_value(allow_name); allow != nullptr) {
        if (std::string_view(allow) == "1") {
            config.allow_live_submission = true;
        } else if (std::string_view(allow) == "0") {
            config.allow_live_submission = false;
        } else {
            throw ProtocolError(ProtocolErrorCode::InvalidEndpoint,
                                allow_name + " must be exactly 0 or 1");
        }
    }
    return config;
}

} // namespace xdna::qubic
