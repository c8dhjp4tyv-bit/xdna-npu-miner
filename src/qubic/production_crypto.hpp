#pragma once

#include "qubic/direct_node.hpp"

#include <string>
#include <span>
#include <string_view>

namespace xdna::qubic {

// This provider treats SigningSecret as the 32-byte Qubic signing subseed.
// FourQ private scalars are derived by K12(subseed), matching the protocol
// boundary without exposing a raw private scalar to the caller.
class K12FourQCryptoProvider final : public CryptoProvider {
public:
    [[nodiscard]] CryptoProviderInfo info() const override;

    [[nodiscard]] bool hash(std::span<const Byte> input,
                            std::span<Byte> output) const noexcept override;

    [[nodiscard]] bool sign(const SigningMaterial& material,
                            std::span<const Byte> digest,
                            std::span<Byte> signature) const noexcept override;

    [[nodiscard]] bool derive_shared_key(const SigningMaterial& material,
                                         const PublicKey& peer,
                                         std::span<Byte> shared_key) const noexcept override;

    [[nodiscard]] bool derive_public_key(std::span<const Byte> signing_subseed,
                                         PublicKey& public_key) const noexcept;

    [[nodiscard]] bool verify(const PublicKey& public_key,
                              std::span<const Byte> digest,
                              std::span<const Byte> signature) const noexcept;
};

// Clean-room implementation of the observable 60-character Qubic identity
// encoding. It uses this provider's K12 implementation only for the public
// three-byte checksum; it never exposes or accepts a private scalar.
[[nodiscard]] std::string public_identity_from_public_key(const PublicKey& public_key);
[[nodiscard]] bool public_key_from_identity(std::string_view identity,
                                            PublicKey& public_key) noexcept;

} // namespace xdna::qubic
