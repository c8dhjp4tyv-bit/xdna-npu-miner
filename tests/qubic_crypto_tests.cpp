#include "qubic/production_crypto.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using xdna::bpp9000::Byte;
using xdna::bpp9000::ScoreResult;
using xdna::bpp9000::ScoreStatus;
using xdna::qubic::Algorithm;
using xdna::qubic::build_solution;
using xdna::qubic::CandidateSolution;
using xdna::qubic::Frame;
using xdna::qubic::K12FourQCryptoProvider;
using xdna::qubic::PublicKey;
using xdna::qubic::ScoreEvidence;
using xdna::qubic::SigningMaterial;
using xdna::qubic::SubmissionInput;
using xdna::qubic::TaskIdentity;
using xdna::qubic::WorkContext;

int failures = 0;

[[nodiscard]] Byte hex_digit(const char value)
{
    if (value >= '0' && value <= '9') {
        return static_cast<Byte>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<Byte>(value - 'a' + 10);
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<Byte>(value - 'A' + 10);
    }
    throw std::runtime_error("invalid hexadecimal test vector");
}

template <std::size_t N>
[[nodiscard]] std::array<Byte, N> from_hex(const std::string_view value)
{
    if (value.size() != N * 2U) {
        throw std::runtime_error("wrong hexadecimal test vector length");
    }

    std::array<Byte, N> result{};
    for (std::size_t index = 0U; index < N; ++index) {
        result[index] = static_cast<Byte>((hex_digit(value[index * 2U]) << 4U)
                                          | hex_digit(value[index * 2U + 1U]));
    }
    return result;
}

[[nodiscard]] std::string to_hex(const std::span<const Byte> value)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() * 2U);
    for (const Byte byte : value) {
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0FU]);
    }
    return result;
}

void expect(const bool condition, const std::string_view name)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

template <std::size_t N>
void expect_bytes(const std::string_view name,
                  const std::span<const Byte> actual,
                  const std::string_view expected)
{
    const std::string actual_hex = to_hex(actual);
    if (actual_hex != expected) {
        ++failures;
        std::cerr << "FAIL: " << name << "\n  expected: " << expected
                  << "\n  actual:   " << actual_hex << '\n';
    }
    expect(actual.size() == N, name);
}

} // namespace

int main()
{
    try {
        K12FourQCryptoProvider provider;
        const auto provider_info = provider.info();
        expect(provider_info.production_ready, "provider reports production readiness");
        expect(provider_info.provider.find("FourQlib") != std::string::npos,
               "provider identifies FourQlib");
        expect(provider_info.license.find("MIT") != std::string::npos,
               "provider reports FourQlib license");

        // RFC 9861 KangarooTwelve vectors: empty message and customization.
        const auto expected_empty_32 = from_hex<32U>(
            "1ac2d450fc3b4205d19da7bfca1b37513c0803577ac7167f06fe2ce1f0ef39e5");
        const auto expected_empty_64 = from_hex<64U>(
            "1ac2d450fc3b4205d19da7bfca1b37513c0803577ac7167f06fe2ce1f0ef39e"
            "54269c056b8c82e48276038b6d292966cc07a3d4645272e31ff38508139eb0a71");
        const auto expected_one_32 = from_hex<32U>(
            "2bda92450e8b147f8a7cb629e784a058efca7cf7d8218e02d345dfaa65244a1f");
        const std::array<Byte, 1U> one{0U};

        std::array<Byte, 32U> empty_32{};
        std::array<Byte, 64U> empty_64{};
        std::array<Byte, 32U> one_32{};
        expect(provider.hash({}, std::span<Byte>(empty_32)), "K12 empty 32 succeeds");
        expect(provider.hash({}, std::span<Byte>(empty_64)), "K12 empty 64 succeeds");
        expect(provider.hash(std::span<const Byte>(one), std::span<Byte>(one_32)),
               "K12 one-byte vector succeeds");
        expect_bytes<32U>("K12 empty 32", empty_32, to_hex(expected_empty_32));
        expect_bytes<64U>("K12 empty 64", empty_64, to_hex(expected_empty_64));
        expect_bytes<32U>("K12 one-byte vector", one_32, to_hex(expected_one_32));
        expect(!provider.hash({}, {}), "K12 rejects empty output");

        // Synthetic, non-user key material. This is the first Qubic
        // SchnorrQ compatibility vector from the upstream test corpus.
        const auto signing_subseed = from_hex<32U>(
            "4ac19e2bf0d3776519aabe31924f7dc2589b3d0e7411a65f84c9b16df72c038e");
        const auto signing_digest = from_hex<32U>(
            "94e120a4d3f58c217a53eb9046d9f2c5b11288a9fe340d6ce5a771cf04b82e63");
        const auto expected_signature = from_hex<64U>(
            "357d47b1366f33eed311a4458ec7326d35728e9292328a9b7ff8d4ec0f7b0df9"
            "323f5d1cd01bd5a380a1a8e4f29ad3ae9c5d94e84f4181a61ca73030d6d11600");

        SigningMaterial signing_material;
        signing_material.secret.bytes = signing_subseed;
        PublicKey signing_public_key{};
        expect(provider.derive_public_key(std::span<const Byte>(signing_subseed),
                                          signing_public_key),
               "derive Qubic public key");
        expect_bytes<32U>("Qubic public-key KAT",
                          signing_public_key.bytes,
                          "f6ef25559eb0ebf767e9a1d30db7e75680adf0a1de527ed0d249a09794c880d6");
        const std::string signing_identity =
            xdna::qubic::public_identity_from_public_key(signing_public_key);
        expect(signing_identity.size() == 60U, "Qubic public identity has 60 characters");
        PublicKey decoded_identity_key{};
        expect(xdna::qubic::public_key_from_identity(signing_identity, decoded_identity_key)
                   && decoded_identity_key == signing_public_key,
               "Qubic public identity round trips the public key");
        auto tampered_identity = signing_identity;
        tampered_identity.back() = tampered_identity.back() == 'A' ? 'B' : 'A';
        expect(!xdna::qubic::public_key_from_identity(tampered_identity, decoded_identity_key),
               "tampered Qubic public identity checksum rejects");
        constexpr std::string_view arbitrator_identity =
            "AFZPUAIYVPNUYGJRQVLUKOPPVLHAZQTGLYAAUUNBXFTVTAMSBKQBLEIEPCVJ";
        PublicKey arbitrator_key{};
        expect(xdna::qubic::public_key_from_identity(arbitrator_identity, arbitrator_key)
                   && xdna::qubic::public_identity_from_public_key(arbitrator_key)
                       == arbitrator_identity,
               "pinned Qubic arbitrator identity decodes and re-encodes");

        std::array<Byte, 64U> signature{};
        signing_material.public_key = signing_public_key;
        expect(provider.sign(signing_material,
                             std::span<const Byte>(signing_digest),
                             std::span<Byte>(signature)),
               "Qubic SchnorrQ sign succeeds");
        expect_bytes<64U>("Qubic SchnorrQ signature KAT",
                          signature,
                          to_hex(expected_signature));
        expect(provider.verify(signing_public_key,
                               std::span<const Byte>(signing_digest),
                               std::span<const Byte>(signature)),
               "Qubic SchnorrQ signature verifies");
        auto tampered_signature = signature;
        tampered_signature[0] ^= 0x01U;
        expect(!provider.verify(signing_public_key,
                                std::span<const Byte>(signing_digest),
                                std::span<const Byte>(tampered_signature)),
               "tampered signature rejects");

        // Synthetic FourQ ECDH vector. The expected values are committed as
        // fixed bytes; neither input is a real operator secret.
        const auto secret_a = from_hex<32U>(
            "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
        const auto secret_b = from_hex<32U>(
            "ffeeddccbbaa99887766554433221100f0e1d2c3b4a5968778695a4b3c2d1e0f");
        SigningMaterial material_a;
        SigningMaterial material_b;
        material_a.secret.bytes = secret_a;
        material_b.secret.bytes = secret_b;
        PublicKey public_a{};
        PublicKey public_b{};
        expect(provider.derive_public_key(std::span<const Byte>(secret_a), public_a),
               "derive FourQ public key A");
        expect(provider.derive_public_key(std::span<const Byte>(secret_b), public_b),
               "derive FourQ public key B");
        expect_bytes<32U>("FourQ public-key A KAT",
                          public_a.bytes,
                          "50c72fb73d5264043624ee10ce3a416953f208baa554d4e527af08681c1bcf33");
        expect_bytes<32U>("FourQ public-key B KAT",
                          public_b.bytes,
                          "66522bda989ecbb9668d6299bf93864ffd4f16300ace1f89a6855b5f8fc5ceca");
        material_a.public_key = public_a;
        material_b.public_key = public_b;

        std::array<Byte, 32U> shared_a{};
        std::array<Byte, 32U> shared_b{};
        expect(provider.derive_shared_key(material_a,
                                          public_b,
                                          std::span<Byte>(shared_a)),
               "derive shared key A");
        expect(provider.derive_shared_key(material_b,
                                          public_a,
                                          std::span<Byte>(shared_b)),
               "derive shared key B");
        expect(shared_a == shared_b, "FourQ shared keys agree");
        expect_bytes<32U>("FourQ shared-key KAT",
                          shared_a,
                          "58c565790e8c60e99c95a7bf0289314ec3431be336be586fe694e5d0c1757578");

        const auto gamming_nonce = from_hex<32U>(
            "00112233445566778899aabbccddeeff102132435465768798a9bacbdcedfe0f");
        std::array<Byte, 64U> gamma_input{};
        std::copy(shared_a.begin(), shared_a.end(), gamma_input.begin());
        std::copy(gamming_nonce.begin(), gamming_nonce.end(), gamma_input.begin() + 32U);
        std::array<Byte, 32U> gamming_key{};
        std::array<Byte, 68U> gamma{};
        expect(provider.hash(std::span<const Byte>(gamma_input), std::span<Byte>(gamming_key)),
               "gamming-key K12 succeeds");
        expect(provider.hash(std::span<const Byte>(gamming_key), std::span<Byte>(gamma)),
               "gamma K12 succeeds");
        expect_bytes<32U>("gamming-key KAT",
                          gamming_key,
                          "adb1f01c2c68dd86695ea5277b914d513f484a4dae70ea7ef29c46f82d8979f0");
        expect_bytes<68U>("gamma-stream KAT",
                          gamma,
                          "1ec0d19227f320525a216d6b0c4b5b3d6b3adaa56065d98f69e2697faf8b510f"
                          "e1e703f017aed75d47a353b7e792353c7cc098e80021b6c4408fd1cd4a4bca4a"
                          "6514892f");

        SigningMaterial mismatched_material;
        mismatched_material.secret.bytes = signing_subseed;
        mismatched_material.public_key = signing_public_key;
        mismatched_material.public_key.bytes[0] ^= 0x01U;
        expect(!provider.sign(mismatched_material,
                              std::span<const Byte>(signing_digest),
                              std::span<Byte>(signature)),
               "mismatched signing material rejects");

        SigningMaterial zero_material;
        PublicKey zero_public{};
        expect(!provider.derive_public_key(std::span<const Byte>(zero_material.secret.bytes),
                                           zero_public),
               "zero signing subseed rejects");
        expect(!provider.derive_shared_key(material_a, zero_public, std::span<Byte>(shared_a)),
               "zero peer key rejects");

        // Exercise the production provider through the direct-node solution
        // boundary as well as through its individual primitives. Search only
        // a bounded synthetic nonce space for a solution-message gamming key.
        WorkContext direct_context;
        direct_context.epoch = 1U;
        direct_context.tick = 2U;
        direct_context.mining_seed.bytes = signing_subseed;
        direct_context.solution_threshold = 1U;
        direct_context.algorithm = Algorithm::Bpp9000;
        direct_context.task = TaskIdentity{};
        direct_context.number_of_windows = 0U;

        SubmissionInput direct_input;
        direct_input.candidate_context = direct_context;
        direct_input.candidate_task = direct_context.task;
        direct_input.candidate_algorithm = Algorithm::Bpp9000;
        direct_input.source_public_key = signing_public_key;
        direct_input.destination_public_key = public_b;
        direct_input.candidate = CandidateSolution{direct_context.mining_seed, {}, 0U};
        direct_input.candidate.nonce.bytes[0U] = 1U;
        direct_input.candidate.nonce.bytes[1U] = 1U;
        direct_input.evidence = ScoreEvidence{
            ScoreResult{0U, ScoreStatus::Settled, 0U, 0U},
            ScoreResult{0U, ScoreStatus::Settled, 0U, 0U},
        };
        direct_input.signing_material = &signing_material;

        bool found_solution_nonce = false;
        std::array<Byte, 32U> direct_gamming_key{};
        for (unsigned int value = 0U; value < 256U && !found_solution_nonce; ++value) {
            direct_input.gamming_nonce.fill(0U);
            direct_input.gamming_nonce[0U] = static_cast<Byte>(value);
            std::array<Byte, 64U> gamming_input{};
            std::copy(direct_input.gamming_nonce.begin(),
                      direct_input.gamming_nonce.end(),
                      gamming_input.begin() + 32U);
            found_solution_nonce = provider.hash(std::span<const Byte>(gamming_input),
                                                 std::span<Byte>(direct_gamming_key))
                && direct_gamming_key[0U] == 0U;
        }
        expect(found_solution_nonce, "find bounded synthetic solution-message nonce");
        if (found_solution_nonce) {
            const auto solution = build_solution(direct_input, provider);
            const Frame frame = xdna::qubic::parse_frame(solution.frame);
            expect(frame.payload.size() == xdna::qubic::kBroadcastPayloadBytes,
                   "production provider builds exact direct-node payload size");
            const std::span<const Byte> body(frame.payload.data(), 164U);
            const std::span<const Byte> wire_signature(frame.payload.data() + 164U, 64U);
            std::array<Byte, 32U> body_digest{};
            expect(provider.hash(body, std::span<Byte>(body_digest)),
                   "production provider hashes direct-node body");
            expect(provider.verify(signing_public_key,
                                   std::span<const Byte>(body_digest),
                                   wire_signature),
                   "production provider verifies direct-node signature");
        }
    } catch (const std::exception& error) {
        std::cerr << "FAIL: test setup exception: " << error.what() << '\n';
        return 1;
    }

    if (failures != 0) {
        std::cerr << failures << " crypto test(s) failed\n";
        return 1;
    }

    std::cout << "Qubic K12/FourQ crypto KATs passed\n";
    return 0;
}
