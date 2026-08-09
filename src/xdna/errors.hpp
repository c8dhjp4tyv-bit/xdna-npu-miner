#pragma once

#include <stdexcept>
#include <string>

namespace xdna::runtime {

enum class ErrorCode {
    XrtUnavailable,
    DriverUnavailable,
    FirmwareUnavailableOrUnknown,
    ToolchainUnavailable,
    RuntimeVersionMismatch,
    NoXdnaDevice,
    WrongXdnaGeneration,
    DeviceOpenFailed,
    ArtifactMissing,
    ArtifactInvalid,
    RuntimeContextCreationFailed,
    InvalidBuffer,
    SynchronizationFailed,
    DeviceExecutionFailed,
    OutputMismatch,
    InvalidArgument,
};

[[nodiscard]] const char* error_code_name(ErrorCode code) noexcept;

class RuntimeError final : public std::runtime_error {
public:
    RuntimeError(ErrorCode code, std::string message);

    [[nodiscard]] ErrorCode code() const noexcept
    {
        return code_;
    }

private:
    ErrorCode code_;
};

} // namespace xdna::runtime
