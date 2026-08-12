#include "pearl/gateway.hpp"

#include "pearl/json.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdexcept>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace xdna::pearl {
namespace {

using Clock = std::chrono::steady_clock;

[[noreturn]] void fail(GatewayErrorCode code, const std::string& message)
{
    throw GatewayError(code, message);
}

[[nodiscard]] std::uint32_t parse_u32(std::string_view text, const char* label)
{
    if (text.empty() || text.front() == '-') {
        fail(GatewayErrorCode::MalformedResponse, std::string(label) + " is not a nonnegative integer");
    }
    std::uint32_t value = 0U;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        fail(GatewayErrorCode::MalformedResponse, std::string(label) + " is outside u32 range");
    }
    return value;
}

[[nodiscard]] Digest decimal_target(std::string_view text)
{
    if (text.empty() || text.front() == '-') {
        fail(GatewayErrorCode::MalformedResponse, "target must be a nonnegative decimal integer");
    }
    Digest result{};
    for (const char character : text) {
        if (character < '0' || character > '9') {
            fail(GatewayErrorCode::MalformedResponse, "target must be a decimal integer");
        }
        std::uint32_t carry = static_cast<std::uint32_t>(character - '0');
        for (std::uint8_t& byte : result) {
            const std::uint32_t value = static_cast<std::uint32_t>(byte) * 10U + carry;
            byte = static_cast<std::uint8_t>(value & 0xFFU);
            carry = value >> 8U;
        }
        if (carry != 0U) {
            fail(GatewayErrorCode::MalformedResponse, "target exceeds 256 bits");
        }
    }
    return result;
}

[[nodiscard]] std::string json_escape(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += character; break;
        }
    }
    return result;
}

[[nodiscard]] std::string required_string(const json::Value& object,
                                          std::string_view key,
                                          GatewayErrorCode code = GatewayErrorCode::MalformedResponse)
{
    const json::Value* value = object.find(key);
    if (value == nullptr || !value->is_string()) {
        fail(code, "required string field missing: " + std::string(key));
    }
    return value->as_string();
}

[[nodiscard]] std::string digest_hex(const Digest& value)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() * 2U);
    for (const std::uint8_t byte : value) {
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0FU]);
    }
    return result;
}

[[nodiscard]] std::string json_target(const json::Value& object)
{
    const json::Value* value = object.find("target");
    if (value == nullptr || (!value->is_number() && !value->is_string())) {
        fail(GatewayErrorCode::MalformedResponse, "target is missing or not an integer");
    }
    const std::string& text = value->is_number()
        ? value->as_number().value
        : value->as_string();
    (void)decimal_target(text);
    return text;
}

[[nodiscard]] MiningJob parse_mining_job(const json::Value& value)
{
    if (!value.is_object()) {
        fail(GatewayErrorCode::MalformedResponse, "getMiningInfo result is not an object");
    }
    const std::string header_b64 = required_string(value, "incomplete_header_bytes");
    MiningJob job;
    job.incomplete_header_bytes = base64_decode_strict(header_b64);
    if (job.incomplete_header_bytes.size() != kHeaderBytes) {
        fail(GatewayErrorCode::MalformedResponse, "incomplete header must be 76 bytes");
    }
    job.target_decimal = json_target(value);
    job.target = decimal_target(job.target_decimal);
    const json::Value* version = value.find("cert_version");
    if (version == nullptr || !version->is_number()) {
        fail(GatewayErrorCode::MalformedResponse, "cert_version is missing or not an integer");
    }
    const std::uint32_t raw_certificate_version = parse_u32(
        version->as_number().value, "cert_version");
    if (!is_supported_certificate_version(raw_certificate_version)) {
        fail(GatewayErrorCode::UnsupportedCertificateVersion,
             "UNSUPPORTED_CERTIFICATE_VERSION_" + std::to_string(raw_certificate_version));
    }
    job.certificate_version = certificate_version_from_u32(raw_certificate_version);
    const json::Value* job_id = value.find("job_id");
    if (job_id != nullptr) {
        if (!job_id->is_string()) {
            fail(GatewayErrorCode::MalformedResponse, "job_id is not a string");
        }
        job.job_id = job_id->as_string();
    } else {
        Digest key{};
        std::vector<std::uint8_t> fingerprint_input = job.incomplete_header_bytes;
        fingerprint_input.insert(fingerprint_input.end(), job.target.begin(), job.target.end());
        const auto version_text = std::to_string(
            certificate_version_number(job.certificate_version));
        fingerprint_input.insert(fingerprint_input.end(), version_text.begin(), version_text.end());
        job.job_id = "derived-" + digest_hex(blake3_keyed(key, fingerprint_input))
            .substr(0U, 32U);
    }
    if (job.job_id.size() > 256U) {
        fail(GatewayErrorCode::MalformedResponse, "job_id exceeds configured limit");
    }
    return job;
}

[[nodiscard]] int remaining_ms(Clock::time_point deadline)
{
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - Clock::now());
    if (remaining <= std::chrono::milliseconds::zero()) {
        return 0;
    }
    return remaining.count() > static_cast<long long>(INT_MAX)
        ? INT_MAX
        : static_cast<int>(remaining.count());
}

void wait_fd(int fd, short events, Clock::time_point deadline)
{
    pollfd descriptor{fd, events, 0};
    const int timeout = remaining_ms(deadline);
    if (timeout == 0) {
        fail(GatewayErrorCode::Timeout, "gateway operation timed out");
    }
    const int result = poll(&descriptor, 1U, timeout);
    if (result == 0) {
        fail(GatewayErrorCode::Timeout, "gateway operation timed out");
    }
    if (result < 0) {
        if (errno == EINTR) {
            wait_fd(fd, events, deadline);
            return;
        }
        fail(GatewayErrorCode::Transport, std::string("gateway poll failed: ") + std::strerror(errno));
    }
    if ((descriptor.revents & (POLLERR | POLLNVAL)) != 0
        || ((descriptor.revents & POLLHUP) != 0 && (descriptor.revents & POLLIN) == 0)) {
        fail(GatewayErrorCode::Transport, "gateway socket closed or failed");
    }
}

void write_all(int fd, std::string_view message, Clock::time_point deadline)
{
    std::size_t offset = 0U;
    while (offset < message.size()) {
        wait_fd(fd, POLLOUT, deadline);
        const ssize_t written = send(fd, message.data() + offset, message.size() - offset, MSG_NOSIGNAL);
        if (written < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            fail(GatewayErrorCode::Transport,
                 std::string("gateway write failed: ") + std::strerror(errno));
        }
        if (written == 0) {
            fail(GatewayErrorCode::Transport, "gateway wrote zero bytes");
        }
        offset += static_cast<std::size_t>(written);
    }
}

[[nodiscard]] std::string read_line(int fd,
                                    std::size_t max_bytes,
                                    Clock::time_point deadline)
{
    std::string result;
    result.reserve(256U);
    std::array<char, 512U> buffer{};
    while (result.size() < max_bytes) {
        wait_fd(fd, POLLIN, deadline);
        const ssize_t received = recv(fd, buffer.data(), buffer.size(), 0);
        if (received < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            fail(GatewayErrorCode::Transport,
                 std::string("gateway read failed: ") + std::strerror(errno));
        }
        if (received == 0) {
            fail(GatewayErrorCode::Transport, "gateway closed before a JSON-RPC response");
        }
        result.append(buffer.data(), static_cast<std::size_t>(received));
        const auto newline = result.find('\n');
        if (newline != std::string::npos) {
            result.resize(newline);
            if (!result.empty() && result.back() == '\r') {
                result.pop_back();
            }
            return result;
        }
    }
    fail(GatewayErrorCode::Protocol, "gateway response exceeds message-size limit");
}

void set_nonblocking(int fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        fail(GatewayErrorCode::Transport, "cannot configure gateway socket");
    }
}

[[nodiscard]] int connect_unix(const GatewayEndpoint& endpoint, Clock::time_point deadline)
{
    if (endpoint.unix_path.empty() || endpoint.unix_path.size() >= sizeof(sockaddr_un::sun_path)) {
        fail(GatewayErrorCode::Transport, "gateway Unix socket path is invalid");
    }
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        fail(GatewayErrorCode::Transport, std::string("cannot create Unix socket: ") + std::strerror(errno));
    }
    try {
        set_nonblocking(fd);
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::copy(endpoint.unix_path.begin(), endpoint.unix_path.end(), address.sun_path);
        const int result = connect(fd,
                                   reinterpret_cast<const sockaddr*>(&address),
                                   sizeof(address));
        if (result < 0 && errno != EINPROGRESS) {
            fail(GatewayErrorCode::Transport, std::string("gateway Unix connect failed: ") + std::strerror(errno));
        }
        if (result < 0) {
            wait_fd(fd, POLLOUT, deadline);
            int error = 0;
            socklen_t length = sizeof(error);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) != 0 || error != 0) {
                fail(GatewayErrorCode::Transport, "gateway Unix connect did not complete");
            }
        }
        return fd;
    } catch (...) {
        close(fd);
        throw;
    }
}

[[nodiscard]] int connect_tcp(const GatewayEndpoint& endpoint, Clock::time_point deadline)
{
    if (endpoint.host != "127.0.0.1" && endpoint.host != "localhost" && endpoint.host != "::1") {
        fail(GatewayErrorCode::Transport, "gateway TCP endpoint must be loopback");
    }
    const std::string port = std::to_string(endpoint.port);
    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    addrinfo* addresses = nullptr;
    const int lookup = getaddrinfo(endpoint.host.c_str(), port.c_str(), &hints, &addresses);
    if (lookup != 0) {
        fail(GatewayErrorCode::Transport, std::string("gateway address lookup failed: ") + gai_strerror(lookup));
    }
    int connected = -1;
    for (addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
        const int fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (fd < 0) {
            continue;
        }
        set_nonblocking(fd);
        const int result = connect(fd, address->ai_addr, address->ai_addrlen);
        if (result == 0) {
            connected = fd;
            break;
        }
        if (errno == EINPROGRESS) {
            try {
                wait_fd(fd, POLLOUT, deadline);
                int error = 0;
                socklen_t length = sizeof(error);
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) == 0 && error == 0) {
                    connected = fd;
                    break;
                }
            } catch (const GatewayError&) {
                // Try no other address after the configured deadline.
                close(fd);
                freeaddrinfo(addresses);
                throw;
            }
        }
        close(fd);
    }
    freeaddrinfo(addresses);
    if (connected < 0) {
        fail(GatewayErrorCode::Transport, "gateway loopback TCP connect failed");
    }
    return connected;
}

[[nodiscard]] int connect_endpoint(const GatewayEndpoint& endpoint, Clock::time_point deadline)
{
    return endpoint.transport == GatewayTransport::Unix
        ? connect_unix(endpoint, deadline)
        : connect_tcp(endpoint, deadline);
}

[[nodiscard]] std::string rpc_request(std::uint64_t id,
                                      std::string_view method,
                                      std::string_view params)
{
    return "{\"jsonrpc\":\"2.0\",\"method\":\"" + json_escape(method)
        + "\",\"params\":" + std::string(params)
        + ",\"id\":" + std::to_string(id) + "}\n";
}

[[nodiscard]] json::Value rpc_call(const GatewayClientConfig& config,
                                   std::uint64_t id,
                                   std::string_view method,
                                   std::string_view params)
{
    if (config.timeout <= std::chrono::milliseconds::zero()) {
        fail(GatewayErrorCode::Timeout, "gateway timeout must be positive");
    }
    const auto deadline = Clock::now() + config.timeout;
    int fd = connect_endpoint(config.endpoint, deadline);
    try {
        const std::string request = rpc_request(id, method, params);
        if (request.size() > config.max_message_bytes) {
            fail(GatewayErrorCode::Protocol, "gateway request exceeds message-size limit");
        }
        write_all(fd, request, deadline);
        const std::string response = read_line(fd, config.max_message_bytes, deadline);
        close(fd);
        fd = -1;
        json::Value body;
        try {
            body = json::parse(response, config.max_message_bytes);
        } catch (const std::exception& error) {
            fail(GatewayErrorCode::MalformedResponse, error.what());
        }
        if (!body.is_object()) {
            fail(GatewayErrorCode::Protocol, "JSON-RPC response is not an object");
        }
        const json::Value* jsonrpc = body.find("jsonrpc");
        if (jsonrpc == nullptr || !jsonrpc->is_string() || jsonrpc->as_string() != "2.0") {
            fail(GatewayErrorCode::Protocol, "gateway response is not JSON-RPC 2.0");
        }
        const json::Value* error = body.find("error");
        if (error != nullptr && !error->is_null()) {
            std::string detail = "gateway returned an error";
            if (error->is_object()) {
                const json::Value* message = error->find("message");
                if (message != nullptr && message->is_string()) {
                    detail = message->as_string();
                }
            }
            fail(GatewayErrorCode::GatewayRejected, detail);
        }
        const json::Value* result = body.find("result");
        if (result == nullptr) {
            fail(GatewayErrorCode::Protocol, "gateway response has neither result nor error");
        }
        return *result;
    } catch (...) {
        if (fd >= 0) {
            close(fd);
        }
        throw;
    }
}

} // namespace

const char* gateway_error_code_name(GatewayErrorCode code) noexcept
{
    switch (code) {
    case GatewayErrorCode::Transport: return "TRANSPORT";
    case GatewayErrorCode::Timeout: return "TIMEOUT";
    case GatewayErrorCode::Protocol: return "PROTOCOL";
    case GatewayErrorCode::MalformedResponse: return "MALFORMED_RESPONSE";
    case GatewayErrorCode::StaleJob: return "STALE_JOB";
    case GatewayErrorCode::InvalidCandidate: return "INVALID_CANDIDATE";
    case GatewayErrorCode::UnsupportedCertificateVersion:
        return "UNSUPPORTED_CERTIFICATE_VERSION";
    case GatewayErrorCode::GatewayRejected: return "GATEWAY_REJECTED";
    case GatewayErrorCode::ProverFailure: return "PROVER_FAILURE";
    case GatewayErrorCode::NodeRejected: return "NODE_REJECTED";
    case GatewayErrorCode::Shutdown: return "SHUTDOWN";
    }
    return "UNKNOWN";
}

MiningJobIdentity mining_job_identity(const MiningJob& job)
{
    return MiningJobIdentity{job.job_id,
                             job.incomplete_header_bytes,
                             job.target,
                             job.certificate_version};
}

bool same_mining_job_identity(const MiningJob& left, const MiningJob& right) noexcept
{
    return mining_job_identity(left) == mining_job_identity(right);
}

GatewayError::GatewayError(GatewayErrorCode code, const std::string& message)
    : std::runtime_error(message),
      code_(code)
{
}

std::vector<std::uint8_t> base64_decode_strict(std::string_view value)
{
    auto digit = [](char character) -> int {
        if (character >= 'A' && character <= 'Z') return character - 'A';
        if (character >= 'a' && character <= 'z') return character - 'a' + 26;
        if (character >= '0' && character <= '9') return character - '0' + 52;
        if (character == '+') return 62;
        if (character == '/') return 63;
        return -1;
    };
    if (value.empty()) {
        return {};
    }
    if (value.size() % 4U != 0U) {
        fail(GatewayErrorCode::MalformedResponse, "base64 length is not a multiple of four");
    }
    std::vector<std::uint8_t> result;
    result.reserve((value.size() / 4U) * 3U);
    for (std::size_t offset = 0U; offset < value.size(); offset += 4U) {
        const char a = value[offset];
        const char b = value[offset + 1U];
        const char c = value[offset + 2U];
        const char d = value[offset + 3U];
        const int va = digit(a);
        const int vb = digit(b);
        const int vc = c == '=' ? 0 : digit(c);
        const int vd = d == '=' ? 0 : digit(d);
        if (va < 0 || vb < 0 || vc < 0 || vd < 0
            || (c == '=' && d != '=')
            || (offset + 4U != value.size() && (c == '=' || d == '='))) {
            fail(GatewayErrorCode::MalformedResponse, "invalid base64 character or padding");
        }
        if (c == '=' && (vb & 0x0FU) != 0) {
            fail(GatewayErrorCode::MalformedResponse, "noncanonical base64 padding bits");
        }
        if (d == '=' && c != '=' && (vc & 0x03) != 0) {
            fail(GatewayErrorCode::MalformedResponse, "noncanonical base64 padding bits");
        }
        result.push_back(static_cast<std::uint8_t>((va << 2) | (vb >> 4)));
        if (c != '=') {
            result.push_back(static_cast<std::uint8_t>((vb << 4) | (vc >> 2)));
        }
        if (d != '=') {
            result.push_back(static_cast<std::uint8_t>((vc << 6) | vd));
        }
    }
    return result;
}

std::string base64_encode(std::span<const std::uint8_t> bytes)
{
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((bytes.size() + 2U) / 3U) * 4U);
    for (std::size_t offset = 0U; offset < bytes.size(); offset += 3U) {
        const std::size_t remaining = bytes.size() - offset;
        const std::uint32_t word = static_cast<std::uint32_t>(bytes[offset]) << 16U
            | (remaining > 1U ? static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U : 0U)
            | (remaining > 2U ? bytes[offset + 2U] : 0U);
        result.push_back(alphabet[(word >> 18U) & 0x3FU]);
        result.push_back(alphabet[(word >> 12U) & 0x3FU]);
        result.push_back(remaining > 1U ? alphabet[(word >> 6U) & 0x3FU] : '=');
        result.push_back(remaining > 2U ? alphabet[word & 0x3FU] : '=');
    }
    return result;
}

GatewayClient::GatewayClient(GatewayClientConfig config)
    : config_(std::move(config))
{
}

MiningJob GatewayClient::get_mining_info()
{
    ++request_id_;
    const json::Value result = rpc_call(config_, request_id_, "getMiningInfo", "{}");
    return parse_mining_job(result);
}

SubmissionResult GatewayClient::submit_plain_proof(const PlainProof& proof,
                                                   const MiningJob& job)
{
    const std::vector<std::uint8_t> serialized = proof.serialize();
    const std::string plain_proof = base64_encode(serialized);
    const std::string header = base64_encode(job.incomplete_header_bytes);
    const std::string params = "{\"plain_proof\":\"" + plain_proof
        + "\",\"mining_job\":{\"incomplete_header_bytes\":\"" + header
        + "\",\"target\":" + job.target_for_json()
        + ",\"cert_version\":"
        + std::to_string(certificate_version_number(job.certificate_version)) + "}}";
    ++request_id_;
    const json::Value result = rpc_call(config_, request_id_, "submitPlainProof", params);
    SubmissionResult submission;
    if (result.is_string()) {
        submission.detail = result.as_string();
        submission.accepted_by_gateway = submission.detail == "submitted";
    } else if (result.is_object()) {
        const json::Value* status = result.find("status");
        if (status != nullptr && status->is_string()) {
            submission.detail = status->as_string();
            submission.accepted_by_gateway = submission.detail == "submitted"
                || submission.detail == "accepted";
        }
    }
    if (submission.detail.empty()) {
        throw GatewayError(GatewayErrorCode::Protocol, "submitPlainProof result has no status");
    }
    if (!submission.accepted_by_gateway) {
        throw GatewayError(GatewayErrorCode::GatewayRejected,
                           "gateway did not accept PlainProof: " + submission.detail);
    }
    return submission;
}

SubmissionResult GatewayClient::submit_official_plain_proof(
    std::span<const std::uint8_t> official_wire,
    const MiningJob& job)
{
    const std::string plain_proof = base64_encode(official_wire);
    const std::string header = base64_encode(job.incomplete_header_bytes);
    const std::string params = "{\"plain_proof\":\"" + plain_proof
        + "\",\"mining_job\":{\"incomplete_header_bytes\":\"" + header
        + "\",\"target\":" + job.target_for_json()
        + ",\"cert_version\":"
        + std::to_string(certificate_version_number(job.certificate_version)) + "}}";
    ++request_id_;
    const json::Value result = rpc_call(config_, request_id_, "submitPlainProof", params);
    SubmissionResult submission;
    if (result.is_string()) {
        submission.detail = result.as_string();
        submission.accepted_by_gateway = submission.detail == "submitted";
    } else if (result.is_object()) {
        const json::Value* status = result.find("status");
        if (status != nullptr && status->is_string()) {
            submission.detail = status->as_string();
            submission.accepted_by_gateway = submission.detail == "submitted"
                || submission.detail == "accepted";
        }
    }
    if (submission.detail.empty()) {
        throw GatewayError(GatewayErrorCode::Protocol, "submitPlainProof result has no status");
    }
    if (!submission.accepted_by_gateway) {
        throw GatewayError(GatewayErrorCode::GatewayRejected,
                           "gateway did not accept official PlainProof: " + submission.detail);
    }
    return submission;
}

void GatewayClient::health_check()
{
    (void)get_mining_info();
}

} // namespace xdna::pearl
