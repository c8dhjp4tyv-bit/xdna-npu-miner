#include "pearl/work.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace xdna::pearl {

WorkError::WorkError(WorkErrorCode code, const std::string& message)
    : std::runtime_error(message),
      code_(code)
{
}

PearlJob GatewayJobProvider::fetch()
{
    MiningJob gateway_job = client_.get_mining_info();
    const std::array<std::uint8_t, kHeaderBytes> header_bytes = [&] {
        std::array<std::uint8_t, kHeaderBytes> result{};
        std::copy(gateway_job.incomplete_header_bytes.begin(),
                  gateway_job.incomplete_header_bytes.end(),
                  result.begin());
        return result;
    }();
    PearlJob result{std::move(gateway_job), deserialize_header(header_bytes)};
    if (!last_job_id_.empty() && result.gateway_job.job_id == last_job_id_) {
        // Reusing a current job is valid; it is not a stale submission.
    }
    last_job_id_ = result.gateway_job.job_id;
    return result;
}

PearlWorkUnit DeterministicFixtureWorkProvider::fetch(const PearlJob& job)
{
    MiningConfiguration config;
    config.common_dim = 2048U;
    config.rank = 128U;
    config.rows_pattern = PeriodicPattern::from_indices(
        std::array<std::uint32_t, 2U>{0U, 8U});
    std::array<std::uint32_t, kSelectedColumns> columns{};
    for (std::size_t index = 0U; index < columns.size(); ++index) {
        columns[index] = static_cast<std::uint32_t>((index / 2U) * 8U + (index % 2U));
    }
    config.cols_pattern = PeriodicPattern::from_indices(columns);
    const Int8Matrix a(9U, 2048U, std::vector<std::int8_t>(9U * 2048U, 0));
    const Int8Matrix b(2048U, 250U, std::vector<std::int8_t>(2048U * 250U, 0));
    validate_configuration(config, 9U, 250U, 0U, 0U);
    return PearlWorkUnit{job, std::move(config), a, b, 0U, 0U};
}

PearlWorkUnit ExternalPearlUsefulWorkProvider::fetch(const PearlJob&)
{
    throw WorkError(
        WorkErrorCode::ProviderUnavailable,
        "official Pearl useful-work/inference provider is not installed; matrices were not fabricated");
}

} // namespace xdna::pearl
