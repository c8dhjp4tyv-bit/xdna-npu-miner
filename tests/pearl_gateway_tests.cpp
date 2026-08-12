#include "pearl/gateway.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include <sys/socket.h>
#include <sys/un.h>

namespace {

using namespace xdna::pearl;

void expect(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

[[nodiscard]] PeriodicPattern rows_pattern()
{
    return PeriodicPattern::from_indices(std::array<std::uint32_t, 2U>{0U, 8U});
}

[[nodiscard]] PeriodicPattern columns_pattern()
{
    std::array<std::uint32_t, kSelectedColumns> columns{};
    for (std::size_t index = 0U; index < columns.size(); ++index) {
        columns[index] = static_cast<std::uint32_t>((index / 2U) * 8U + (index % 2U));
    }
    return PeriodicPattern::from_indices(columns);
}

[[nodiscard]] Int8Matrix transpose(const Int8Matrix& matrix)
{
    std::vector<std::int8_t> values(matrix.rows() * matrix.cols(), 0);
    for (std::size_t row = 0U; row < matrix.rows(); ++row) {
        for (std::size_t column = 0U; column < matrix.cols(); ++column) {
            values[column * matrix.rows() + row] = matrix.at(row, column);
        }
    }
    return Int8Matrix(matrix.cols(), matrix.rows(), std::move(values));
}

[[nodiscard]] PlainProof make_fixture_proof()
{
    MiningConfiguration config;
    config.common_dim = 2048U;
    config.rank = 128U;
    config.rows_pattern = rows_pattern();
    config.cols_pattern = columns_pattern();
    IncompleteBlockHeader header;
    header.version = 1U;
    header.timestamp = 123U;
    header.nbits = 0x207FFFFFU;
    const Digest key = job_key(header, config);
    const Int8Matrix a(9U, 2048U, std::vector<std::int8_t>(9U * 2048U, 0));
    const Int8Matrix b(2048U, 250U, std::vector<std::int8_t>(2048U * 250U, 0));
    const Int8Matrix bt = transpose(b);
    const Digest hash_a = merkle_root(a.raw_bytes(), key);
    const Digest hash_b = merkle_root(bt.raw_bytes(), key);
    const CommitmentSeeds seeds = commitment_seeds(key, hash_a, hash_b);
    const std::vector<std::uint32_t> column_values = config.cols_pattern.indices();
    const std::vector<std::size_t> columns(column_values.begin(), column_values.end());
    const std::array<std::size_t, 2U> rows = {0U, 8U};
    const NoiseMatrices noise = generate_noise(2048U, 128U, seeds, rows, columns);
    const Int8Matrix selected_a(2U, 2048U, std::vector<std::int8_t>(2U * 2048U, 0));
    const Int8Matrix selected_b(2048U, 64U, std::vector<std::int8_t>(2048U * 64U, 0));
    const NoisedOperands operands = make_noised_operands(selected_a, selected_b, noise);
    const Int32Matrix product = gemm_checked(operands.a, operands.b);
    const TranscriptResult transcript = selected_transcript(
        operands.a, operands.b, product, 128U);
    const Digest target = [] {
        Digest value{};
        value.fill(0xFFU);
        return value;
    }();
    PlainProof proof;
    proof.header = header;
    proof.config = config;
    proof.header_config_key = key;
    proof.hash_a = hash_a;
    proof.hash_b = hash_b;
    proof.commitment_b = seeds.b_noise_seed;
    proof.commitment_a = seeds.a_noise_seed;
    proof.jackpot = jackpot_hash(transcript.words, proof.commitment_a);
    proof.target = target;
    proof.m = 9U;
    proof.n = 250U;
    proof.k = 2048U;
    proof.rank = 128U;
    proof.a_opening = open_matrix_rows(a, key, rows);
    proof.bt_opening = open_matrix_rows(bt, key, columns);
    proof.transcript = transcript;
    return proof;
}

class UnixResponder final {
public:
    explicit UnixResponder(std::string path,
                           bool malformed = false,
                           std::string certificate_version = "1",
                           bool omit_certificate_version = false)
        : path_(std::move(path)),
          malformed_(malformed),
          certificate_version_(std::move(certificate_version)),
          omit_certificate_version_(omit_certificate_version)
    {
        listener_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (listener_ < 0) {
            throw std::runtime_error("cannot create test Unix socket");
        }
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        if (path_.size() >= sizeof(address.sun_path)) {
            throw std::runtime_error("test Unix socket path too long");
        }
        std::copy(path_.begin(), path_.end(), address.sun_path);
        (void)unlink(path_.c_str());
        if (bind(listener_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0
            || listen(listener_, 4) != 0) {
            close(listener_);
            throw std::runtime_error("cannot bind test Unix socket");
        }
        thread_ = std::thread([this] { serve(); });
    }

    ~UnixResponder()
    {
        if (listener_ >= 0) {
            shutdown(listener_, SHUT_RDWR);
            close(listener_);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
        (void)unlink(path_.c_str());
    }

    [[nodiscard]] bool saw_get() const noexcept { return saw_get_; }
    [[nodiscard]] bool saw_submit() const noexcept { return saw_submit_; }

private:
    void serve()
    {
        for (unsigned request = 0U; request < (malformed_ ? 1U : 2U); ++request) {
            const int client = accept(listener_, nullptr, nullptr);
            if (client < 0) {
                return;
            }
            std::string line;
            std::array<char, 512U> buffer{};
            while (line.find('\n') == std::string::npos && line.size() < (1U << 20U)) {
                const ssize_t count = recv(client, buffer.data(), buffer.size(), 0);
                if (count <= 0) {
                    break;
                }
                line.append(buffer.data(), static_cast<std::size_t>(count));
            }
            std::string response;
            if (malformed_) {
                response = "not-json\n";
            } else if (line.find("getMiningInfo") != std::string::npos) {
                saw_get_ = true;
                const std::vector<std::uint8_t> header(kHeaderBytes, 0U);
                const std::string certificate_field = omit_certificate_version_
                    ? std::string{}
                    : ",\"cert_version\":" + certificate_version_;
                response = "{\"jsonrpc\":\"2.0\",\"result\":{\"incomplete_header_bytes\":\""
                    + base64_encode(header)
                    + "\",\"target\":115792089237316195423570985008687907853269984665640564039457584007913129639935"
                    + certificate_field + ",\"ignored\":true},\"id\":1}\n";
            } else if (line.find("submitPlainProof") != std::string::npos) {
                saw_submit_ = true;
                response = "{\"jsonrpc\":\"2.0\",\"result\":\"submitted\",\"id\":2}\n";
            } else {
                response = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"unknown\"},\"id\":null}\n";
            }
            (void)send(client, response.data(), response.size(), MSG_NOSIGNAL);
            shutdown(client, SHUT_RDWR);
            close(client);
        }
    }

    std::string path_;
    bool malformed_ = false;
    std::string certificate_version_;
    bool omit_certificate_version_ = false;
    int listener_ = -1;
    std::thread thread_;
    std::atomic<bool> saw_get_{false};
    std::atomic<bool> saw_submit_{false};
};

} // namespace

int main()
{
    try {
        const std::vector<std::uint8_t> bytes = {0U, 1U, 2U, 253U, 254U, 255U};
        expect(base64_decode_strict(base64_encode(bytes)) == bytes, "base64 round trip");
        bool bad_base64 = false;
        try {
            (void)base64_decode_strict("ab=c");
        } catch (const GatewayError& error) {
            bad_base64 = error.code() == GatewayErrorCode::MalformedResponse;
        }
        expect(bad_base64, "noncanonical base64 is rejected");

        const std::string socket_path = "/tmp/pearl-gateway-test-" + std::to_string(getpid()) + ".sock";
        UnixResponder responder(socket_path);
        GatewayClientConfig config;
        config.endpoint.transport = GatewayTransport::Unix;
        config.endpoint.unix_path = socket_path;
        config.timeout = std::chrono::milliseconds(2000);
        GatewayClient client(config);
        const MiningJob job = client.get_mining_info();
        expect(job.incomplete_header_bytes.size() == kHeaderBytes, "header decoded");
        expect(job.certificate_version == CertificateVersion::V1, "certificate version decoded");
        const Digest max_target = [] {
            Digest value{};
            value.fill(0xFFU);
            return value;
        }();
        expect(job.target == max_target, "256-bit decimal target decoded");
        (void)client.submit_plain_proof(make_fixture_proof(), job);
        expect(responder.saw_get() && responder.saw_submit(), "both gateway methods framed");
        MiningJob v3_job = job;
        v3_job.certificate_version = CertificateVersion::V3;
        bool legacy_v3_rejected = false;
        try {
            (void)client.submit_plain_proof(make_fixture_proof(), v3_job);
        } catch (const GatewayError& error) {
            legacy_v3_rejected = error.code() == GatewayErrorCode::InvalidCandidate;
        }
        expect(legacy_v3_rejected,
               "V3 cannot silently use the historical project PlainProof envelope");

        const std::string malformed_path = "/tmp/pearl-gateway-malformed-" + std::to_string(getpid()) + ".sock";
        UnixResponder malformed(malformed_path, true);
        GatewayClientConfig malformed_config = config;
        malformed_config.endpoint.unix_path = malformed_path;
        bool malformed_seen = false;
        try {
            (void)GatewayClient(malformed_config).get_mining_info();
        } catch (const GatewayError& error) {
            malformed_seen = error.code() == GatewayErrorCode::MalformedResponse;
        }
        expect(malformed_seen, "malformed JSON is categorized");

        const std::string unsupported_path = "/tmp/pearl-gateway-unsupported-"
            + std::to_string(getpid()) + ".sock";
        UnixResponder unsupported(unsupported_path, false, "4");
        GatewayClientConfig unsupported_config = config;
        unsupported_config.endpoint.unix_path = unsupported_path;
        bool unsupported_seen = false;
        try {
            (void)GatewayClient(unsupported_config).get_mining_info();
        } catch (const GatewayError& error) {
            unsupported_seen = error.code() == GatewayErrorCode::UnsupportedCertificateVersion
                && std::string(error.what()) == "UNSUPPORTED_CERTIFICATE_VERSION_4";
        }
        expect(unsupported_seen, "unknown certificate versions fail closed with a stable code");

        const std::string missing_cert_path = "/tmp/pearl-gateway-missing-cert-"
            + std::to_string(getpid()) + ".sock";
        UnixResponder missing_cert(missing_cert_path, false, "1", true);
        GatewayClientConfig missing_cert_config = config;
        missing_cert_config.endpoint.unix_path = missing_cert_path;
        bool missing_cert_seen = false;
        try {
            (void)GatewayClient(missing_cert_config).get_mining_info();
        } catch (const GatewayError& error) {
            missing_cert_seen = error.code() == GatewayErrorCode::MalformedResponse;
        }
        expect(missing_cert_seen, "missing certificate version is rejected");

        GatewayClientConfig unsafe = config;
        unsafe.endpoint.transport = GatewayTransport::LoopbackTcp;
        unsafe.endpoint.host = "192.0.2.1";
        bool unsafe_seen = false;
        try {
            GatewayClient(unsafe).health_check();
        } catch (const GatewayError& error) {
            unsafe_seen = error.code() == GatewayErrorCode::Transport;
        }
        expect(unsafe_seen, "non-loopback gateway is rejected");
        std::cout << "pearl gateway protocol tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "pearl gateway test failure: " << error.what() << '\n';
        return 1;
    }
}
