#include "qubic/production_crypto.hpp"

extern "C" {
#include <FourQ_api.h>
#include <KangarooTwelve.h>
}

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

namespace xdna::qubic {

namespace {

constexpr std::size_t kSecretBytes = 32U;
constexpr std::size_t kDigestBytes = 32U;
constexpr std::size_t kProviderSignatureBytes = 64U;

[[nodiscard]] bool all_zero(std::span<const Byte> bytes) noexcept
{
    return std::all_of(bytes.begin(), bytes.end(), [](const Byte value) { return value == 0U; });
}

[[nodiscard]] bool valid_hash_arguments(std::span<const Byte> input,
                                        std::span<Byte> output) noexcept
{
    if (output.empty() || output.data() == nullptr) {
        return false;
    }

    if (input.data() == nullptr && !input.empty()) {
        return false;
    }

    return input.size() <= static_cast<std::size_t>(std::numeric_limits<unsigned long long>::max());
}

[[nodiscard]] bool k12_hash(std::span<const Byte> input,
                            std::span<Byte> output) noexcept
{
    if (!valid_hash_arguments(input, output)) {
        return false;
    }

    static constexpr Byte empty_input = 0U;
    const Byte* input_data = input.empty() ? &empty_input : input.data();
    return KT128(input_data,
                 input.size(),
                 output.data(),
                 output.size(),
                 nullptr,
                 0U) == 0;
}

[[nodiscard]] bool derive_private_scalar(std::span<const Byte> signing_subseed,
                                         std::array<Byte, kSecretBytes>& private_scalar) noexcept
{
    if (signing_subseed.size() != kSecretBytes || all_zero(signing_subseed)) {
        return false;
    }

    return k12_hash(signing_subseed, std::span<Byte>(private_scalar));
}

[[nodiscard]] bool public_key_matches_secret(const SigningMaterial& material) noexcept
{
    PublicKey derived{};
    K12FourQCryptoProvider provider;
    return provider.derive_public_key(std::span<const Byte>(material.secret.bytes), derived)
        && derived == material.public_key;
}

} // namespace

// FourQlib's SchnorrQ implementation calls this symbol for every hash. The
// protocol requires K12 rather than FourQlib's default SHA-512, so the
// provider supplies the documented hash hook without modifying FourQlib.
extern "C" int crypto_sha512(const unsigned char* input,
                             unsigned long long input_length,
                             unsigned char* output)
{
    if (output == nullptr) {
        return 1;
    }

    static constexpr Byte empty_input = 0U;
    const Byte* input_data = input_length == 0U ? &empty_input : input;
    if (input_data == nullptr
        || input_length > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
        return 1;
    }

    const std::span<const Byte> input_span(input_data, static_cast<std::size_t>(input_length));
    const std::span<Byte> output_span(output, 64U);
    return k12_hash(input_span, output_span) ? 0 : 1;
}

CryptoProviderInfo K12FourQCryptoProvider::info() const
{
    return CryptoProviderInfo{
        "FourQlib v3.1@1031567f + K12@f95b0b73 (pinned external sources)",
        "MIT (FourQlib) + CC0/public-domain dedication (K12 selected files)",
        true,
    };
}

bool K12FourQCryptoProvider::hash(std::span<const Byte> input,
                                  std::span<Byte> output) const noexcept
{
    return k12_hash(input, output);
}

bool K12FourQCryptoProvider::derive_public_key(std::span<const Byte> signing_subseed,
                                               PublicKey& public_key) const noexcept
{
    std::array<Byte, kSecretBytes> private_scalar{};
    if (!derive_private_scalar(signing_subseed, private_scalar)) {
        public_key.bytes.fill(0U);
        return false;
    }

    const ECCRYPTO_STATUS status = CompressedPublicKeyGeneration(private_scalar.data(),
                                                                  public_key.bytes.data());
    std::fill(private_scalar.begin(), private_scalar.end(), 0U);
    if (status != ECCRYPTO_SUCCESS) {
        public_key.bytes.fill(0U);
        return false;
    }

    return !public_key.is_zero();
}

bool K12FourQCryptoProvider::sign(const SigningMaterial& material,
                                  std::span<const Byte> digest,
                                  std::span<Byte> signature) const noexcept
{
    if (digest.size() != kDigestBytes || signature.size() != kProviderSignatureBytes
        || !public_key_matches_secret(material)) {
        return false;
    }

    return SchnorrQ_Sign(material.secret.bytes.data(),
                         material.public_key.bytes.data(),
                         digest.data(),
                         static_cast<unsigned int>(digest.size()),
                         signature.data()) == ECCRYPTO_SUCCESS;
}

bool K12FourQCryptoProvider::verify(const PublicKey& public_key,
                                    std::span<const Byte> digest,
                                    std::span<const Byte> signature) const noexcept
{
    if (public_key.is_zero() || digest.size() != kDigestBytes
        || signature.size() != kProviderSignatureBytes) {
        return false;
    }

    unsigned int valid = 0U;
    const ECCRYPTO_STATUS status = SchnorrQ_Verify(public_key.bytes.data(),
                                                    digest.data(),
                                                    static_cast<unsigned int>(digest.size()),
                                                    signature.data(),
                                                    &valid);
    return status == ECCRYPTO_SUCCESS && valid != 0U;
}

bool K12FourQCryptoProvider::derive_shared_key(const SigningMaterial& material,
                                               const PublicKey& peer,
                                               std::span<Byte> shared_key) const noexcept
{
    if (shared_key.size() != kSecretBytes || peer.is_zero()
        || !public_key_matches_secret(material)) {
        return false;
    }

    std::array<Byte, kSecretBytes> private_scalar{};
    if (!derive_private_scalar(std::span<const Byte>(material.secret.bytes), private_scalar)) {
        return false;
    }

    const ECCRYPTO_STATUS status = CompressedSecretAgreement(private_scalar.data(),
                                                              peer.bytes.data(),
                                                              shared_key.data());
    std::fill(private_scalar.begin(), private_scalar.end(), 0U);
    if (status != ECCRYPTO_SUCCESS) {
        std::fill(shared_key.begin(), shared_key.end(), 0U);
        return false;
    }

    return !all_zero(std::span<const Byte>(shared_key));
}

std::string public_identity_from_public_key(const PublicKey& public_key)
{
    if (public_key.is_zero()) {
        return {};
    }

    constexpr std::size_t kPublicKeyChunks = 4U;
    constexpr std::size_t kChunkBytes = 8U;
    constexpr std::size_t kChunkCharacters = 14U;
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string identity;
    identity.reserve(60U);
    for (std::size_t chunk = 0U; chunk < kPublicKeyChunks; ++chunk) {
        std::uint64_t value = 0U;
        std::memcpy(&value,
                    public_key.bytes.data() + chunk * kChunkBytes,
                    sizeof(value));
        for (std::size_t character = 0U; character < kChunkCharacters; ++character) {
            identity.push_back(alphabet[value % 26U]);
            value /= 26U;
        }
    }

    K12FourQCryptoProvider provider;
    std::array<Byte, 3U> checksum_bytes{};
    if (!provider.hash(std::span<const Byte>(public_key.bytes),
                       std::span<Byte>(checksum_bytes))) {
        return {};
    }
    std::uint32_t checksum = static_cast<std::uint32_t>(checksum_bytes[0U])
        | (static_cast<std::uint32_t>(checksum_bytes[1U]) << 8U)
        | (static_cast<std::uint32_t>(checksum_bytes[2U]) << 16U);
    checksum &= 0x3FFFFU;
    for (std::size_t character = 0U; character < 4U; ++character) {
        identity.push_back(alphabet[checksum % 26U]);
        checksum /= 26U;
    }
    return identity;
}

bool public_key_from_identity(const std::string_view identity,
                              PublicKey& public_key) noexcept
{
    constexpr std::size_t kPublicKeyChunks = 4U;
    constexpr std::size_t kChunkBytes = 8U;
    constexpr std::size_t kChunkCharacters = 14U;
    if (identity.size() != 60U) {
        public_key.bytes.fill(0U);
        return false;
    }

    std::array<Byte, 32U> decoded{};
    for (std::size_t chunk = 0U; chunk < kPublicKeyChunks; ++chunk) {
        std::uint64_t value = 0U;
        const std::size_t start = chunk * kChunkCharacters;
        for (std::size_t position = kChunkCharacters; position > 0U; --position) {
            const char character = identity[start + position - 1U];
            if (character < 'A' || character > 'Z') {
                public_key.bytes.fill(0U);
                return false;
            }
            const std::uint64_t digit = static_cast<std::uint64_t>(character - 'A');
            if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 26U) {
                public_key.bytes.fill(0U);
                return false;
            }
            value = value * 26U + digit;
        }
        std::memcpy(decoded.data() + chunk * kChunkBytes, &value, sizeof(value));
    }

    std::uint32_t supplied_checksum = 0U;
    for (std::size_t position = 60U; position > 56U; --position) {
        const char character = identity[position - 1U];
        if (character < 'A' || character > 'Z') {
            public_key.bytes.fill(0U);
            return false;
        }
        supplied_checksum = supplied_checksum * 26U
            + static_cast<std::uint32_t>(character - 'A');
    }
    supplied_checksum &= 0x3FFFFU;

    K12FourQCryptoProvider provider;
    std::array<Byte, 3U> checksum_bytes{};
    if (!provider.hash(std::span<const Byte>(decoded), std::span<Byte>(checksum_bytes))) {
        public_key.bytes.fill(0U);
        return false;
    }
    const std::uint32_t expected_checksum = (static_cast<std::uint32_t>(checksum_bytes[0U])
                                              | (static_cast<std::uint32_t>(checksum_bytes[1U]) << 8U)
                                              | (static_cast<std::uint32_t>(checksum_bytes[2U]) << 16U))
        & 0x3FFFFU;
    if (supplied_checksum != expected_checksum) {
        public_key.bytes.fill(0U);
        return false;
    }

    std::copy(decoded.begin(), decoded.end(), public_key.bytes.begin());
    return !public_key.is_zero();
}

} // namespace xdna::qubic
