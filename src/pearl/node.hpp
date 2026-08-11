#pragma once

#include "pearl/json.hpp"

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace xdna::pearl {

enum class NodeErrorCode : std::uint8_t {
    Transport,
    Timeout,
    Protocol,
    MalformedResponse,
    NodeRejected,
};

class NodeError final : public std::runtime_error {
public:
    NodeError(NodeErrorCode code, const std::string& message);

    [[nodiscard]] NodeErrorCode code() const noexcept
    {
        return code_;
    }

private:
    NodeErrorCode code_;
};

struct NodeClientConfig {
    std::string rpc_url = "http://127.0.0.1:8332/";
    std::string rpc_user;
    std::string rpc_password;
    std::chrono::milliseconds timeout{5000};
    std::size_t max_response_bytes = 4U << 20U;
};

struct BlockTemplateSummary {
    std::uint32_t version = 0U;
    std::uint32_t height = 0U;
    std::uint32_t curtime = 0U;
    std::string previous_block_hash;
    std::string bits;
    std::string target;
    std::uint32_t required_certificate_version = 0U;
    json::Value raw;
};

class NodeClient final {
public:
    explicit NodeClient(NodeClientConfig config = {});

    [[nodiscard]] BlockTemplateSummary get_block_template();
    [[nodiscard]] std::string submit_block(std::string_view block_hex);

private:
    [[nodiscard]] json::Value call(std::string_view method, std::string_view params_json);

    NodeClientConfig config_;
    std::uint64_t request_id_ = 0U;
};

} // namespace xdna::pearl
