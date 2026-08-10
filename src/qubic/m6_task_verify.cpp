#include "bpp9000/task.hpp"
#include "qubic/production_crypto.hpp"

#include <cstddef>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using xdna::bpp9000::BlockDigestProvider;
using xdna::bpp9000::Byte;
using xdna::bpp9000::Digest32;
using xdna::bpp9000::Task;
using xdna::qubic::K12FourQCryptoProvider;

constexpr std::size_t kExpectedTaskBytes = 44744U;

class K12TaskDigest final : public BlockDigestProvider {
public:
    [[nodiscard]] Digest32 digest(std::span<const Byte> block) const override
    {
        Digest32 result{};
        K12FourQCryptoProvider provider;
        if (!provider.hash(block, std::span<Byte>(result.bytes))) {
            throw std::runtime_error("K12 task digest failed");
        }
        return result;
    }
};

[[nodiscard]] std::string digest_hex(const Digest32& digest)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(digest.bytes.size() * 2U);
    for (const Byte value : digest.bytes) {
        result.push_back(digits[value >> 4U]);
        result.push_back(digits[value & 0x0FU]);
    }
    return result;
}

[[nodiscard]] std::vector<Byte> read_file(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open the pinned task cache");
    }
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 || static_cast<std::size_t>(size) != kExpectedTaskBytes) {
        throw std::runtime_error("pinned task cache has an unexpected size");
    }
    input.seekg(0, std::ios::beg);
    std::vector<Byte> bytes(kExpectedTaskBytes, 0U);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
        throw std::runtime_error("pinned task cache could not be read completely");
    }
    return bytes;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 3 || std::string_view(argv[1]) != "--path") {
        std::cerr << "usage: " << argv[0] << " --path PATH\n";
        return 64;
    }
    try {
        const std::vector<Byte> bytes = read_file(argv[2]);
        K12TaskDigest digest;
        const Task task = xdna::bpp9000::parse_production_task(bytes, digest);
        if (task.header.shape != xdna::bpp9000::production_shape()) {
            throw std::runtime_error("pinned task shape is not the production BPP9000 shape");
        }
        std::cout << "task_verified=true\n";
        std::cout << "task_bytes=" << bytes.size() << '\n';
        std::cout << "input_trits=" << task.header.shape.input_trits << '\n';
        std::cout << "output_trits=" << task.header.shape.output_trits << '\n';
        std::cout << "sequence_length=" << task.header.shape.sequence_length << '\n';
        std::cout << "window_width=" << xdna::bpp9000::kProductionWindowWidth << '\n';
        std::cout << "number_of_windows="
                  << task.number_of_windows(xdna::bpp9000::kProductionWindowWidth) << '\n';
        std::cout << "topology_hash_hex=" << digest_hex(task.header.topology_hash) << '\n';
        std::cout << "data_hash_hex=" << digest_hex(task.header.data_hash) << '\n';
        std::cout << "secret_output=never\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "m6_task_verify_error=" << error.what() << '\n';
        return 2;
    }
}
