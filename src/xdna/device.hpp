#pragma once

#include <string>

namespace xdna::runtime {

enum class CapabilityStatus {
    SupportedXdna1,
    NoXdnaDevice,
    WrongXdnaGeneration,
    XrtUnavailable,
    DriverUnavailable,
    FirmwareUnavailableOrUnknown,
    ToolchainUnavailable,
    RuntimeVersionMismatch,
    DeviceOpenFailed,
};

[[nodiscard]] const char* capability_status_name(CapabilityStatus status) noexcept;

struct CapabilityReport {
    CapabilityStatus status = CapabilityStatus::XrtUnavailable;
    std::string selector;
    std::string device_name;
    std::string architecture;
    std::string bdf;
    std::string device_node;
    std::string firmware_version;
    std::string xrt_version;
    std::string xrt_hash;
    std::string amdxdna_version;
    std::string detail;

    [[nodiscard]] bool supported() const noexcept
    {
        return status == CapabilityStatus::SupportedXdna1;
    }
};

[[nodiscard]] CapabilityReport probe_device(const std::string& selector = "0");

} // namespace xdna::runtime
