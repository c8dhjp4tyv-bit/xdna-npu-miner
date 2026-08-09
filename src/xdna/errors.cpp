#include "xdna/errors.hpp"

#include <utility>

namespace xdna::runtime {

const char* error_code_name(ErrorCode code) noexcept
{
    switch (code) {
    case ErrorCode::XrtUnavailable:
        return "XRT_UNAVAILABLE";
    case ErrorCode::DriverUnavailable:
        return "DRIVER_UNAVAILABLE";
    case ErrorCode::FirmwareUnavailableOrUnknown:
        return "FIRMWARE_UNAVAILABLE_OR_UNKNOWN";
    case ErrorCode::ToolchainUnavailable:
        return "TOOLCHAIN_UNAVAILABLE";
    case ErrorCode::RuntimeVersionMismatch:
        return "RUNTIME_VERSION_MISMATCH";
    case ErrorCode::NoXdnaDevice:
        return "NO_XDNA_DEVICE";
    case ErrorCode::WrongXdnaGeneration:
        return "WRONG_XDNA_GENERATION";
    case ErrorCode::DeviceOpenFailed:
        return "DEVICE_OPEN_FAILED";
    case ErrorCode::ArtifactMissing:
        return "ARTIFACT_MISSING";
    case ErrorCode::ArtifactInvalid:
        return "ARTIFACT_INVALID";
    case ErrorCode::RuntimeContextCreationFailed:
        return "RUNTIME_CONTEXT_CREATION_FAILED";
    case ErrorCode::InvalidBuffer:
        return "INVALID_BUFFER";
    case ErrorCode::SynchronizationFailed:
        return "SYNCHRONIZATION_FAILED";
    case ErrorCode::DeviceExecutionFailed:
        return "DEVICE_EXECUTION_FAILED";
    case ErrorCode::OutputMismatch:
        return "OUTPUT_MISMATCH";
    case ErrorCode::InvalidArgument:
        return "INVALID_ARGUMENT";
    }
    return "UNCLASSIFIED";
}

RuntimeError::RuntimeError(ErrorCode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code)
{
}

} // namespace xdna::runtime
