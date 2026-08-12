#pragma once

#include "pearl/gateway.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace xdna::pearl {

enum class WorkErrorCode : std::uint8_t {
    ProviderUnavailable,
    InvalidWork,
    StaleWork,
};

class WorkError final : public std::runtime_error {
public:
    WorkError(WorkErrorCode code, const std::string& message);

    [[nodiscard]] WorkErrorCode code() const noexcept
    {
        return code_;
    }

private:
    WorkErrorCode code_;
};

struct PearlJob {
    MiningJob gateway_job;
    IncompleteBlockHeader header;
};

struct PearlWorkUnit {
    PearlJob job;
    MiningConfiguration config;
    Int8Matrix a;
    Int8Matrix b;
    std::uint32_t t_rows = 0U;
    std::uint32_t t_cols = 0U;
};

class GatewayJobProvider final {
public:
    explicit GatewayJobProvider(GatewayClient& client) noexcept
        : client_(client)
    {
    }

    [[nodiscard]] PearlJob fetch();
    // Fetches a fresh template immediately before a submission boundary.  A
    // candidate is stale if *any* immutable identity field changed.
    void assert_current(const MiningJobIdentity& expected);

private:
    GatewayClient& client_;
    std::string last_job_id_;
};

class UsefulWorkProvider {
public:
    virtual ~UsefulWorkProvider() = default;
    [[nodiscard]] virtual PearlWorkUnit fetch(const PearlJob& job) = 0;
};

class DeterministicFixtureWorkProvider final : public UsefulWorkProvider {
public:
    [[nodiscard]] PearlWorkUnit fetch(const PearlJob& job) override;
};

// The pinned official gateway's MiningJob carries header/target/certificate
// metadata only.  The current A/B tensors are supplied by the official local
// useful-work/inference runtime, which is an external component here because
// its hot miner code has no clear reuse grant.
class ExternalPearlUsefulWorkProvider final : public UsefulWorkProvider {
public:
    [[nodiscard]] PearlWorkUnit fetch(const PearlJob&) override;
};

} // namespace xdna::pearl
