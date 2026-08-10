#include "qubic/local_identity.hpp"
#include "qubic/production_crypto.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

namespace {

using xdna::bpp9000::Byte;
using xdna::qubic::K12FourQCryptoProvider;
using xdna::qubic::PublicKey;
using xdna::qubic::SigningSecret;

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

[[nodiscard]] int usage(const char* program)
{
    std::cerr << "usage: " << program
              << " <generate|show|erase> --path PATH [--replace]\n";
    return 64;
}

[[nodiscard]] bool parse_arguments(int argc,
                                   char** argv,
                                   std::string_view& command,
                                   std::filesystem::path& path,
                                   bool& replace)
{
    if (argc < 3) {
        return false;
    }
    command = argv[1];
    for (int index = 2; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--path" && index + 1 < argc) {
            path = argv[++index];
        } else if (argument == "--replace") {
            replace = true;
        } else if (argument == "--help") {
            return false;
        } else {
            return false;
        }
    }
    return !path.empty() && (command == "generate" || command == "show" || command == "erase")
        && (command == "generate" || !replace);
}

[[nodiscard]] bool derive_and_print(const SigningSecret& secret)
{
    K12FourQCryptoProvider provider;
    PublicKey public_key{};
    if (!provider.derive_public_key(std::span<const Byte>(secret.bytes), public_key)) {
        std::cerr << "cannot derive the public identity from the local subseed\n";
        return false;
    }
    const std::string identity = xdna::qubic::public_identity_from_public_key(public_key);
    if (identity.empty()) {
        std::cerr << "cannot encode the public identity\n";
        return false;
    }
    std::cout << "identity=" << identity << '\n';
    std::cout << "public_key_hex=" << public_key_hex(public_key) << '\n';
    std::cout << "message_dissemination_threshold=1000000000\n";
    std::cout << "secret_output=never\n";
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    std::string_view command;
    std::filesystem::path path;
    bool replace = false;
    if (!parse_arguments(argc, argv, command, path, replace)) {
        return usage(argv[0]);
    }

    std::string error;
    if (command == "erase") {
        if (!xdna::qubic::erase_signing_subseed_file(path, error)) {
            std::cerr << error << '\n';
            return 2;
        }
        std::cout << "erased=true\n";
        std::cout << "secret_output=never\n";
        return 0;
    }

    SigningSecret secret;
    if (command == "generate") {
        if (!xdna::qubic::create_signing_subseed_file(path, secret, replace, error)) {
            std::cerr << error << '\n';
            return 2;
        }
    } else if (!xdna::qubic::load_signing_subseed_file(path, secret, error)) {
        std::cerr << error << '\n';
        return 2;
    }

    return derive_and_print(secret) ? 0 : 3;
}
