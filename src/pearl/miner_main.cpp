#include "pearl/candidate.hpp"
#include "pearl/gateway.hpp"
#include "pearl/work.hpp"

#include "xdna/device.hpp"
#include "xdna/errors.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace xdna::pearl;

enum class Mode : std::uint8_t {
    None,
    Help,
    Version,
    SelfTest,
    HardwareInfo,
    Benchmark,
    DryRun,
    Mine,
};

struct Options {
    Mode mode = Mode::None;
    std::string config_path;
    std::string gateway_unix = "/tmp/pearlgw.sock";
    std::string gateway_host = "127.0.0.1";
    std::uint16_t gateway_port = 8337U;
    bool gateway_tcp = false;
    std::string node_url;
    std::string mining_address;
    std::string device = "0";
    std::size_t batch = 1U;
    unsigned columns = 1U;
    std::string log_level = "info";
    bool json_status = false;
    std::uint64_t max_runtime_seconds = 0U;
    bool fixture_work = false;
    std::filesystem::path artifact_dir;
    std::size_t benchmark_iterations = 100U;
};

constexpr std::string_view kVersion = "pearl-xdna-miner 0.1.0-p11";

[[nodiscard]] std::string json_escape(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += character; break;
        }
    }
    return result;
}

[[nodiscard]] std::string trim(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1U);
}

void apply_config_value(Options& options, std::string key, std::string value)
{
    value = trim(value);
    if (value.size() >= 2U && value.front() == '"' && value.back() == '"') {
        value = value.substr(1U, value.size() - 2U);
    }
    if (key == "gateway_unix") options.gateway_unix = value;
    else if (key == "gateway_host") { options.gateway_host = value; options.gateway_tcp = true; }
    else if (key == "gateway_port") options.gateway_port = static_cast<std::uint16_t>(std::stoul(value));
    else if (key == "node_url") options.node_url = value;
    else if (key == "mining_address") options.mining_address = value;
    else if (key == "device") options.device = value;
    else if (key == "batch") options.batch = std::stoull(value);
    else if (key == "columns") options.columns = static_cast<unsigned>(std::stoul(value));
    else if (key == "log_level") options.log_level = value;
    else if (key == "max_runtime") options.max_runtime_seconds = std::stoull(value);
    else if (key == "artifact_dir") options.artifact_dir = value;
    else if (key == "network") { /* accepted for forward-compatible config files */ }
}

void load_config_file(Options& options, const std::string& path)
{
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open --config file");
    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;
        const auto equals = line.find('=');
        if (equals == std::string::npos) throw std::runtime_error("config line lacks '='");
        apply_config_value(options, trim(line.substr(0U, equals)), line.substr(equals + 1U));
    }
}

[[nodiscard]] std::string require_value(int& index, int argc, char** argv, const char* name)
{
    if (index + 1 >= argc) throw std::runtime_error(std::string("missing value for ") + name);
    return argv[++index];
}

void parse_options(int argc, char** argv, Options& options)
{
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == "--config") {
            options.config_path = require_value(index, argc, argv, "--config");
            load_config_file(options, options.config_path);
            break;
        }
    }
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--help" || argument == "-h") options.mode = Mode::Help;
        else if (argument == "--version") options.mode = Mode::Version;
        else if (argument == "--self-test") options.mode = Mode::SelfTest;
        else if (argument == "--hardware-info") options.mode = Mode::HardwareInfo;
        else if (argument == "--benchmark") options.mode = Mode::Benchmark;
        else if (argument == "--dry-run") options.mode = Mode::DryRun;
        else if (argument == "--mine") options.mode = Mode::Mine;
        else if (argument == "--config") { ++index; }
        else if (argument == "--gateway-unix") options.gateway_unix = require_value(index, argc, argv, "--gateway-unix");
        else if (argument == "--gateway-host") { options.gateway_host = require_value(index, argc, argv, "--gateway-host"); options.gateway_tcp = true; }
        else if (argument == "--gateway-port") options.gateway_port = static_cast<std::uint16_t>(std::stoul(require_value(index, argc, argv, "--gateway-port")));
        else if (argument == "--node-url") options.node_url = require_value(index, argc, argv, "--node-url");
        else if (argument == "--mining-address") options.mining_address = require_value(index, argc, argv, "--mining-address");
        else if (argument == "--device") options.device = require_value(index, argc, argv, "--device");
        else if (argument == "--batch") options.batch = std::stoull(require_value(index, argc, argv, "--batch"));
        else if (argument == "--columns") options.columns = static_cast<unsigned>(std::stoul(require_value(index, argc, argv, "--columns")));
        else if (argument == "--log-level") options.log_level = require_value(index, argc, argv, "--log-level");
        else if (argument == "--json-status") options.json_status = true;
        else if (argument == "--max-runtime") options.max_runtime_seconds = std::stoull(require_value(index, argc, argv, "--max-runtime"));
        else if (argument == "--fixture-work") options.fixture_work = true;
        else if (argument == "--artifact-dir") options.artifact_dir = require_value(index, argc, argv, "--artifact-dir");
        else if (argument == "--benchmark-iterations") options.benchmark_iterations = std::stoull(require_value(index, argc, argv, "--benchmark-iterations"));
        else if (argument == "--network") { ++index; }
        else if (argument == "--version" || argument == "-h") { /* handled above */ }
        else throw std::runtime_error("unknown option: " + std::string(argument));
    }
    if (options.batch == 0U || options.batch > 1024U) throw std::runtime_error("--batch must be 1..1024");
    if (options.columns != 1U && options.columns != 2U && options.columns != 4U) throw std::runtime_error("--columns must be 1, 2, or 4");
    if (options.log_level != "error" && options.log_level != "warn"
        && options.log_level != "info" && options.log_level != "debug") {
        throw std::runtime_error("--log-level must be error, warn, info, or debug");
    }
}

void print_help()
{
    std::cout << kVersion << "\n\n"
              << "Safe default: no live mining is started. Choose one explicit mode.\n\n"
              << "  --help                 show this help\n"
              << "  --version              show version\n"
              << "  --self-test            CPU vectors plus one physical XDNA GEMM\n"
              << "  --hardware-info        print detected XDNA identity\n"
              << "  --benchmark            benchmark exact P2 GEMM\n"
              << "  --dry-run              acquire/verify work without submitting\n"
              << "  --mine                 explicit live mode; requires a public address\n\n"
              << "Configuration: --gateway-unix PATH | --gateway-host HOST --gateway-port N\n"
              << "  --node-url URL --mining-address ADDRESS --device SELECTOR\n"
              << "  --batch N --columns 1|2|4 --log-level LEVEL --json-status\n"
              << "  --max-runtime SECONDS --config FILE --artifact-dir DIR\n"
              << "  --fixture-work (local deterministic dry-run only; never implied live work)\n\n"
              << "Secrets: node RPC credentials are read only from PEARL_NODE_RPC_USER and\n"
              << "PEARL_NODE_RPC_PASSWORD; they are never printed.\n";
}

[[nodiscard]] std::filesystem::path default_artifact_dir()
{
    const char* environment = std::getenv("PEARL_XDNA_ARTIFACT_DIR");
    return environment == nullptr
        ? std::filesystem::path("build/pearl-xdna-gemm-p2")
        : std::filesystem::path(environment);
}

[[nodiscard]] XdnaMatmulArtifact artifact_for(const Options& options)
{
    std::filesystem::path directory = options.artifact_dir.empty()
        ? default_artifact_dir()
        : options.artifact_dir;
    if (options.columns != 1U && options.artifact_dir.empty()) {
        directory = "build/pearl-xdna-gemm-p2-c" + std::to_string(options.columns);
    }
    return XdnaMatmulArtifact{directory / "pearl_p2_gemm.xclbin",
                              directory / "pearl_p2_gemm.insts",
                              directory / "pearl_p2_gemm.manifest"};
}

[[nodiscard]] MiningConfiguration fixture_config()
{
    MiningConfiguration config;
    config.common_dim = 2048U;
    config.rank = 128U;
    config.rows_pattern = PeriodicPattern::from_indices(std::array<std::uint32_t, 2U>{0U, 8U});
    std::array<std::uint32_t, kSelectedColumns> columns{};
    for (std::size_t index = 0U; index < columns.size(); ++index) {
        columns[index] = static_cast<std::uint32_t>((index / 2U) * 8U + (index % 2U));
    }
    config.cols_pattern = PeriodicPattern::from_indices(columns);
    return config;
}

[[nodiscard]] PlainProof run_fixture_pipeline(const Options& options,
                                               XdnaMatmulExecutor& executor)
{
    const MiningConfiguration config = fixture_config();
    IncompleteBlockHeader header;
    header.version = 1U;
    header.timestamp = 123U;
    header.nbits = 0x207FFFFFU;
    const Int8Matrix full_a(9U, 2048U, std::vector<std::int8_t>(9U * 2048U, 0));
    const Int8Matrix full_b(2048U, 250U, std::vector<std::int8_t>(2048U * 250U, 0));
    const std::vector<std::uint32_t> rows = config.rows_pattern.indices();
    const std::vector<std::uint32_t> columns = config.cols_pattern.indices();
    std::vector<std::int8_t> selected_a_values(2U * 2048U, 0);
    for (std::size_t row = 0U; row < rows.size(); ++row) {
        for (std::size_t column = 0U; column < 2048U; ++column) {
            selected_a_values[row * 2048U + column] = full_a.at(rows[row], column);
        }
    }
    std::vector<std::int8_t> selected_b_values(2048U * 64U, 0);
    for (std::size_t row = 0U; row < 2048U; ++row) {
        for (std::size_t column = 0U; column < columns.size(); ++column) {
            selected_b_values[row * 64U + column] = full_b.at(row, columns[column]);
        }
    }
    const Int8Matrix selected_a(2U, 2048U, std::move(selected_a_values));
    const Int8Matrix selected_b(2048U, 64U, std::move(selected_b_values));
    const Digest key = job_key(header, config);
    const Digest hash_a = merkle_root(full_a.raw_bytes(), key);
    std::vector<std::int8_t> bt_values(250U * 2048U, 0);
    for (std::size_t row = 0U; row < full_b.rows(); ++row) {
        for (std::size_t column = 0U; column < full_b.cols(); ++column) {
            bt_values[column * full_b.rows() + row] = full_b.at(row, column);
        }
    }
    const Int8Matrix bt(250U, 2048U, std::move(bt_values));
    const Digest hash_b = merkle_root(bt.raw_bytes(), key);
    const CommitmentSeeds seeds = commitment_seeds(key, hash_a, hash_b);
    const std::vector<std::size_t> row_indices(rows.begin(), rows.end());
    const std::vector<std::size_t> column_indices(columns.begin(), columns.end());
    const NoiseMatrices noise = generate_noise(2048U, 128U, seeds, row_indices, column_indices);
    Digest target{};
    target.fill(0xFFU);
    ComputePipeline pipeline(executor);
    const ComputePipelineResult result = pipeline.run(
        selected_a, selected_b, noise, 128U, seeds.a_noise_seed, target);
    PlainProof proof = build_plain_proof(header, config, full_a, full_b, 0U, 0U, result, target);
    verify_plain_proof_candidate(proof);
    (void)options;
    return proof;
}

void print_status(const Options& options, std::string_view state, std::string_view detail)
{
    if (options.json_status) {
        std::cout << "{\"state\":\"" << json_escape(state)
                  << "\",\"device\":\"" << json_escape(options.device)
                  << "\",\"batch\":" << options.batch
                  << ",\"columns\":" << options.columns
                  << ",\"detail\":\"" << json_escape(detail) << "\"}\n";
    } else {
        std::cout << state << ": " << detail << '\n';
    }
}

int run_self_test(const Options& options)
{
    const Int8Matrix left(2U, 3U, {-1, 2, 3, 4, -5, 6});
    const Int8Matrix right(3U, 2U, {7, -8, 9, 10, -11, 12});
    const Int32Matrix expected = gemm_checked(left, right);
    if (expected.at(0U, 0U) != -22 || expected.at(1U, 1U) != -10) {
        print_status(options, "CPU_SELF_TEST_FAIL", "P1 signed GEMM vector mismatch");
        return 1;
    }
    const Digest key{};
    if (blake3_keyed(key, std::vector<std::uint8_t>{1U, 2U, 3U}) == Digest{}) {
        print_status(options, "CPU_SELF_TEST_FAIL", "BLAKE3 returned an all-zero digest unexpectedly");
        return 1;
    }
    print_status(options, "CPU_SELF_TEST_PASS", "P1 vectors and BLAKE3 passed");
    try {
        XdnaMatmulExecutor executor(artifact_for(options), options.device);
        const Int8Matrix npu_left(kP2Rows, kP2Common,
                                  std::vector<std::int8_t>(kP2LeftBytes, 1));
        const Int8Matrix npu_right(kP2Common, kP2Columns,
                                   std::vector<std::int8_t>(kP2RightBytes, -1));
        const Int32Matrix npu_expected = gemm_checked(npu_left, npu_right);
        const Int32Matrix actual = executor.dispatch(npu_left, npu_right);
        if (actual.values() != npu_expected.values()) {
            print_status(options, "XDNA_MISMATCH", "physical GEMM differs from CPU oracle");
            return 1;
        }
        print_status(options, "XDNA_SELF_TEST_PASS", "physical XDNA1 GEMM matched CPU oracle");
        return 0;
    } catch (const std::exception& error) {
        print_status(options, "XDNA_DEVICE_UNAVAILABLE", error.what());
        return 1;
    }
}

int run_hardware_info(const Options& options)
{
    const auto report = xdna::runtime::probe_device(options.device);
    if (options.json_status) {
        std::cout << "{\"status\":\"" << xdna::runtime::capability_status_name(report.status)
                  << "\",\"device\":\"" << json_escape(report.device_name)
                  << "\",\"architecture\":\"" << json_escape(report.architecture)
                  << "\",\"bdf\":\"" << json_escape(report.bdf)
                  << "\",\"device_node\":\"" << json_escape(report.device_node)
                  << "\",\"firmware\":\"" << json_escape(report.firmware_version)
                  << "\",\"xrt\":\"" << json_escape(report.xrt_version)
                  << "\",\"amdxdna\":\"" << json_escape(report.amdxdna_version)
                  << "\",\"detail\":\"" << json_escape(report.detail) << "\"}\n";
    } else {
        std::cout << "status=" << xdna::runtime::capability_status_name(report.status) << '\n'
                  << "device=" << report.device_name << '\n'
                  << "architecture=" << report.architecture << '\n'
                  << "bdf=" << report.bdf << '\n'
                  << "device_node=" << report.device_node << '\n'
                  << "firmware=" << report.firmware_version << '\n'
                  << "xrt=" << report.xrt_version << '\n'
                  << "amdxdna=" << report.amdxdna_version << '\n'
                  << "detail=" << report.detail << '\n';
    }
    return report.supported() ? 0 : 1;
}

int run_benchmark(const Options& options)
{
    XdnaMatmulExecutor executor(artifact_for(options), options.device);
    const Int8Matrix left(kP2Rows, kP2Common, std::vector<std::int8_t>(kP2LeftBytes, 1));
    const Int8Matrix right(kP2Common, kP2Columns, std::vector<std::int8_t>(kP2RightBytes, -1));
    const Int32Matrix expected = gemm_checked(left, right);
    const auto begin = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0U; iteration < options.benchmark_iterations; ++iteration) {
        if (executor.dispatch(left, right).values() != expected.values()) {
            print_status(options, "XDNA_MISMATCH", "benchmark output differs from CPU oracle");
            return 1;
        }
    }
    const auto end = std::chrono::steady_clock::now();
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
    const double throughput = static_cast<double>(options.benchmark_iterations)
        / (static_cast<double>(ns) / 1'000'000'000.0);
    std::ostringstream detail;
    detail << "iterations=" << options.benchmark_iterations
           << " throughput_dispatches_per_s=" << std::fixed << std::setprecision(2) << throughput
           << " mismatches=0 cpu_fallbacks=0";
    print_status(options, "BENCHMARK_PASS", detail.str());
    return 0;
}

int run_gateway_mode(const Options& options, bool mine)
{
    if (mine && options.mining_address.empty()) {
        print_status(options, "MAINNET_PAYOUT_ADDRESS_NOT_CONFIGURED",
                     "--mine requires an explicit public --mining-address");
        return 1;
    }
    GatewayClientConfig config;
    config.endpoint.transport = options.gateway_tcp
        ? GatewayTransport::LoopbackTcp : GatewayTransport::Unix;
    config.endpoint.unix_path = options.gateway_unix;
    config.endpoint.host = options.gateway_host;
    config.endpoint.port = options.gateway_port;
    GatewayClient client(config);
    try {
        GatewayJobProvider jobs(client);
        const PearlJob job = jobs.fetch();
        print_status(options, mine ? "JOB_ACQUIRED_LIVE_MODE" : "JOB_ACQUIRED_DRY_RUN",
                     "job_id=" + job.gateway_job.job_id + " target=" + job.gateway_job.target_decimal);
        ExternalPearlUsefulWorkProvider external_work;
        (void)external_work.fetch(job);
        return 0;
    } catch (const WorkError& error) {
        print_status(options, "WORK_PROVIDER_UNAVAILABLE", error.what());
        return 1;
    } catch (const GatewayError& error) {
        print_status(options, gateway_error_code_name(error.code()), error.what());
        return 1;
    } catch (const std::exception& error) {
        print_status(options, "DRY_RUN_FAILURE", error.what());
        return 1;
    }
}

int run_fixture_dry_run(const Options& options)
{
    try {
        XdnaMatmulExecutor executor(artifact_for(options), options.device);
        const PlainProof proof = run_fixture_pipeline(options, executor);
        std::ostringstream detail;
        detail << "fixture pipeline verified; proof_bytes=" << proof.serialize().size()
               << " submission=skipped cpu_fallbacks=0";
        print_status(options, "DRY_RUN_PASS", detail.str());
        return 0;
    } catch (const std::exception& error) {
        print_status(options, "DRY_RUN_FAILURE", error.what());
        return 1;
    }
}

} // namespace

int main(int argc, char** argv)
{
    try {
        Options options;
        parse_options(argc, argv, options);
        if (options.mode == Mode::Help || options.mode == Mode::None) {
            print_help();
            return options.mode == Mode::None ? 0 : 0;
        }
        if (options.mode == Mode::Version) {
            std::cout << kVersion << '\n';
            return 0;
        }
        if (options.mode == Mode::SelfTest) return run_self_test(options);
        if (options.mode == Mode::HardwareInfo) return run_hardware_info(options);
        if (options.mode == Mode::Benchmark) return run_benchmark(options);
        if (options.mode == Mode::DryRun) {
            return options.fixture_work ? run_fixture_dry_run(options)
                                        : run_gateway_mode(options, false);
        }
        if (options.mode == Mode::Mine) {
            if (options.fixture_work) {
                print_status(options, "INVALID_CONFIGURATION", "fixture work is allowed only with --dry-run");
                return 1;
            }
            return run_gateway_mode(options, true);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "CLI_ERROR: " << error.what() << '\n';
        return 2;
    }
}
