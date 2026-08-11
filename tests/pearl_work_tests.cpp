#include "pearl/work.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using namespace xdna::pearl;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main()
{
    try {
        MiningJob job;
        job.incomplete_header_bytes.assign(kHeaderBytes, 0U);
        job.target.fill(0xFFU);
        job.target_decimal = "115792089237316195423570985008687907853269984665640564039457584007913129639935";
        job.certificate_version = 1U;
        job.job_id = "fixture-job";
        const PearlJob pearl_job{job, IncompleteBlockHeader{}};
        DeterministicFixtureWorkProvider fixture;
        const PearlWorkUnit work = fixture.fetch(pearl_job);
        expect(work.a.rows() == 9U && work.a.cols() == 2048U, "fixture A shape");
        expect(work.b.rows() == 2048U && work.b.cols() == 250U, "fixture B shape");
        validate_configuration(work.config, 9U, 250U, 0U, 0U);

        ExternalPearlUsefulWorkProvider external;
        bool unavailable = false;
        try {
            (void)external.fetch(pearl_job);
        } catch (const WorkError& error) {
            unavailable = error.code() == WorkErrorCode::ProviderUnavailable;
        }
        expect(unavailable, "external useful-work provider is explicit");
        std::cout << "pearl work contract tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "pearl work test failure: " << error.what() << '\n';
        return 1;
    }
}
