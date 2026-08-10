#pragma once

#include "qubic/direct_node.hpp"

#include <filesystem>
#include <string>

namespace xdna::qubic {

// These helpers are deliberately file-descriptor based. They accept only a
// regular, current-user-owned 0600 file containing one 32-byte subseed in
// hexadecimal form and never include the secret in an error string.
[[nodiscard]] bool load_signing_subseed_file(const std::filesystem::path& path,
                                             SigningSecret& secret,
                                             std::string& error) noexcept;

[[nodiscard]] bool create_signing_subseed_file(const std::filesystem::path& path,
                                               SigningSecret& secret,
                                               bool replace_existing,
                                               std::string& error) noexcept;

// This operation is intentionally explicit and only targets the path passed
// by the caller. It overwrites the file, fsyncs it, closes it and unlinks it.
[[nodiscard]] bool erase_signing_subseed_file(const std::filesystem::path& path,
                                              std::string& error) noexcept;

} // namespace xdna::qubic
