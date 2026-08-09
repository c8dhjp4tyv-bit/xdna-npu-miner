#include "xdna/device.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string_view>

#include "xrt/xrt_device.h"

namespace xdna::runtime {
namespace {

[[nodiscard]] std::string lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

[[nodiscard]] std::string trim(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1U);
}

[[nodiscard]] std::string section_label_value(const std::string& text,
                                               std::string_view section,
                                               std::string_view label)
{
    std::istringstream stream(text);
    std::string line;
    bool in_section = false;
    while (std::getline(stream, line)) {
        const std::string stripped = trim(line);
        if (stripped == section) {
            in_section = true;
            continue;
        }
        if (in_section && !line.empty() && line.front() != ' ' && line.front() != '\t') {
            break;
        }
        if (in_section) {
            const auto position = line.find(label);
            if (position != std::string::npos) {
                const auto colon = line.find(':', position + label.size());
                if (colon != std::string::npos) {
                    return trim(line.substr(colon + 1U));
                }
            }
        }
    }
    return {};
}

[[nodiscard]] std::string capture_xrt_smi()
{
    const char* command =
        std::filesystem::exists("/opt/xilinx/xrt/bin/xrt-smi")
        ? "/opt/xilinx/xrt/bin/xrt-smi examine 2>/dev/null"
        : "xrt-smi examine 2>/dev/null";
    FILE* pipe = popen(command, "r");
    if (pipe == nullptr) {
        return {};
    }

    std::string output;
    std::array<char, 512U> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output.append(buffer.data());
    }
    (void)pclose(pipe);
    return output;
}

[[nodiscard]] std::string device_node_for_accel()
{
    const std::filesystem::path accel_root{"/dev/accel"};
    if (!std::filesystem::is_directory(accel_root)) {
        return {};
    }
    for (const auto& entry : std::filesystem::directory_iterator(accel_root)) {
        if (entry.path().filename().string().starts_with("accel")) {
            return entry.path().string();
        }
    }
    return {};
}

[[nodiscard]] bool driver_present()
{
    return std::filesystem::exists("/sys/module/amdxdna")
        || std::filesystem::exists("/sys/bus/pci/drivers/amdxdna");
}

[[nodiscard]] bool selector_is_default(const std::string& selector)
{
    return selector == "0" || selector == "accel0";
}

[[nodiscard]] xrt::device open_xrt_device(const std::string& selector)
{
    if (selector_is_default(selector)) {
        return xrt::device(0U);
    }
    return xrt::device(selector);
}

} // namespace

const char* capability_status_name(CapabilityStatus status) noexcept
{
    switch (status) {
    case CapabilityStatus::SupportedXdna1:
        return "SUPPORTED_XDNA1";
    case CapabilityStatus::NoXdnaDevice:
        return "NO_XDNA_DEVICE";
    case CapabilityStatus::WrongXdnaGeneration:
        return "WRONG_XDNA_GENERATION";
    case CapabilityStatus::XrtUnavailable:
        return "XRT_UNAVAILABLE";
    case CapabilityStatus::DriverUnavailable:
        return "DRIVER_UNAVAILABLE";
    case CapabilityStatus::FirmwareUnavailableOrUnknown:
        return "FIRMWARE_UNAVAILABLE_OR_UNKNOWN";
    case CapabilityStatus::ToolchainUnavailable:
        return "TOOLCHAIN_UNAVAILABLE";
    case CapabilityStatus::RuntimeVersionMismatch:
        return "RUNTIME_VERSION_MISMATCH";
    case CapabilityStatus::DeviceOpenFailed:
        return "DEVICE_OPEN_FAILED";
    }
    return "UNKNOWN";
}

CapabilityReport probe_device(const std::string& selector)
{
    CapabilityReport report;
    report.selector = selector;
    report.device_node = device_node_for_accel();
    if (!driver_present()) {
        report.status = CapabilityStatus::DriverUnavailable;
        report.detail = "amdxdna is not present in sysfs";
        return report;
    }

    xrt::device device;
    try {
        device = open_xrt_device(selector);
    } catch (const std::exception& error) {
        report.status = report.device_node.empty() || selector_is_default(selector)
            ? CapabilityStatus::NoXdnaDevice
            : CapabilityStatus::DeviceOpenFailed;
        report.detail = error.what();
        return report;
    }

    try {
        report.device_name = device.get_info<xrt::info::device::name>();
        report.bdf = device.get_info<xrt::info::device::bdf>();
    } catch (const std::exception& error) {
        report.status = CapabilityStatus::DeviceOpenFailed;
        report.detail = std::string("XRT device identity query failed: ") + error.what();
        return report;
    }

    const std::string xrt_output = capture_xrt_smi();
    report.xrt_version = section_label_value(xrt_output, "XRT", "Version");
    report.xrt_hash = section_label_value(xrt_output, "XRT", "Hash");
    report.firmware_version = section_label_value(xrt_output, "XRT", "NPU Firmware Version");
    report.amdxdna_version = section_label_value(xrt_output, "XRT", "amdxdna Version");

    const std::string lower_name = lower_copy(report.device_name);
    const std::string lower_output = lower_copy(xrt_output);
    if (lower_name.find("npu2") != std::string::npos
        || lower_name.find("strix") != std::string::npos
        || lower_name.find("npu4") != std::string::npos
        || lower_name.find("npu5") != std::string::npos
        || lower_name.find("npu6") != std::string::npos
        || lower_output.find("aie2p") != std::string::npos
        || lower_output.find("npu strix") != std::string::npos) {
        report.architecture = lower_output.find("aie2p") != std::string::npos ? "aie2p" : "unknown";
        report.status = CapabilityStatus::WrongXdnaGeneration;
        report.detail = "the selected device is not XDNA1/AIE2";
        return report;
    }

    const bool npu1_reported = lower_output.find("ryzenai-npu1") != std::string::npos
        || lower_name.find("ryzenai-npu1") != std::string::npos
        || lower_name.find("npu1") != std::string::npos;
    const bool aie2_reported = lower_output.find("|aie2") != std::string::npos
        || lower_output.find(" aie2") != std::string::npos
        || lower_output.find("architecture: aie2") != std::string::npos;
    if (npu1_reported && aie2_reported) {
        report.architecture = "aie2";
    }

    if (report.firmware_version.empty() || report.xrt_version.empty()
        || report.architecture != "aie2") {
        report.status = CapabilityStatus::FirmwareUnavailableOrUnknown;
        report.detail = "XDNA1-like device opened, but XRT identity/firmware evidence is incomplete";
        return report;
    }

    if (report.device_name.empty()) {
        report.status = CapabilityStatus::DeviceOpenFailed;
        report.detail = "XRT returned an empty device name";
        return report;
    }

    report.status = CapabilityStatus::SupportedXdna1;
    report.detail = "XRT opened RyzenAI-npu1 and xrt-smi reported AIE2";
    return report;
}

} // namespace xdna::runtime
