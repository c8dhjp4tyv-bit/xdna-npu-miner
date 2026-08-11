#include "pearl/node.hpp"

#include <algorithm>
#include <charconv>
#include <curl/curl.h>
#include <string_view>

namespace xdna::pearl {
namespace {

struct ResponseBuffer {
    std::string data;
    std::size_t limit = 0U;
    bool over_limit = false;
};

std::size_t write_response(char* pointer,
                           std::size_t size,
                           std::size_t count,
                           void* opaque)
{
    auto& buffer = *static_cast<ResponseBuffer*>(opaque);
    const std::size_t bytes = size * count;
    if (bytes > buffer.limit - std::min(buffer.limit, buffer.data.size())) {
        buffer.over_limit = true;
        return 0U;
    }
    buffer.data.append(pointer, bytes);
    return bytes;
}

[[noreturn]] void fail(NodeErrorCode code, const std::string& message)
{
    throw NodeError(code, message);
}

[[nodiscard]] std::uint32_t number_u32(const json::Value& object,
                                       std::string_view key)
{
    const json::Value* value = object.find(key);
    if (value == nullptr || !value->is_number()) {
        fail(NodeErrorCode::MalformedResponse, "node field is not an integer: " + std::string(key));
    }
    std::uint32_t result = 0U;
    const auto text = value->as_number().value;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        fail(NodeErrorCode::MalformedResponse, "node integer is outside u32 range: " + std::string(key));
    }
    return result;
}

[[nodiscard]] std::string required_string(const json::Value& object, std::string_view key)
{
    const json::Value* value = object.find(key);
    if (value == nullptr || !value->is_string()) {
        fail(NodeErrorCode::MalformedResponse, "node string field is missing: " + std::string(key));
    }
    return value->as_string();
}

void validate_hex(std::string_view value, std::size_t length, const char* label)
{
    if (value.size() != length) {
        fail(NodeErrorCode::MalformedResponse, std::string(label) + " has an invalid hex length");
    }
    for (const char character : value) {
        if (!((character >= '0' && character <= '9')
              || (character >= 'a' && character <= 'f')
              || (character >= 'A' && character <= 'F'))) {
            fail(NodeErrorCode::MalformedResponse, std::string(label) + " contains non-hex data");
        }
    }
}

} // namespace

NodeError::NodeError(NodeErrorCode code, const std::string& message)
    : std::runtime_error(message),
      code_(code)
{
}

NodeClient::NodeClient(NodeClientConfig config)
    : config_(std::move(config))
{
}

json::Value NodeClient::call(std::string_view method, std::string_view params_json)
{
    if (config_.rpc_url.empty() || config_.timeout <= std::chrono::milliseconds::zero()) {
        fail(NodeErrorCode::Protocol, "node RPC URL and positive timeout are required");
    }
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        fail(NodeErrorCode::Transport, "cannot initialize node HTTP client");
    }
    const std::string payload = "{\"jsonrpc\":\"2.0\",\"method\":\""
        + std::string(method) + "\",\"params\":" + std::string(params_json)
        + ",\"id\":" + std::to_string(++request_id_) + "}";
    ResponseBuffer response{"", config_.max_response_bytes, false};
    curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, config_.rpc_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(config_.timeout.count()));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(config_.timeout.count()));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    if (!config_.rpc_user.empty() || !config_.rpc_password.empty()) {
        const std::string credentials = config_.rpc_user + ":" + config_.rpc_password;
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        curl_easy_setopt(curl, CURLOPT_USERPWD, credentials.c_str());
    }
    const CURLcode result = curl_easy_perform(curl);
    long response_code = 0L;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK) {
        if (result == CURLE_OPERATION_TIMEDOUT) {
            fail(NodeErrorCode::Timeout, "node RPC request timed out");
        }
        if (response.over_limit || result == CURLE_WRITE_ERROR) {
            fail(NodeErrorCode::Protocol, "node RPC response exceeds configured size limit");
        }
        fail(NodeErrorCode::Transport, std::string("node RPC transport failed: ")
             + curl_easy_strerror(result));
    }
    if (response_code < 200L || response_code >= 300L) {
        fail(NodeErrorCode::Transport, "node RPC returned HTTP status " + std::to_string(response_code));
    }
    json::Value body;
    try {
        body = json::parse(response.data, config_.max_response_bytes);
    } catch (const std::exception& error) {
        fail(NodeErrorCode::MalformedResponse, error.what());
    }
    if (!body.is_object()) {
        fail(NodeErrorCode::Protocol, "node RPC response is not an object");
    }
    const json::Value* error = body.find("error");
    if (error != nullptr && !error->is_null()) {
        fail(NodeErrorCode::NodeRejected, "node RPC returned an error");
    }
    const json::Value* value = body.find("result");
    if (value == nullptr) {
        fail(NodeErrorCode::Protocol, "node RPC response has no result");
    }
    return *value;
}

BlockTemplateSummary NodeClient::get_block_template()
{
    const json::Value result = call(
        "getblocktemplate",
        "[{\"capabilities\":[\"coinbasevalue\",\"workid\",\"coinbase/append\"],\"rules\":[\"segwit\"]}]"
    );
    if (!result.is_object()) {
        fail(NodeErrorCode::MalformedResponse, "getblocktemplate result is not an object");
    }
    BlockTemplateSummary summary;
    summary.version = number_u32(result, "version");
    summary.height = number_u32(result, "height");
    summary.curtime = number_u32(result, "curtime");
    summary.previous_block_hash = required_string(result, "previousblockhash");
    summary.bits = required_string(result, "bits");
    summary.target = required_string(result, "target");
    const json::Value* cert = result.find("requiredcertversion");
    if (cert != nullptr && cert->is_number()) {
        summary.required_certificate_version = number_u32(result, "requiredcertversion");
    }
    validate_hex(summary.previous_block_hash, 64U, "previousblockhash");
    validate_hex(summary.bits, 8U, "bits");
    validate_hex(summary.target, 64U, "target");
    summary.raw = result;
    return summary;
}

std::string NodeClient::submit_block(std::string_view block_hex)
{
    validate_hex(block_hex, block_hex.size(), "block");
    if (block_hex.empty() || (block_hex.size() % 2U) != 0U) {
        fail(NodeErrorCode::Protocol, "block hex is empty or has odd length");
    }
    const json::Value result = call("submitblock", "[\"" + std::string(block_hex) + "\"]");
    if (result.is_null()) {
        return "accepted";
    }
    if (result.is_string()) {
        throw NodeError(NodeErrorCode::NodeRejected, "node rejected block: " + result.as_string());
    }
    fail(NodeErrorCode::Protocol, "submitblock result has an unsupported type");
}

} // namespace xdna::pearl
