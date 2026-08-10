#include "qubic/local_identity.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <string_view>

namespace xdna::qubic {
namespace {

constexpr mode_t kRequiredMode = S_IRUSR | S_IWUSR;
constexpr mode_t kPermissionMask = S_IRWXU | S_IRWXG | S_IRWXO;
constexpr std::size_t kSecretBytes = 32U;
constexpr std::size_t kSecretHexCharacters = kSecretBytes * 2U;
constexpr std::size_t kSecretTextBytes = kSecretHexCharacters + 1U;

void clear_secret(SigningSecret& secret) noexcept
{
    std::fill(secret.bytes.begin(), secret.bytes.end(), static_cast<Byte>(0U));
}

void clear_text(std::array<char, kSecretTextBytes>& text) noexcept
{
    std::fill(text.begin(), text.end(), '\0');
}

void clear_string(std::string& text) noexcept
{
    std::fill(text.begin(), text.end(), '\0');
    text.clear();
}

void set_error(std::string& error, std::string_view message) noexcept
{
    clear_string(error);
    error.assign(message.begin(), message.end());
}

[[nodiscard]] bool validate_secret_file(int fd, struct stat& status, std::string& error) noexcept
{
    if (::fstat(fd, &status) != 0) {
        set_error(error, "cannot inspect the local identity file");
        return false;
    }
    if (!S_ISREG(status.st_mode)) {
        set_error(error, "local identity path is not a regular file");
        return false;
    }
    if (status.st_uid != :: geteuid()) {
        set_error(error, "local identity file is not owned by the current user");
        return false;
    }
    if ((status.st_mode & kPermissionMask) != kRequiredMode) {
        set_error(error, "local identity file must have mode 0600");
        return false;
    }
    if (status.st_size != static_cast<off_t>(kSecretHexCharacters)
        && status.st_size != static_cast<off_t>(kSecretTextBytes)) {
        set_error(error, "local identity file has an unexpected size");
        return false;
    }
    return true;
}

[[nodiscard]] bool hex_value(const char character, Byte& value) noexcept
{
    if (character >= '0' && character <= '9') {
        value = static_cast<Byte>(character - '0');
        return true;
    }
    if (character >= 'a' && character <= 'f') {
        value = static_cast<Byte>(character - 'a' + 10);
        return true;
    }
    if (character >= 'A' && character <= 'F') {
        value = static_cast<Byte>(character - 'A' + 10);
        return true;
    }
    return false;
}

[[nodiscard]] bool parse_secret_text(const std::array<char, kSecretTextBytes>& text,
                                     std::size_t length,
                                     SigningSecret& secret,
                                     std::string& error) noexcept
{
    clear_secret(secret);
    if (length == kSecretTextBytes) {
        if (text[kSecretHexCharacters] != '\n') {
            set_error(error, "local identity file must contain hexadecimal text and an optional newline");
            return false;
        }
        length = kSecretHexCharacters;
    }
    if (length != kSecretHexCharacters) {
        set_error(error, "local identity file must contain exactly 64 hexadecimal characters");
        return false;
    }
    for (std::size_t index = 0U; index < kSecretBytes; ++index) {
        Byte high = 0U;
        Byte low = 0U;
        if (!hex_value(text[index * 2U], high) || !hex_value(text[index * 2U + 1U], low)) {
            clear_secret(secret);
            set_error(error, "local identity file contains a non-hexadecimal character");
            return false;
        }
        secret.bytes[index] = static_cast<Byte>((high << 4U) | low);
    }
    if (secret.bytes == std::array<Byte, kSecretBytes>{}) {
        clear_secret(secret);
        set_error(error, "local identity file contains an all-zero subseed");
        return false;
    }
    return true;
}

[[nodiscard]] bool read_secret_fd(int fd,
                                  SigningSecret& secret,
                                  std::string& error) noexcept
{
    std::array<char, kSecretTextBytes> text{};
    std::size_t length = 0U;
    while (length < text.size()) {
        const ssize_t count = ::read(fd, text.data() + length, text.size() - length);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            clear_text(text);
            set_error(error, "cannot read the local identity file");
            return false;
        }
        if (count == 0) {
            break;
        }
        length += static_cast<std::size_t>(count);
    }
    if (length == text.size()) {
        char extra = '\0';
        ssize_t count = 0;
        do {
            count = ::read(fd, &extra, 1U);
        } while (count < 0 && errno == EINTR);
        if (count != 0) {
            clear_text(text);
            set_error(error, "local identity file contains too much data");
            return false;
        }
    }
    const bool result = parse_secret_text(text, length, secret, error);
    clear_text(text);
    return result;
}

[[nodiscard]] bool write_all(int fd,
                             const char* data,
                             std::size_t size,
                             std::string& error) noexcept
{
    std::size_t offset = 0U;
    while (offset < size) {
        const ssize_t count = ::write(fd, data + offset, size - offset);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            set_error(error, "cannot write the local identity file");
            return false;
        }
        if (count == 0) {
            set_error(error, "local identity file write returned zero bytes");
            return false;
        }
        offset += static_cast<std::size_t>(count);
    }
    return true;
}

[[nodiscard]] bool fill_random(SigningSecret& secret, std::string& error) noexcept
{
#if defined(__linux__)
    clear_secret(secret);
    std::size_t offset = 0U;
    while (offset < secret.bytes.size()) {
        const ssize_t count = ::getrandom(secret.bytes.data() + offset,
                                          secret.bytes.size() - offset,
                                          0U);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            clear_secret(secret);
            set_error(error, "operating-system random generation failed");
            return false;
        }
        if (count == 0) {
            clear_secret(secret);
            set_error(error, "operating-system random generation returned zero bytes");
            return false;
        }
        offset += static_cast<std::size_t>(count);
    }
    if (secret.bytes == std::array<Byte, kSecretBytes>{}) {
        clear_secret(secret);
        set_error(error, "operating-system random generation returned an all-zero subseed");
        return false;
    }
    return true;
#else
    clear_secret(secret);
    set_error(error, "secure local identity generation requires Linux getrandom");
    return false;
#endif
}

[[nodiscard]] bool open_existing_secret(const std::filesystem::path& path,
                                        int flags,
                                        int& fd,
                                        struct stat& status,
                                        std::string& error) noexcept
{
    fd = ::open(path.c_str(), flags | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        set_error(error, "cannot open the local identity file");
        return false;
    }
    if (!validate_secret_file(fd, status, error)) {
        (void)::close(fd);
        fd = -1;
        return false;
    }
    return true;
}

[[nodiscard]] bool overwrite_fd_with_zeros(int fd, off_t size, std::string& error) noexcept
{
    std::array<Byte, 4096U> zeros{};
    off_t remaining = size;
    while (remaining > 0) {
        const std::size_t count = std::min<std::size_t>(zeros.size(),
                                                        static_cast<std::size_t>(remaining));
        if (!write_all(fd,
                       reinterpret_cast<const char*>(zeros.data()),
                       count,
                       error)) {
            return false;
        }
        remaining -= static_cast<off_t>(count);
    }
    if (::fsync(fd) != 0) {
        set_error(error, "cannot synchronize the erased local identity file");
        return false;
    }
    return true;
}

} // namespace

bool load_signing_subseed_file(const std::filesystem::path& path,
                               SigningSecret& secret,
                               std::string& error) noexcept
{
    clear_secret(secret);
    int fd = -1;
    struct stat status{};
    if (!open_existing_secret(path, O_RDONLY, fd, status, error)) {
        return false;
    }
    const bool result = read_secret_fd(fd, secret, error);
    (void)::close(fd);
    if (!result) {
        clear_secret(secret);
    }
    return result;
}

bool create_signing_subseed_file(const std::filesystem::path& path,
                                 SigningSecret& secret,
                                 const bool replace_existing,
                                 std::string& error) noexcept
{
    clear_secret(secret);
    struct stat existing{};
    if (::lstat(path.c_str(), &existing) == 0) {
        if (!replace_existing) {
            set_error(error, "local identity file already exists; explicit replacement is required");
            return false;
        }
        if (!erase_signing_subseed_file(path, error)) {
            return false;
        }
    } else if (errno != ENOENT) {
        set_error(error, "cannot inspect the requested local identity path");
        return false;
    }

    if (!fill_random(secret, error)) {
        return false;
    }

    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) {
        clear_secret(secret);
        set_error(error, "cannot create the local identity file");
        return false;
    }
    if (::fchmod(fd, kRequiredMode) != 0) {
        (void)::close(fd);
        (void)::unlink(path.c_str());
        clear_secret(secret);
        set_error(error, "cannot restrict the local identity file to mode 0600");
        return false;
    }

    constexpr char digits[] = "0123456789abcdef";
    std::array<char, kSecretTextBytes> encoded{};
    for (std::size_t index = 0U; index < secret.bytes.size(); ++index) {
        encoded[index * 2U] = digits[secret.bytes[index] >> 4U];
        encoded[index * 2U + 1U] = digits[secret.bytes[index] & 0x0FU];
    }
    encoded[kSecretHexCharacters] = '\n';
    bool result = write_all(fd, encoded.data(), encoded.size(), error);
    if (result && ::fsync(fd) != 0) {
        set_error(error, "cannot synchronize the local identity file");
        result = false;
    }
    const int close_result = ::close(fd);
    clear_text(encoded);
    if (close_result != 0 && result) {
        set_error(error, "cannot close the local identity file");
        result = false;
    }
    if (!result) {
        (void)::unlink(path.c_str());
        clear_secret(secret);
    }
    return result;
}

bool erase_signing_subseed_file(const std::filesystem::path& path,
                                std::string& error) noexcept
{
    int fd = -1;
    struct stat status{};
    if (!open_existing_secret(path, O_RDWR, fd, status, error)) {
        return false;
    }
    bool result = overwrite_fd_with_zeros(fd, status.st_size, error);
    const int close_result = ::close(fd);
    if (close_result != 0 && result) {
        set_error(error, "cannot close the erased local identity file");
        result = false;
    }
    if (result && ::unlink(path.c_str()) != 0) {
        set_error(error, "cannot unlink the erased local identity file");
        result = false;
    }
    return result;
}

} // namespace xdna::qubic
